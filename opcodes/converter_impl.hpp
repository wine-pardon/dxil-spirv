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

#pragma once

#include "SpvBuilder.h"
#include "cfg_structurizer.hpp"
#include "dxil_converter.hpp"
#include "scratch_pool.hpp"

#include "GLSL.std.450.h"

#ifdef HAVE_LLVMBC
#include "function.hpp"
#include "instruction.hpp"
#include "module.hpp"
#include "value.hpp"
#include "metadata.hpp"
#else
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Instructions.h>
#endif

#include <stdlib.h>
#include <string.h>

namespace dxil_spv
{
enum class LocalRootSignatureType
{
	Constants,
	Descriptor,
	Table
};

struct LocalRootSignatureConstants
{
	uint32_t num_words;
};

struct LocalRootSignatureDescriptor
{
	ResourceClass type;
};

struct LocalRootSignatureTable
{
	ResourceClass type;
	uint32_t num_descriptors_in_range;
	uint32_t offset_in_heap;
};

struct LocalRootSignatureEntry
{
	LocalRootSignatureType type;
	uint32_t register_space;
	uint32_t register_index;
	union
	{
		LocalRootSignatureConstants constants;
		LocalRootSignatureDescriptor descriptor;
		LocalRootSignatureTable table;
	};
};

struct Converter::Impl
{
	Impl(LLVMBCParser &bitcode_parser_, SPIRVModule &module_)
	    : bitcode_parser(bitcode_parser_)
	    , spirv_module(module_)
	{
	}

	LLVMBCParser &bitcode_parser;
	SPIRVModule &spirv_module;

	struct BlockMeta
	{
		explicit BlockMeta(llvm::BasicBlock *bb_)
		    : bb(bb_)
		{
		}

		llvm::BasicBlock *bb;
		CFGNode *node = nullptr;
	};
	std::vector<std::unique_ptr<BlockMeta>> metas;
	std::unordered_map<llvm::BasicBlock *, BlockMeta *> bb_map;
	std::unordered_map<const llvm::Value *, spv::Id> value_map;

	ConvertedFunction convert_entry_point();
	CFGNode *convert_function(llvm::Function *func, CFGNodePool &pool);
	CFGNode *build_hull_main(llvm::Function *func, CFGNodePool &pool,
	                         std::vector<ConvertedFunction::LeafFunction> &leaves);
	spv::Id get_id_for_value(const llvm::Value *value, unsigned forced_integer_width = 0);
	spv::Id get_id_for_constant(const llvm::Constant *constant, unsigned forced_width);
	spv::Id get_id_for_undef(const llvm::UndefValue *undef);

	bool emit_stage_input_variables();
	bool emit_stage_output_variables();
	bool emit_patch_variables();
	bool emit_global_variables();
	bool emit_incoming_ray_payload();
	bool emit_hit_attribute();
	void emit_interpolation_decorations(spv::Id variable_id, DXIL::InterpolationMode mode);

	spv::ExecutionModel execution_model = spv::ExecutionModelMax;
	bool emit_execution_modes();
	bool emit_execution_modes_compute();
	bool emit_execution_modes_geometry();
	bool emit_execution_modes_hull();
	bool emit_execution_modes_domain();
	bool emit_execution_modes_pixel();
	bool emit_execution_modes_ray_tracing(spv::ExecutionModel model);

	bool analyze_instructions();
	bool analyze_instructions(const llvm::Function *function);
	struct UAVAccessTracking
	{
		bool has_read = false;
		bool has_written = false;
	};
	std::unordered_map<uint32_t, UAVAccessTracking> uav_access_tracking;
	std::unordered_map<const llvm::Value *, uint32_t> llvm_value_to_uav_resource_index_map;
	std::unordered_set<const llvm::Value *> llvm_values_using_update_counter;
	std::unordered_map<const llvm::Value *, uint32_t> llvm_values_to_payload_location;
	std::unordered_set<const llvm::Value *> llvm_values_potential_sparse_feedback;
	std::unordered_set<const llvm::Value *> llvm_value_is_sparse_feedback;
	std::unordered_map<const llvm::Value *, spv::Id> llvm_value_actual_type;
	uint32_t payload_location_counter = 0;

