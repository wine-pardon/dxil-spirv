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

#include "opcodes_llvm_builtins.hpp"
#include "converter_impl.hpp"
#include "logging.hpp"

namespace dxil_spv
{
bool emit_binary_instruction(Converter::Impl &impl, const llvm::BinaryOperator *instruction)
{
	spv::Op opcode;
	switch (instruction->getOpcode())
	{
	case llvm::BinaryOperator::BinaryOps::FAdd:
		opcode = spv::OpFAdd;
		break;

	case llvm::BinaryOperator::BinaryOps::FSub:
		opcode = spv::OpFSub;
		break;

	case llvm::BinaryOperator::BinaryOps::FMul:
		opcode = spv::OpFMul;
		break;

	case llvm::BinaryOperator::BinaryOps::FDiv:
		opcode = spv::OpFDiv;
		break;

	case llvm::BinaryOperator::BinaryOps::Add:
		opcode = spv::OpIAdd;
		break;

	case llvm::BinaryOperator::BinaryOps::Sub:
		opcode = spv::OpISub;
		break;

	case llvm::BinaryOperator::BinaryOps::Mul:
		opcode = spv::OpIMul;
		break;

	case llvm::BinaryOperator::BinaryOps::SDiv:
		opcode = spv::OpSDiv;
		break;

	case llvm::BinaryOperator::BinaryOps::UDiv:
		opcode = spv::OpUDiv;
		break;

	case llvm::BinaryOperator::BinaryOps::Shl:
		opcode = spv::OpShiftLeftLogical;
		break;

	case llvm::BinaryOperator::BinaryOps::LShr:
		opcode = spv::OpShiftRightLogical;
		break;

	case llvm::BinaryOperator::BinaryOps::AShr:
		opcode = spv::OpShiftRightArithmetic;
		break;

	case llvm::BinaryOperator::BinaryOps::SRem:
		opcode = spv::OpSRem;
		break;

	case llvm::BinaryOperator::BinaryOps::FRem:
		opcode = spv::OpFRem;
		break;

	case llvm::BinaryOperator::BinaryOps::URem:
		// Is this correct? There is no URem.
		opcode = spv::OpUMod;
		break;

	case llvm::BinaryOperator::BinaryOps::Xor:
		if (instruction->getType()->getIntegerBitWidth() == 1)
		{
			// Logical not in LLVM IR is encoded as XOR i1 against true.
			spv::Id not_id = 0;
			if (const auto *c = llvm::dyn_cast<llvm::ConstantInt>(instruction->getOperand(0)))
			{
				if (c->getUniqueInteger().getZExtValue() != 0)
					not_id = impl.get_id_for_value(instruction->getOperand(1));
			}
			else if (const auto *c = llvm::dyn_cast<llvm::ConstantInt>(instruction->getOperand(1)))
			{
				if (c->getUniqueInteger().getZExtValue() != 0)
					not_id = impl.get_id_for_value(instruction->getOperand(0));
			}

			if (not_id)
			{
				opcode = spv::OpLogicalNot;
				auto *op = impl.allocate(opcode, instruction);
				op->add_id(not_id);
				impl.add(op);
				return true;
			}

			opcode = spv::OpLogicalNotEqual;
		}
		else
			opcode = spv::OpBitwiseXor;
		break;

	case llvm::BinaryOperator::BinaryOps::And:
		if (instruction->getType()->getIntegerBitWidth() == 1)
			opcode = spv::OpLogicalAnd;
		else
			opcode = spv::OpBitwiseAnd;
		break;

	case llvm::BinaryOperator::BinaryOps::Or:
		if (instruction->getType()->getIntegerBitWidth() == 1)
			opcode = spv::OpLogicalOr;
		else
			opcode = spv::OpBitwiseOr;
		break;

	default:
		LOGE("Unknown binary operator.\n");
		return false;
	}

	Operation *op = impl.allocate(opcode, instruction);

	uint32_t id0 = impl.get_id_for_value(instruction->getOperand(0));
	uint32_t id1 = impl.get_id_for_value(instruction->getOperand(1));
	op->add_ids({ id0, id1 });

	impl.add(op);
	return true;
}

bool emit_unary_instruction(Converter::Impl &impl, const llvm::UnaryOperator *instruction)
{
	spv::Op opcode;

	switch (instruction->getOpcode())
	{
	case llvm::UnaryOperator::UnaryOps::FNeg:
		opcode = spv::OpFNegate;
		break;

	default:
		LOGE("Unknown unary operator.\n");
		return false;
	}

	Operation *op = impl.allocate(opcode, instruction);
	op->add_id(impl.get_id_for_value(instruction->getOperand(0)));

	impl.add(op);
	return true;
}

bool emit_boolean_convert_instruction(Converter::Impl &impl, const llvm::CastInst *instruction, bool is_signed)
{
	auto &builder = impl.builder();
	spv::Id const_0;
	spv::Id const_1;

	switch (instruction->getType()->getTypeID())
	{
	case llvm::Type::TypeID::HalfTyID:
		const_0 = builder.makeFloat16Constant(0);
		const_1 = builder.makeFloat16Constant(0x3c00u | (is_signed ? 0x8000u : 0u));
		break;

	case llvm::Type::TypeID::FloatTyID:
		const_0 = builder.makeFloatConstant(0.0f);
		const_1 = builder.makeFloatConstant(is_signed ? -1.0f : 1.0f);
		break;

	case llvm::Type::TypeID::DoubleTyID:
		const_0 = builder.makeDoubleConstant(0.0);
		const_1 = builder.makeDoubleConstant(is_signed ? -1.0 : 1.0);
		break;

	case llvm::Type::TypeID::IntegerTyID:
		switch (instruction->getType()->getIntegerBitWidth())
		{
		case 16:
			const_0 = builder.makeUint16Constant(0);
			const_1 = builder.makeUint16Constant(is_signed ? 0xffff : 1u);
			break;

		case 32:
			const_0 = builder.makeUintConstant(0);
			const_1 = builder.makeUintConstant(is_signed ? 0xffff : 1u);
			break;

		default:
			return false;
		}
		break;

	default:
		return false;
	}

	Operation *op = impl.allocate(spv::OpSelect, instruction);
	op->add_id(impl.get_id_for_value(instruction->getOperand(0)));
	op->add_ids({ const_1, const_0 });
	impl.add(op);
	return true;
}

bool emit_cast_instruction(Converter::Impl &impl, const llvm::CastInst *instruction)
{
	spv::Op opcode;

	switch (instruction->getOpcode())
	{
	case llvm::CastInst::CastOps::BitCast:
		opcode = spv::OpBitcast;
		break;

	case llvm::CastInst::CastOps::SExt:
		if (instruction->getOperand(0)->getType()->getIntegerBitWidth() == 1)
			return emit_boolean_convert_instruction(impl, instruction, true);
		opcode = spv::OpSConvert;
		break;

	case llvm::CastInst::CastOps::ZExt:
		if (instruction->getOperand(0)->getType()->getIntegerBitWidth() == 1)
			return emit_boolean_convert_instruction(impl, instruction, false);
		opcode = spv::OpUConvert;
		break;

	case llvm::CastInst::CastOps::Trunc:
		opcode = spv::OpUConvert;
		break;

	case llvm::CastInst::CastOps::FPTrunc:
	case llvm::CastInst::CastOps::FPExt:
		opcode = spv::OpFConvert;
		break;

	case llvm::CastInst::CastOps::FPToUI:
		opcode = spv::OpConvertFToU;
		break;

	case llvm::CastInst::CastOps::FPToSI:
		opcode = spv::OpConvertFToS;
		break;

	case llvm::CastInst::CastOps::SIToFP:
		if (instruction->getOperand(0)->getType()->getIntegerBitWidth() == 1)
			return emit_boolean_convert_instruction(impl, instruction, true);
		opcode = spv::OpConvertSToF;
		break;

	case llvm::CastInst::CastOps::UIToFP:
		if (instruction->getOperand(0)->getType()->getIntegerBitWidth() == 1)
			return emit_boolean_convert_instruction(impl, instruction, false);
		opcode = spv::OpConvertUToF;
		break;

	default:
		LOGE("Unknown cast operation.\n");
		return false;
	}

	if (instruction->getType()->getTypeID() == llvm::Type::TypeID::PointerTyID)
	{
		// I have observed this code in the wild
		// %blah = bitcast float* %foo to i32*
		// on function local memory.
		// I have no idea if this is legal DXIL.
		// Fake this by copying the object instead without any cast, and resolve the bitcast in OpLoad/OpStore instead.
		auto *pointer_type = llvm::cast<llvm::PointerType>(instruction->getOperand(0)->getType());
		auto *pointee_type = pointer_type->getPointerElementType();
		auto addr_space = static_cast<DXIL::AddressSpace>(pointer_type->getAddressSpace());
		spv::Id value_type = impl.get_type_id(pointee_type);

		spv::Id type_id = impl.builder().makePointer(addr_space == DXIL::AddressSpace::GroupShared ?
		                                             spv::StorageClassWorkgroup :
		                                             spv::StorageClassFunction,
		                                             value_type);
		Operation *op = impl.allocate(spv::OpCopyObject, instruction, type_id);
		op->add_id(impl.get_id_for_value(instruction->getOperand(0)));

		// Remember that we will need to bitcast on load or store to the real underlying type.
		impl.llvm_value_actual_type[instruction] = value_type;
		impl.add(op);
	}
	else
	{
		Operation *op = impl.allocate(opcode, instruction);
		op->add_id(impl.get_id_for_value(instruction->getOperand(0)));
		impl.add(op);
	}
	return true;
}

static bool emit_getelementptr_resource(Converter::Impl &impl, const llvm::GetElementPtrInst *instruction,
                                        const Converter::Impl::ResourceMetaReference &meta)
{
	auto *elem_index = instruction->getOperand(1);

	// This one must be constant 0, ignore it.
	if (!llvm::isa<llvm::ConstantInt>(elem_index))
	{
		LOGE("First GetElementPtr operand is not constant 0.\n");
		return false;
	}

	if (instruction->getNumOperands() != 3)
	{
		LOGE("Number of operands to getelementptr for a resource handle is unexpected.\n");
		return false;
	}

	auto indexed_meta = meta;
	indexed_meta.offset = instruction->getOperand(2);
	impl.llvm_global_variable_to_resource_mapping[instruction] = indexed_meta;
	return true;
}

bool emit_getelementptr_instruction(Converter::Impl &impl, const llvm::GetElementPtrInst *instruction)
{
	// This is actually the same as PtrAccessChain, but we would need to use variable pointers to support that properly.
	// For now, just assert that the first index is constant 0, in which case PtrAccessChain == AccessChain.

	auto global_itr = impl.llvm_global_variable_to_resource_mapping.find(instruction->getOperand(0));
	if (global_itr != impl.llvm_global_variable_to_resource_mapping.end())
		return emit_getelementptr_resource(impl, instruction, global_itr->second);

	auto &builder = impl.builder();
	spv::Id ptr_id = impl.get_id_for_value(instruction->getOperand(0));
	spv::Id type_id = impl.get_type_id(instruction->getType()->getPointerElementType());

	auto storage_class_itr = impl.handle_to_storage_class.find(instruction->getOperand(0));
	spv::StorageClass storage_class;
	if (storage_class_itr != impl.handle_to_storage_class.end())
		storage_class = storage_class_itr->second;
	else
		storage_class = builder.getStorageClass(ptr_id);

	type_id = builder.makePointer(storage_class, type_id);

	Operation *op = impl.allocate(instruction->isInBounds() ? spv::OpInBoundsAccessChain : spv::OpAccessChain,
	                              instruction, type_id);

	op->add_id(ptr_id);

	auto *elem_index = instruction->getOperand(1);

	// This one must be constant 0, ignore it.
	if (!llvm::isa<llvm::ConstantInt>(elem_index))
	{
		LOGE("First GetElementPtr operand is not constant 0.\n");
		return false;
	}

	if (llvm::cast<llvm::ConstantInt>(elem_index)->getUniqueInteger().getZExtValue() != 0)
	{
		LOGE("First GetElementPtr operand is not constant 0.\n");
		return false;
	}

	unsigned num_operands = instruction->getNumOperands();
	for (uint32_t i = 2; i < num_operands; i++)
		op->add_id(impl.get_id_for_value(instruction->getOperand(i)));

	impl.handle_to_storage_class[instruction] = storage_class;
	impl.add(op);
	return true;
}

bool emit_load_instruction(Converter::Impl &impl, const llvm::LoadInst *instruction)
{
	auto itr = impl.llvm_global_variable_to_resource_mapping.find(instruction->getPointerOperand());
	auto type_itr = impl.llvm_value_actual_type.find(instruction->getPointerOperand());

	// If we are trying to load a resource in RT, this does not translate in SPIR-V, defer this to createHandleForLib.
	if (itr != impl.llvm_global_variable_to_resource_mapping.end())
	{
		impl.llvm_global_variable_to_resource_mapping[instruction] = itr->second;
	}
	else if (type_itr != impl.llvm_value_actual_type.end())
	{
		Operation *load_op = impl.allocate(spv::OpLoad, type_itr->second);
		load_op->add_id(impl.get_id_for_value(instruction->getPointerOperand()));
		impl.add(load_op);

		Operation *cast_op = impl.allocate(spv::OpBitcast, instruction);
		cast_op->add_id(load_op->id);
		impl.add(cast_op);
	}
	else
	{
		Operation *op = impl.allocate(spv::OpLoad, instruction);
		op->add_id(impl.get_id_for_value(instruction->getPointerOperand()));
		impl.add(op);
	}
	return true;
}

bool emit_store_instruction(Converter::Impl &impl, const llvm::StoreInst *instruction)
{
	Operation *op = impl.allocate(spv::OpStore);

	auto itr = impl.llvm_value_actual_type.find(instruction->getOperand(1));
	if (itr != impl.llvm_value_actual_type.end())
	{
		Operation *cast_op = impl.allocate(spv::OpBitcast, itr->second);
		cast_op->add_id(impl.get_id_for_value(instruction->getOperand(0)));
		impl.add(cast_op);

		op->add_ids(
			{ impl.get_id_for_value(instruction->getOperand(1)), cast_op->id });
	}
	else
	{
		op->add_ids(
		    { impl.get_id_for_value(instruction->getOperand(1)), impl.get_id_for_value(instruction->getOperand(0)) });
	}

	impl.add(op);
	return true;
}

bool emit_compare_instruction(Converter::Impl &impl, const llvm::CmpInst *instruction)
{
	spv::Op opcode;
	switch (instruction->getPredicate())
	{
	case llvm::CmpInst::Predicate::FCMP_OEQ:
		opcode = spv::OpFOrdEqual;
		break;

	case llvm::CmpInst::Predicate::FCMP_UEQ:
		opcode = spv::OpFUnordEqual;
		break;

	case llvm::CmpInst::Predicate::FCMP_OGT:
		opcode = spv::OpFOrdGreaterThan;
		break;

	case llvm::CmpInst::Predicate::FCMP_UGT:
		opcode = spv::OpFUnordGreaterThan;
		break;

	case llvm::CmpInst::Predicate::FCMP_OGE:
		opcode = spv::OpFOrdGreaterThanEqual;
		break;

	case llvm::CmpInst::Predicate::FCMP_UGE:
		opcode = spv::OpFUnordGreaterThanEqual;
		break;

	case llvm::CmpInst::Predicate::FCMP_OLT:
		opcode = spv::OpFOrdLessThan;
		break;

	case llvm::CmpInst::Predicate::FCMP_ULT:
		opcode = spv::OpFUnordLessThan;
		break;

	case llvm::CmpInst::Predicate::FCMP_OLE:
		opcode = spv::OpFOrdLessThanEqual;
		break;

	case llvm::CmpInst::Predicate::FCMP_ULE:
		opcode = spv::OpFUnordLessThanEqual;
		break;

	case llvm::CmpInst::Predicate::FCMP_ONE:
		opcode = spv::OpFOrdNotEqual;
		break;

	case llvm::CmpInst::Predicate::FCMP_UNE:
		opcode = spv::OpFUnordNotEqual;
		break;

	case llvm::CmpInst::Predicate::FCMP_FALSE:
	{
		// Why on earth is this a thing ...
		impl.value_map[instruction] = impl.builder().makeBoolConstant(false);
		return true;
	}

	case llvm::CmpInst::Predicate::FCMP_TRUE:
	{
		// Why on earth is this a thing ...
		impl.value_map[instruction] = impl.builder().makeBoolConstant(true);
		return true;
	}

	case llvm::CmpInst::Predicate::ICMP_EQ:
		opcode = spv::OpIEqual;
		break;

	case llvm::CmpInst::Predicate::ICMP_NE:
		opcode = spv::OpINotEqual;
		break;

	case llvm::CmpInst::Predicate::ICMP_SLT:
		opcode = spv::OpSLessThan;
		break;

	case llvm::CmpInst::Predicate::ICMP_SLE:
		opcode = spv::OpSLessThanEqual;
		break;

	case llvm::CmpInst::Predicate::ICMP_SGT:
		opcode = spv::OpSGreaterThan;
		break;

	case llvm::CmpInst::Predicate::ICMP_SGE:
		opcode = spv::OpSGreaterThanEqual;
		break;

	case llvm::CmpInst::Predicate::ICMP_ULT:
		opcode = spv::OpULessThan;
		break;

	case llvm::CmpInst::Predicate::ICMP_ULE:
		opcode = spv::OpULessThanEqual;
		break;

	case llvm::CmpInst::Predicate::ICMP_UGT:
		opcode = spv::OpUGreaterThan;
		break;

	case llvm::CmpInst::Predicate::ICMP_UGE:
		opcode = spv::OpUGreaterThanEqual;
		break;

	default:
		LOGE("Unknown CmpInst predicate.\n");
		return false;
	}

	Operation *op = impl.allocate(opcode, instruction);
	uint32_t id0 = impl.get_id_for_value(instruction->getOperand(0));
	uint32_t id1 = impl.get_id_for_value(instruction->getOperand(1));
	op->add_ids({ id0, id1 });

	impl.add(op);
	return true;
}

bool emit_extract_value_instruction(Converter::Impl &impl, const llvm::ExtractValueInst *instruction)
{
	Operation *op = impl.allocate(spv::OpCompositeExtract, instruction);

	op->add_id(impl.get_id_for_value(instruction->getAggregateOperand()));
	for (unsigned i = 0; i < instruction->getNumIndices(); i++)
		op->add_literal(instruction->getIndices()[i]);

	impl.add(op);
	return true;
}

bool emit_alloca_instruction(Converter::Impl &impl, const llvm::AllocaInst *instruction)
{
	spv::Id pointee_type_id = impl.get_type_id(instruction->getType()->getPointerElementType());

	// DXC seems to allocate arrays on stack as 1 element of array type rather than N elements of basic non-array type.
	// Should be possible to support both schemes if desirable, but this will do.
	if (!llvm::isa<llvm::ConstantInt>(instruction->getArraySize()))
	{
		LOGE("Array size for alloca must be constant int.\n");
		return false;
	}

	if (llvm::cast<llvm::ConstantInt>(instruction->getArraySize())->getUniqueInteger().getZExtValue() != 1)
	{
		LOGE("Alloca array size must be constant 1.\n");
		return false;
	}

	auto address_space = static_cast<DXIL::AddressSpace>(instruction->getType()->getAddressSpace());
	if (address_space != DXIL::AddressSpace::Thread)
		return false;

	auto payload_itr = impl.llvm_values_to_payload_location.find(instruction);
	if (payload_itr != impl.llvm_values_to_payload_location.end())
	{
		spv::Id var_id = impl.builder().createVariable(spv::StorageClassRayPayloadKHR, pointee_type_id);
		impl.handle_to_storage_class[instruction] = spv::StorageClassRayPayloadKHR;
		impl.value_map[instruction] = var_id;
		impl.builder().addDecoration(var_id, spv::DecorationLocation, payload_itr->second);
	}
	else
	{
		spv::Id var_id = impl.builder().createVariable(spv::StorageClassFunction, pointee_type_id);
		impl.value_map[instruction] = var_id;
	}
	return true;
}

bool emit_select_instruction(Converter::Impl &impl, const llvm::SelectInst *instruction)
{
	Operation *op = impl.allocate(spv::OpSelect, instruction);

	op->add_ids({
	    impl.get_id_for_value(instruction->getOperand(0)),
	    impl.get_id_for_value(instruction->getOperand(1)),
	    impl.get_id_for_value(instruction->getOperand(2)),
	});

	impl.add(op);
	return true;
}

bool emit_cmpxchg_instruction(Converter::Impl &impl, const llvm::AtomicCmpXchgInst *instruction)
{
	auto &builder = impl.builder();

	Operation *atomic_op = impl.allocate(spv::OpAtomicCompareExchange, builder.makeUintType(32));
	atomic_op->add_ids({ impl.get_id_for_value(instruction->getPointerOperand()),
	                     builder.makeUintConstant(spv::ScopeWorkgroup),
	                     builder.makeUintConstant(0), // Relaxed
	                     builder.makeUintConstant(0), // Relaxed
	                     impl.get_id_for_value(instruction->getNewValOperand()),
	                     impl.get_id_for_value(instruction->getCompareOperand()) });

	impl.add(atomic_op);

	Operation *cmp_op = impl.allocate(spv::OpIEqual, builder.makeBoolType());
	cmp_op->add_ids({ atomic_op->id, impl.get_id_for_value(instruction->getCompareOperand()) });
	impl.add(cmp_op);

	if (!impl.cmpxchg_type)
		impl.cmpxchg_type =
		    impl.get_struct_type({ builder.makeUintType(32), builder.makeBoolType() }, "CmpXchgResult");

	Operation *op = impl.allocate(spv::OpCompositeConstruct, instruction, impl.cmpxchg_type);
	op->add_ids({ atomic_op->id, cmp_op->id });
	impl.add(op);
	return true;
}

bool emit_atomicrmw_instruction(Converter::Impl &impl, const llvm::AtomicRMWInst *instruction)
{
	auto &builder = impl.builder();
	spv::Op opcode;
	switch (instruction->getOperation())
	{
	case llvm::AtomicRMWInst::BinOp::Add:
		opcode = spv::OpAtomicIAdd;
		break;

	case llvm::AtomicRMWInst::BinOp::Sub:
		opcode = spv::OpAtomicISub;
		break;

	case llvm::AtomicRMWInst::BinOp::And:
		opcode = spv::OpAtomicAnd;
		break;

	case llvm::AtomicRMWInst::BinOp::Or:
		opcode = spv::OpAtomicOr;
		break;

	case llvm::AtomicRMWInst::BinOp::Xor:
		opcode = spv::OpAtomicXor;
		break;

	case llvm::AtomicRMWInst::BinOp::UMax:
		opcode = spv::OpAtomicUMax;
		break;

	case llvm::AtomicRMWInst::BinOp::UMin:
		opcode = spv::OpAtomicUMin;
		break;

	case llvm::AtomicRMWInst::BinOp::Max:
		opcode = spv::OpAtomicSMax;
		break;

	case llvm::AtomicRMWInst::BinOp::Min:
		opcode = spv::OpAtomicSMin;
		break;

	case llvm::AtomicRMWInst::BinOp::Xchg:
		opcode = spv::OpAtomicExchange;
		break;

	default:
		LOGE("Unrecognized atomicrmw opcode: %u.\n", unsigned(instruction->getOperation()));
		return false;
	}

	Operation *op = impl.allocate(opcode, instruction);
	op->add_ids({
	    impl.get_id_for_value(instruction->getPointerOperand()),
	    builder.makeUintConstant(spv::ScopeWorkgroup),
	    builder.makeUintConstant(0),
	    impl.get_id_for_value(instruction->getValOperand()),
	});

	impl.add(op);
	return true;
}

bool emit_shufflevector_instruction(Converter::Impl &impl, const llvm::ShuffleVectorInst *inst)
{
	Operation *op = impl.allocate(spv::OpVectorShuffle, inst);
	op->add_ids({ impl.get_id_for_value(inst->getOperand(0)), impl.get_id_for_value(inst->getOperand(1)) });

	unsigned num_outputs = inst->getType()->getVectorNumElements();
	for (unsigned i = 0; i < num_outputs; i++)
		op->add_literal(inst->getMaskValue(i));

	impl.add(op);
	return true;
}

bool emit_extractelement_instruction(Converter::Impl &impl, const llvm::ExtractElementInst *inst)
{
	if (auto *constant_int = llvm::dyn_cast<llvm::ConstantInt>(inst->getIndexOperand()))
	{
		Operation *op = impl.allocate(spv::OpCompositeExtract, inst);
		op->add_id(impl.get_id_for_value(inst->getVectorOperand()));
		op->add_literal(uint32_t(constant_int->getUniqueInteger().getZExtValue()));
		impl.add(op);
	}
	else
	{
		Operation *op = impl.allocate(spv::OpVectorExtractDynamic, inst);
		op->add_id(impl.get_id_for_value(inst->getVectorOperand()));
		op->add_id(impl.get_id_for_value(inst->getIndexOperand()));
		impl.add(op);
	}
	return true;
}

bool emit_insertelement_instruction(Converter::Impl &impl, const llvm::InsertElementInst *inst)
{
	auto *vec = inst->getOperand(0);
	auto *value = inst->getOperand(1);
	auto *index = inst->getOperand(2);

	if (!llvm::isa<llvm::ConstantInt>(index))
	{
		LOGE("Index to insertelement must be a constant.\n");
		return false;
	}
	Operation *op = impl.allocate(spv::OpCompositeInsert, inst);
	op->add_id(impl.get_id_for_value(value));
	op->add_id(impl.get_id_for_value(vec));
	op->add_literal(uint32_t(llvm::cast<llvm::ConstantInt>(index)->getUniqueInteger().getZExtValue()));
	impl.add(op);
	return true;
}

bool analyze_getelementptr_instruction(Converter::Impl &impl, const llvm::GetElementPtrInst *inst)
{
	auto itr = impl.llvm_global_variable_to_resource_mapping.find(inst->getOperand(0));
	if (itr != impl.llvm_global_variable_to_resource_mapping.end())
		impl.llvm_global_variable_to_resource_mapping[inst] = itr->second;

	return true;
}

bool analyze_load_instruction(Converter::Impl &impl, const llvm::LoadInst *inst)
{
	auto itr = impl.llvm_global_variable_to_resource_mapping.find(inst->getPointerOperand());
	if (itr != impl.llvm_global_variable_to_resource_mapping.end())
		impl.llvm_global_variable_to_resource_mapping[inst] = itr->second;

	return true;
}

bool analyze_extractvalue_instruction(Converter::Impl &impl, const llvm::ExtractValueInst *inst)
{
	if (impl.llvm_values_potential_sparse_feedback.count(inst->getAggregateOperand()) == 0)
		return true;

	// If we extract the 4th argument of a resource load instruction, we know the instruction is sparse feedback.
	if (inst->getNumIndices() == 1 && inst->getIndices()[0] == 4)
	{
		impl.builder().addCapability(spv::CapabilitySparseResidency);
		impl.llvm_value_is_sparse_feedback.insert(inst->getAggregateOperand());
	}
	return true;
}
} // namespace dxil_spv
