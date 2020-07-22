/*
 * Copyright 2019-2020 Hans-Kristian Arntzen for Valve Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "node.hpp"
#include "logging.hpp"

#include <algorithm>
#include <assert.h>

namespace dxil_spv
{
void CFGNode::add_unique_pred(CFGNode *node)
{
	auto itr = std::find(pred.begin(), pred.end(), node);
	if (itr == pred.end())
		pred.push_back(node);
}

void CFGNode::add_unique_header(CFGNode *node)
{
	auto itr = std::find(headers.begin(), headers.end(), node);
	if (itr == headers.end())
		headers.push_back(node);
}

void CFGNode::add_branch(CFGNode *to)
{
	add_unique_succ(to);
	to->add_unique_pred(this);
}

void CFGNode::add_unique_succ(CFGNode *node)
{
	auto itr = std::find(succ.begin(), succ.end(), node);
	if (itr == succ.end())
		succ.push_back(node);
}

unsigned CFGNode::num_forward_preds() const
{
	return unsigned(pred.size());
}

bool CFGNode::has_pred_back_edges() const
{
	return pred_back_edge != nullptr;
}

bool CFGNode::dominates(const CFGNode *other) const
{
	// Follow immediate dominator graph. Either we end up at this, or entry block.
	while (this != other)
	{
		// Entry block case.
		if (other->pred.empty())
			break;

		assert(other->immediate_dominator);
		assert(other != other->immediate_dominator);
		other = other->immediate_dominator;
	}

	return this == other;
}

bool CFGNode::can_loop_merge_to(const CFGNode *other) const
{
	if (!dominates(other))
		return false;

	auto *c = pred_back_edge;

	if (c && !c->succ.empty())
	{
		for (auto *cs : c->succ)
			if (cs->post_dominates(other))
				return true;

		// If the continue block branches to something which is not the loop header,
		// it must be the merge block we're after, i.e., it must be a clean break (or we are kind of screwed).
		// Detect a "fake" merge branch here.
		// E.g., we have a fake merge branch if an escaping edge is branching to one block beyond the real merge block.
		// This can happen after split_merge_scopes() transform where inner loop
		// tries to break out of multiple loops and multiple selection scopes at the same time.
		// We can still dominate this escape target, but it's still an escape which must be resolved some other way with ladders.
		if (std::find(c->succ.begin(), c->succ.end(), other) == c->succ.end())
			return false;
	}

	return true;
}

bool CFGNode::can_backtrace_to(const CFGNode *parent) const
{
	for (auto *p : pred)
		if (p == parent || p->can_backtrace_to(parent))
			return true;

	return false;
}

const CFGNode *CFGNode::get_innermost_loop_header_for(const CFGNode *other) const
{
	while (this != other)
	{
		// Entry block case.
		if (other->pred.empty())
			break;

		// Found a loop header. This better be the one.
		if (other->pred_back_edge)
			break;

		assert(other->immediate_dominator);
		other = other->immediate_dominator;
	}

	return other;
}

bool CFGNode::is_innermost_loop_header_for(const CFGNode *other) const
{
	return this == get_innermost_loop_header_for(other);
}

bool CFGNode::branchless_path_to(const CFGNode *to) const
{
	const auto *node = this;
	while (node != to)
	{
		if (node->succ.size() != 1 || node->succ_back_edge)
			return false;
		node = node->succ.front();
	}

	return true;
}

bool CFGNode::post_dominates(const CFGNode *start_node) const
{
	// Crude algorithm, try to traverse from start_node, and if we can find an exit without entering this,
	// we do not post-dominate.
	// Creating a post-dominator tree might be viable?

	// Terminated at this.
	if (start_node == this)
		return true;

	// Found exit.
	if (start_node->succ.empty())
		return false;

	// If post-visit order is lower, post-dominance is impossible.
	// As we traverse, post visit order will monotonically decrease.
	if (start_node->post_visit_order < post_visit_order)
		return false;

	for (auto *node : start_node->succ)
		if (!post_dominates(node))
			return false;

	return true;
}

bool CFGNode::dominates_all_reachable_exits(const CFGNode &header) const
{
	if (succ_back_edge)
		return false;

	for (auto *node : succ)
		if (!header.dominates(node) || !node->dominates_all_reachable_exits(header))
			return false;

	return true;
}

bool CFGNode::dominates_all_reachable_exits() const
{
	return dominates_all_reachable_exits(*this);
}
bool CFGNode::exists_path_in_cfg_without_intermediate_node(const CFGNode *end_block, const CFGNode *stop_block) const
{
	bool found_path = false;
	std::unordered_set<const CFGNode *> visited;
	walk_cfg_from([&](const CFGNode *node) -> bool {
		if (found_path)
			return false;
		if (visited.count(node))
			return false;
		visited.insert(node);

		if (node == end_block)
			found_path = true;

		return node != stop_block;
	});
	return found_path;
}

CFGNode *CFGNode::find_common_dominator(CFGNode *a, CFGNode *b)
{
	assert(a);
	assert(b);

	while (a != b)
	{
		if (!a->immediate_dominator)
		{
			for (auto *p : a->pred)
				p->recompute_immediate_dominator();
			a->recompute_immediate_dominator();
		}

		if (!b->immediate_dominator)
		{
			for (auto *p : b->pred)
				p->recompute_immediate_dominator();
			b->recompute_immediate_dominator();
		}

		if (a->post_visit_order < b->post_visit_order)
		{
			// Awkward case which can happen when nodes are unreachable in the CFG.
			// Can occur with the dummy blocks we create.
			if (a == a->immediate_dominator)
				return b;
			a = a->immediate_dominator;
		}
		else
		{
			// Awkward case which can happen when nodes are unreachable in the CFG.
			// Can occur with the dummy blocks we create.
			if (b == b->immediate_dominator)
				return a;

			b = b->immediate_dominator;
		}
	}
	return const_cast<CFGNode *>(a);
}

CFGNode *CFGNode::get_immediate_dominator_loop_header()
{
	assert(immediate_dominator);
	auto *node = this;
	while (!node->pred_back_edge)
	{
		if (node->pred.empty())
			return nullptr;

		assert(node->immediate_dominator);
		node = node->immediate_dominator;
	}

	return node;
}

void CFGNode::retarget_branch(CFGNode *to_prev, CFGNode *to_next)
{
	//LOGI("Retargeting branch for %s: %s -> %s\n", name.c_str(), to_prev->name.c_str(), to_next->name.c_str());
	assert(std::find(succ.begin(), succ.end(), to_prev) != succ.end());
	assert(std::find(to_prev->pred.begin(), to_prev->pred.end(), this) != to_prev->pred.end());
	assert(std::find(succ.begin(), succ.end(), to_next) == succ.end());
	assert(std::find(to_next->pred.begin(), to_next->pred.end(), this) == to_next->pred.end());

	to_prev->pred.erase(std::find(to_prev->pred.begin(), to_prev->pred.end(), this));

	// Modify succ in place so we don't invalidate iterator in traverse_dominated_blocks_and_rewrite_branch.
	*std::find(succ.begin(), succ.end(), to_prev) = to_next;
	add_branch(to_next);

	// Branch targets have changed, so recompute immediate dominators.
	if (to_prev->post_visit_order > to_next->post_visit_order)
	{
		to_prev->recompute_immediate_dominator();
		to_next->recompute_immediate_dominator();
	}
	else
	{
		to_next->recompute_immediate_dominator();
		to_prev->recompute_immediate_dominator();
	}

	if (ir.terminator.direct_block == to_prev)
		ir.terminator.direct_block = to_next;
	if (ir.terminator.true_block == to_prev)
		ir.terminator.true_block = to_next;
	if (ir.terminator.false_block == to_prev)
		ir.terminator.false_block = to_next;
	if (ir.terminator.default_node == to_prev)
		ir.terminator.default_node = to_next;
	for (auto &c : ir.terminator.cases)
		if (c.node == to_prev)
			c.node = to_next;
}

void CFGNode::traverse_dominated_blocks_and_rewrite_branch(CFGNode *from, CFGNode *to)
{
	traverse_dominated_blocks_and_rewrite_branch(*this, from, to, [](const CFGNode *) { return true; });
}

void CFGNode::retarget_pred_from(CFGNode *old_succ)
{
	for (auto *p : pred)
	{
		for (auto &s : p->succ)
			if (s == old_succ)
				s = this;

		auto &p_term = p->ir.terminator;
		if (p_term.direct_block == old_succ)
			p_term.direct_block = this;
		if (p_term.true_block == old_succ)
			p_term.true_block = this;
		if (p_term.false_block == old_succ)
			p_term.false_block = this;
		if (p_term.default_node == old_succ)
			p_term.default_node = this;
		for (auto &c : p_term.cases)
			if (c.node == old_succ)
				c.node = this;
	}

	// Do not swap back edges.
}

void CFGNode::recompute_immediate_dominator()
{
	if (pred.empty())
	{
		// For entry block only.
		immediate_dominator = this;
	}
	else
	{
		immediate_dominator = nullptr;

		for (auto *edge : pred)
		{
			if (immediate_dominator)
				immediate_dominator = CFGNode::find_common_dominator(immediate_dominator, edge);
			else
				immediate_dominator = edge;
		}
	}
}

CFGNode *CFGNode::get_outer_selection_dominator()
{
	assert(immediate_dominator);
	auto *node = immediate_dominator;

	// We need to find an immediate dominator which we do not post-dominate.
	// That first idom is considered the outer selection header.
	while (node->ir.terminator.type != Terminator::Type::Switch && post_dominates(node))
	{
		if (node->pred.empty())
			break;

		// Skip from merge block to header.
		while (std::find(node->headers.begin(), node->headers.end(), node->immediate_dominator) != node->headers.end())
			node = node->immediate_dominator;

		if (post_dominates(node))
		{
			assert(node->immediate_dominator);
			node = node->immediate_dominator;
		}
	}

	return node;
}

CFGNode *CFGNode::get_outer_header_dominator()
{
	assert(immediate_dominator);
	auto *node = immediate_dominator;
	while (node->succ.size() == 1 && node->ir.terminator.type != Terminator::Type::Switch && !node->pred_back_edge)
	{
		if (node->pred.empty())
			break;

		assert(node->immediate_dominator);
		node = node->immediate_dominator;
	}

	return node;
}

} // namespace dxil_spv