	struct ResourceMetaReference
	{
		DXIL::ResourceType type;
		unsigned meta_index;
		llvm::Value *offset;
	};
	std::unordered_map<const llvm::Value *, ResourceMetaReference> llvm_global_variable_to_resource_mapping;

	struct ExecutionModeMeta
	{
		unsigned stage_input_num_vertex = 0;
		unsigned stage_output_num_vertex = 0;
		unsigned gs_stream_active_mask = 0;
		llvm::Function *patch_constant_function = nullptr;
	} execution_mode_meta;

	static ShaderStage get_remapping_stage(spv::ExecutionModel model);

	bool emit_resources_global_mapping();
	bool emit_resources_global_mapping(DXIL::ResourceType type, const llvm::MDNode *node);
	bool emit_resources();
	bool emit_srvs(const llvm::MDNode *srvs);
	bool emit_uavs(const llvm::MDNode *uavs);
	bool emit_cbvs(const llvm::MDNode *cbvs);
	bool emit_samplers(const llvm::MDNode *samplers);
	bool emit_shader_record_buffer();
	void register_resource_meta_reference(const llvm::MDOperand &operand, DXIL::ResourceType type, unsigned index);
	void emit_root_constants(unsigned num_words);
	static void scan_resources(ResourceRemappingInterface *iface, const LLVMBCParser &parser);
	static bool scan_srvs(ResourceRemappingInterface *iface, const llvm::MDNode *srvs, ShaderStage stage);
	static bool scan_uavs(ResourceRemappingInterface *iface, const llvm::MDNode *uavs, ShaderStage stage);
	static bool scan_cbvs(ResourceRemappingInterface *iface, const llvm::MDNode *cbvs, ShaderStage stage);
	static bool scan_samplers(ResourceRemappingInterface *iface, const llvm::MDNode *samplers, ShaderStage stage);

	std::vector<LocalRootSignatureEntry> local_root_signature;
	int get_local_root_signature_entry(ResourceClass resource_class, uint32_t space, uint32_t binding) const;

	struct ResourceReference
	{
		spv::Id var_id = 0;
		uint32_t push_constant_member = 0;
		uint32_t base_offset = 0;
		unsigned stride = 0;
		bool bindless = false;
		bool base_resource_is_array = false;
		int local_root_signature_entry = -1;
	};

	std::vector<ResourceReference> srv_index_to_reference;
	std::vector<ResourceReference> sampler_index_to_reference;
	std::vector<ResourceReference> cbv_index_to_reference;
	std::vector<ResourceReference> uav_index_to_reference;
	std::vector<ResourceReference> uav_index_to_counter;
	std::vector<unsigned> cbv_push_constant_member;
	std::unordered_map<const llvm::Value *, spv::Id> handle_to_ptr_id;
	spv::Id root_constant_id = 0;
	unsigned root_constant_num_words = 0;
	unsigned patch_location_offset = 0;

	struct ResourceMeta
	{
		DXIL::ResourceKind kind;
		DXIL::ComponentType component_type;
		unsigned stride;
		spv::Id var_id;
		spv::StorageClass storage;
		bool non_uniform;

		spv::Id counter_var_id;
		bool counter_is_physical_pointer;
	};
	std::unordered_map<spv::Id, ResourceMeta> handle_to_resource_meta;
	std::unordered_map<spv::Id, spv::Id> id_to_type;
	std::unordered_map<const llvm::Value *, unsigned> handle_to_root_member_offset;
	std::unordered_map<const llvm::Value *, spv::StorageClass> handle_to_storage_class;

	spv::Id get_type_id(DXIL::ComponentType element_type, unsigned rows, unsigned cols, bool force_array = false);
	spv::Id get_type_id(const llvm::Type *type);
	spv::Id get_type_id(spv::Id id) const;

	struct ElementMeta
	{
		spv::Id id;
		DXIL::ComponentType component_type;
		unsigned rt_index;
	};

	struct ClipCullMeta
	{
		unsigned offset;
		unsigned row_stride;
		spv::BuiltIn builtin;
	};
	std::unordered_map<uint32_t, ElementMeta> input_elements_meta;
	std::unordered_map<uint32_t, ElementMeta> output_elements_meta;
	std::unordered_map<uint32_t, ElementMeta> patch_elements_meta;
	std::unordered_map<uint32_t, ClipCullMeta> input_clip_cull_meta;
	std::unordered_map<uint32_t, ClipCullMeta> output_clip_cull_meta;
	void emit_builtin_decoration(spv::Id id, DXIL::Semantic semantic, spv::StorageClass storage);

