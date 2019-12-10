/*
 * Copyright 2019 Hans-Kristian Arntzen for Valve Corporation
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

#pragma once

#include "SpvBuilder.h"
#include "cfg_structurizer.hpp"
#include <memory>
#include <vector>

namespace DXIL2SPIRV
{
struct CFGNode;
class CFGNodePool;

class SPIRVModule
{
public:
	SPIRVModule();
	~SPIRVModule();
	bool finalize_spirv(std::vector<uint32_t> &spirv);

	uint32_t allocate_id();
	uint32_t allocate_ids(uint32_t count);

	void emit_entry_point(spv::ExecutionModel model, const char *name);
	void emit_entry_point_function_body(CFGStructurizer &structurizer);
	void emit_leaf_function_body(spv::Function *func, CFGStructurizer &structurizer);

	spv::Builder &get_builder();
	spv::Instruction *get_entry_point();
	spv::Function *get_entry_function();

	void enable_shader_discard();
	spv::Id get_builtin_shader_input(spv::BuiltIn builtin);
	void register_builtin_shader_input(spv::Id id, spv::BuiltIn builtin);
	bool query_builtin_shader_input(spv::Id id, spv::BuiltIn *builtin) const;

	void register_builtin_shader_output(spv::Id id, spv::BuiltIn builtin);
	bool query_builtin_shader_output(spv::Id id, spv::BuiltIn *builtin) const;

	Operation *allocate_op();
	Operation *allocate_op(spv::Op op);
	Operation *allocate_op(spv::Op op, spv::Id id, spv::Id type_id);

private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};
} // namespace DXIL2SPIRV