	bool emit_instruction(CFGNode *block, const llvm::Instruction &instruction);
	bool emit_phi_instruction(CFGNode *block, const llvm::PHINode &instruction);

	spv::Id build_sampled_image(spv::Id image_id, spv::Id sampler_id, bool comparison);
	spv::Id build_vector(spv::Id element_type, spv::Id *elements, unsigned count);
	spv::Id build_vector_type(spv::Id element_type, unsigned count);
	spv::Id build_constant_vector(spv::Id element_type, spv::Id *elements, unsigned count);
	spv::Id build_offset(spv::Id value, unsigned offset);
	void fixup_load_sign(DXIL::ComponentType component_type, unsigned components, const llvm::Value *value);
	void repack_sparse_feedback(DXIL::ComponentType component_type, unsigned components, const llvm::Value *value);
	spv::Id fixup_store_sign(DXIL::ComponentType component_type, unsigned components, spv::Id value);

	std::vector<Operation *> *current_block = nullptr;
	void add(Operation *op);
	Operation *allocate(spv::Op op);
	Operation *allocate(spv::Op op, const llvm::Value *value);
	Operation *allocate(spv::Op op, spv::Id type_id);
	Operation *allocate(spv::Op op, const llvm::Value *value, spv::Id type_id);
	Operation *allocate(spv::Op op, spv::Id id, spv::Id type_id);
	spv::Builder &builder();

	spv::Id glsl_std450_ext = 0;
	spv::Id cmpxchg_type = 0;
	spv::Id texture_sample_pos_lut_id = 0;
	spv::Id rasterizer_sample_count_id = 0;
	spv::Id physical_counter_type = 0;
	spv::Id shader_record_buffer_id = 0;
	std::vector<spv::Id> shader_record_buffer_types;

	ResourceRemappingInterface *resource_mapping_iface = nullptr;

	struct StructTypeEntry
	{
		spv::Id id;
		std::string name;
		std::vector<spv::Id> subtypes;
	};
	std::vector<StructTypeEntry> cached_struct_types;
	spv::Id get_struct_type(const std::vector<spv::Id> &type_ids, const char *name = nullptr);

	void set_option(const OptionBase &cap);
	struct
	{
		bool shader_demote = false;
		bool dual_source_blending = false;
		unsigned rasterizer_sample_count = 0;
		bool rasterizer_sample_count_spec_constant = true;
		std::vector<unsigned> output_swizzles;

		unsigned inline_ubo_descriptor_set = 0;
		unsigned inline_ubo_descriptor_binding = 0;
		bool inline_ubo_enable = false;
		bool bindless_cbv_ssbo_emulation = false;
		bool physical_storage_buffer = false;

		unsigned sbt_descriptor_size_srv_uav_cbv_log2 = 0;
		unsigned sbt_descriptor_size_sampler_log2 = 0;
	} options;

	spv::Id create_bindless_heap_variable(DXIL::ResourceType type, DXIL::ComponentType component,
	                                      DXIL::ResourceKind kind, uint32_t desc_set, uint32_t binding,
	                                      spv::ImageFormat format = spv::ImageFormatUnknown, bool has_uav_read = false,
	                                      bool has_uav_written = false, bool uav_coherent = false,
	                                      bool counters = false);

	struct BindlessResource
	{
		DXIL::ResourceType type;
		DXIL::ComponentType component;
		DXIL::ResourceKind kind;
		spv::ImageFormat format;
		bool uav_read = false;
		bool uav_written = false;
		bool uav_coherent = false;
		bool counters = false;
		uint32_t desc_set;
		uint32_t binding;
		uint32_t var_id;
	};
	std::vector<BindlessResource> bindless_resources;

	struct CombinedImageSampler
	{
		spv::Id image_id;
		spv::Id sampler_id;
		spv::Id combined_id;
		bool non_uniform;
	};
	std::vector<CombinedImageSampler> combined_image_sampler_cache;
};
} // namespace dxil_spv
