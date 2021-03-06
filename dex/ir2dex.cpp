/*@
XOC Release License

Copyright (c) 2013-2014, Alibaba Group, All rights reserved.

    compiler@aliexpress.com

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the Su Zhenyu nor the names of its contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

author: Su Zhenyu
@*/
#include "libdex/DexFile.h"
#include "libdex/DexClass.h"
#include "liropcode.h"

#include "drAlloc.h"
#include "d2d_comm.h"
#include "../opt/cominc.h"
#include "dx_mgr.h"
#include "../opt/prdf.h"
#include "dex.h"
#include "gra.h"
#include "dex_util.h"
#include "dex2ir.h"
#include "ir2dex.h"


bool IR2Dex::is_builtin(IR const* ir, BLTIN_TYPE bt)
{
	ASSERT0(ir && ir->is_call());
	VAR const* v = CALL_idinfo(ir);
	BLTIN_TYPE tbt = (BLTIN_TYPE)m_var2blt->mget(v);
	if (tbt != bt) return false;
#ifdef _DEBUG_
	switch (bt) {
	case BLTIN_INVOKE			:
		{
			/* CALL (r:I32:4) id:16
				ID ('LCAnimal;::<init>' r:I32:4) callee id:9
				PARAM param0 id:11
					INTCONST r:U32:4 (2 0x2) id:10
				PARAM param1 id:13
					INTCONST r:U32:4 (0 0x0) id:12
				PARAM param2 id:15
					PR1 (r:I32:4) id:14 */
			//The first is inoke-flag.
			IR * p = CALL_param_list(ir);
			ASSERT0(IR_type(p) == IR_CONST);
			UINT invoke_flags = CONST_int_val(p);
			p = IR_next(p);

			//The second is method-id.
			ASSERT0(p && IR_type(p) == IR_CONST);
			UINT method_id = CONST_int_val(p);
		}
		break;
	case BLTIN_NEW				:
		{
			/*CALL (r:PTR:4 ptbase:1) id:4
				ID ('#new' r:PTR:4 ptbase:1) callee id:1
				PARAM param0 id:3
					INTCONST r:U32:4 (1 0x1) id:2
				PR, retv0 */
			ASSERT0(CALL_param_list(ir) &&
					 IR_type(CALL_param_list(ir)) == IR_CONST);
			ASSERT0(IR_next(CALL_param_list(ir)) == NULL);
			ASSERT0(ir->hasReturnValue());
		}
		break;
	case BLTIN_NEW_ARRAY		:
		{
			/* (new_array, res), UNDEF, v5 <-
				(num_of_elem, op0)v8, (elem_type, op1)'[LCAnimal;'

			CALL (r:PTR:4 ptbase:1) id:66
				ID ('#new_array' r:PTR:4 ptbase:1) callee id:61
				PARAM param0 id:64
					PR2 (r:PTR:4 ptbase:1) id:62
				PARAM param1 id:65
        			INTCONST r:U32:4 (14 0xe) id:64
				PR7 (r:PTR:4 ptbase:1) retv id:69 */
			//The first is a pr that record the number of array elements.
			IR * p = CALL_param_list(ir);
			ASSERT0(p && p->is_pr());
			p = IR_next(p);

			//The second is array element type id.
			ASSERT0(p && IR_type(p) == IR_CONST);
			UINT elem_type_id = CONST_int_val(p);
			ASSERT0(IR_next(p) == NULL);
		}
		break;
	case BLTIN_MOVE_EXP         :
		//The res is a pr that record the exception handler.
		ASSERT0(ir->hasReturnValue());
		break;
	case BLTIN_MOVE_RES         :
		ASSERT0(ir->hasReturnValue());
		break;
	case BLTIN_THROW			:
		{
			//The first is a pr that is the reference of
			//the exception object.
			IR * p = CALL_param_list(ir);
			ASSERT0(p->is_pr());
			ASSERT0(IR_next(p) == NULL);
		}
		break;
	case BLTIN_CHECK_CAST       :
		{
			/*
			CALL (r:PTR:4 ptbase:1) id:113
				ID ('#check_cast' r:PTR:4 ptbase:1) callee id:108
				PARAM param0 id:110
					PR8 (r:I32:4) id:109
				PARAM param1 id:112
					INTCONST r:I32:4 (16 0x10) id:111
			*/
			//The first is a pr that record the object-ptr
			IR * p = CALL_param_list(ir);
			ASSERT0(p && p->is_pr());
			p = IR_next(p);

			//The second is class type id.
			ASSERT0(p && IR_type(p) == IR_CONST);
			UINT type_id = CONST_int_val(p);
			ASSERT0(IR_next(p) == NULL);
		}
		break;
	case BLTIN_FILLED_NEW_ARRAY	:
		{
			/*
			CALL (r:PTR:4 ptbase:1) id:82
				ID ('#filled_new_array' r:PTR:4 ptbase:1) callee id:75
				PARAM param0 id:77
					INTCONST r:I32:4 (0 0x0) id:76
				PARAM param1 id:77
					INTCONST r:I32:4 (13 0xd) id:76
				PARAM param2 id:79
					PR2 (r:I32:4) id:78
				PARAM param3 id:81
					PR8 (r:I32:4) id:80
			*/
			IR * p = CALL_param_list(ir);

			//The first record invoke flag.
			ASSERT0(IR_type(p) == IR_CONST);
			p = IR_next(p);

			//The second record class type id.
			ASSERT0(p && IR_type(p) == IR_CONST);
			p = IR_next(p);
			while (p != NULL) {
				ASSERT0(p->is_pr());
				p = IR_next(p);
			}
		}
		break;
	case BLTIN_FILL_ARRAY_DATA:
		{
			/*
			CALL (r:PTR:4 ptbase:1) id:82
				ID ('#fill_array_data' r:PTR:4 ptbase:1) callee id:75
				PARAM param0 id:77
					PR2 r:I32:4 id:76
				PARAM param1 id:77
					INTCONST r:U32:4 (13 0xd) id:76
			*/
			IR * p = CALL_param_list(ir);

			//The first record array obj-ptr.
			ASSERT0(p->is_pr());
			p = IR_next(p);

			//The second record binary data.
			ASSERT0(p && IR_type(p) == IR_CONST);
			ASSERT0(IR_next(p) == NULL);
		}
		break;
	case BLTIN_CONST_CLASS      :
		{
			/*
			CALL (r:PTR:4 ptbase:1) id:94
				ID ('#const_class' r:PTR:4 ptbase:1) callee id:88
				PARAM param0 id:91
					PR9 (r:I32:4) id:90
				PARAM param1 id:93
					INTCONST r:I32:4 (2 0x2) id:92
			*/
			ASSERT0(ir->hasReturnValue());
			ASSERT0(CALL_param_list(ir) &&
					 IR_type(CALL_param_list(ir)) == IR_CONST);
			ASSERT0(IR_next(CALL_param_list(ir)) == NULL);
		}
		break;
	case BLTIN_ARRAY_LENGTH     :
		ASSERT0(ir->hasReturnValue());
		ASSERT0(CALL_param_list(ir) &&
				 CALL_param_list(ir)->is_pr());
		ASSERT0(IR_next(CALL_param_list(ir)) == NULL);
		break;
	case BLTIN_MONITOR_ENTER    :
		ASSERT0(CALL_param_list(ir) &&
				 CALL_param_list(ir)->is_pr());
		ASSERT0(IR_next(CALL_param_list(ir)) == NULL);
		break;
	case BLTIN_MONITOR_EXIT     :
		ASSERT0(CALL_param_list(ir) &&
				 CALL_param_list(ir)->is_pr());
		ASSERT0(IR_next(CALL_param_list(ir)) == NULL);
		break;
	case BLTIN_INSTANCE_OF      :
		{
			/*
			CALL (r:PTR:4 ptbase:1) id:82
				ID ('#instance_of' r:PTR:4 ptbase:1) callee id:75
				PARAM param0 id:77
					PR r:I32:4 (0 0x0) id:76
				PARAM param1 id:77
					PR r:I32:4 (13 0xd) id:76
				PARAM param2 id:79
					INTCONST
			*/
			IR * p = CALL_param_list(ir);
			//The first is object-ptr reg.
			ASSERT0(p && p->is_pr());
			p = IR_next(p);

			//The second is type-id.
			ASSERT0(IR_type(p) == IR_CONST);
			ASSERT0(IR_next(p) == NULL);

			//The first is result reg..
			ASSERT0(ir->hasReturnValue());
		}
		break;
	case BLTIN_CMP_BIAS:
		{
			/*
			AABBCC
			cmpkind vAA <- vBB, vCC
			IR will be:
				IR_CALL
					param0: cmp-kind.
					param1: vBB
					param2: vCC
					res: vAA
			*/
			IR * p = CALL_param_list(ir);
			//The first is object-ptr reg.
			ASSERT0(p && p->is_int(m_dm));
			p = IR_next(p);

			ASSERT0(p && p->is_pr());
			p = IR_next(p);

			ASSERT0(p && p->is_pr());
			ASSERT0(IR_next(p) == NULL);
			ASSERT0(ir->hasReturnValue());
		}
		break;
	default: ASSERT0(0);
	}
#endif
	return true;
}


//AABBBB or AABBBBBBBB
LIR * IR2Dex::buildConstString(IN IR ** ir)
{
	IR * tir = *ir;
	ASSERT0(tir->is_stpr());
	UINT vx = get_vreg(STPR_no(tir));
	VAR * v = ID_info(LDA_base(STPR_rhs(tir)));
	CHAR const* n = SYM_name(VAR_name(v));
	LIRABOp * lir = (LIRABOp*)_ymalloc(sizeof(LIRABOp));
	lir->opcode = LOP_CONST_STRING;
	LIR_dt(lir) = LIR_JDT_unknown; //see dir2lir.c
	lir->vA = vx;
	lir->vB = m_var2ofst->mget(v);
	*ir = IR_next(*ir);
	return (LIR*)lir;
}


//AABBBB
LIR * IR2Dex::buildSput(IN IR ** ir)
{
	IR * tir = *ir;
	ASSERT0(tir->is_st() && ST_rhs(tir)->is_pr());
	#ifdef _DEBUG_
	CHAR const* n = SYM_name(VAR_name(ST_idinfo(tir)));
	#endif
	LIRABOp * lir = (LIRABOp*)_ymalloc(sizeof(LIRABOp));
	lir->opcode = LOP_SPUT;
	LIR_dt(lir) = get_lir_ty(TY_dtype(tir->get_type()));
	lir->vA = get_vreg(ST_rhs(tir));
	lir->vB = m_var2ofst->mget(ST_idinfo(tir));
	*ir = IR_next(*ir);
	return (LIR*)lir;
}


//AABBBB
LIR * IR2Dex::buildSgetBasicTypeVar(IN IR ** ir)
{
	IR * tir = *ir;
	ASSERT0(tir->is_stpr());
	ASSERT0(STPR_rhs(tir)->is_ld());
	UINT vx = get_vreg(STPR_no(tir));
	VAR * v = LD_idinfo(STPR_rhs(tir));
	CHAR const* n = SYM_name(VAR_name(v));
	UINT field_id = m_var2ofst->mget(v);
	LIRABOp * lir = (LIRABOp*)_ymalloc(sizeof(LIRABOp));
	lir->opcode = LOP_SGET;
	LIR_dt(lir) = get_lir_ty(TY_dtype(tir->get_type()));
	lir->vA = vx;
	lir->vB = field_id;
	*ir = IR_next(*ir);
	return (LIR*)lir;
}


//AABBBB
LIR * IR2Dex::buildSgetObj(IN IR ** ir)
{
	IR * tir = *ir;
	ASSERT0(tir->is_stpr());
	ASSERT0(STPR_rhs(tir)->is_lda());
	UINT vx = get_vreg(STPR_no(tir));
	VAR * v = ID_info(LDA_base(STPR_rhs(tir)));
	CHAR const* n = SYM_name(VAR_name(v));
	UINT field_id = m_var2ofst->mget(v);
	LIRABOp * lir = (LIRABOp*)_ymalloc(sizeof(LIRABOp));
	lir->opcode = LOP_SGET;
	LIR_dt(lir) = LIR_JDT_object;
	lir->vA = vx;
	lir->vB = field_id;
	*ir = IR_next(*ir);
	return (LIR*)lir;
}


LIR * IR2Dex::buildMove(IN IR ** ir)
{
	IR * tir = *ir;
	ASSERT0(tir->is_stpr());
	ASSERT0(STPR_rhs(tir)->is_pr());
	UINT tgt = get_vreg(STPR_no(tir));
	UINT src = get_vreg(STPR_rhs(tir));
	if (tgt == src) {
		//Redundant move: vn=vn
		*ir = IR_next(*ir);
		return NULL;
	}
	LIRABOp * lir = (LIRABOp*)_ymalloc(sizeof(LIRABOp));
	lir->opcode = LOP_MOVE;
	lir->vA = tgt;
	lir->vB = src;
	Type const* ty = IR_dt(tir);
	if (ty == m_tr->i32 || ty == m_tr->f32 ||
		ty == m_tr->u32 || ty == m_tr->b ||
		ty == m_tr->i16 || ty == m_tr->u16 ||
		ty == m_tr->i8 || ty == m_tr->u8) {
		LIR_dt(lir) = LIR_JDT_unknown;
	} else if (ty == m_tr->ptr) {
		LIR_dt(lir) = LIR_JDT_object;
	} else if (ty == m_tr->i64 || ty == m_tr->f64 || ty == m_tr->u64) {
		LIR_dt(lir) = LIR_JDT_wide;
	} else {
		ASSERT0(0);
	}
	*ir = IR_next(*ir);
	return (LIR*)lir;
}


LIR * IR2Dex::buildCvt(IN IR ** ir)
{
	IR * tir = *ir;
	ASSERT0(tir->is_stpr());
	IR * rhs = STPR_rhs(tir);
	UINT vx = get_vreg(STPR_no(tir));
	UINT vy = get_vreg(CVT_exp(rhs));
	LIRABOp * lir = (LIRABOp*)_ymalloc(sizeof(LIRABOp));
	lir->opcode = LOP_CONVERT;
	lir->vA = vx;
	lir->vB = vy;

	//Determine the CVT src and target type.
	DATA_TYPE tgt = TY_dtype(rhs->get_type());
	DATA_TYPE src = TY_dtype(CVT_exp(rhs)->get_type());
	LIR_Convert_Kind x = LIR_convert_unknown;
	switch (src) {
	case D_I32:
		switch (tgt) {
		case D_I64: x = LIR_convert_i2l; break;
		case D_F32: x = LIR_convert_i2f; break;
		case D_F64: x = LIR_convert_i2d; break;
		case D_B: x = LIR_convert_i2b; break;
		case D_I8: x = LIR_convert_i2c; break;
		case D_I16: x = LIR_convert_i2s; break;
		default: ASSERT0(0);
		}
		break;
	case D_I64:
		switch (tgt) {
		case D_I32: x = LIR_convert_l2i; break;
		case D_F32: x = LIR_convert_l2f; break;
		case D_F64: x = LIR_convert_l2d; break;
		default: ASSERT0(0);
		}
		break;
	case D_F32:
		switch (tgt) {
		case D_I32: x = LIR_convert_f2i; break;
		case D_I64: x = LIR_convert_f2l; break;
		case D_F64: x = LIR_convert_f2d; break;
		default: ASSERT0(0);
		}
		break;
	case D_F64:
		switch (tgt) {
		case D_I32: x = LIR_convert_d2i; break;
		case D_I64: x = LIR_convert_d2l; break;
		case D_F32: x = LIR_convert_d2f; break;
		default: ASSERT0(0);
		}
		break;
	default:
		ASSERT0(0);
	}
	ASSERT0(x != LIR_convert_unknown);
	LIR_dt(lir) = x;
	*ir = IR_next(*ir);
	return (LIR*)lir;
}


//AA
//move-result-object: vA <- retvalue.
LIR * IR2Dex::buildMoveResult(IN IR ** ir)
{
	ASSERT0(is_builtin(*ir, BLTIN_MOVE_RES));
	LIRAOp * lir = (LIRAOp*)_ymalloc(sizeof(LIRAOp));
	lir->opcode = LOP_MOVE_RESULT;

	ASSERT0((*ir)->hasReturnValue());
	lir->vA = get_vreg(CALL_prno(*ir));

	Type const* ty = IR_dt(*ir);
	if (ty == m_tr->i32 || ty == m_tr->f32 ||
		ty == m_tr->u32 || ty == m_tr->b ||
		ty == m_tr->i16 || ty == m_tr->u16 ||
		ty == m_tr->i8 || ty == m_tr->u8) {
		LIR_dt(lir) = LIR_JDT_unknown;
	} else if (ty == m_tr->i64 || ty == m_tr->f64 || ty == m_tr->u64) {
		LIR_dt(lir) = LIR_JDT_wide;
	} else if (ty == m_tr->ptr) {
		LIR_dt(lir) = LIR_JDT_object;
	} else {
		ASSERT0(0);
	}
	*ir = IR_next(*ir);
	return (LIR*)lir;
}


//AA
LIR * IR2Dex::buildThrow(IN IR ** ir)
{
	ASSERT0(is_builtin(*ir, BLTIN_THROW));
	LIRAOp * lir = (LIRAOp*)_ymalloc(sizeof(LIRAOp));
	lir->opcode = LOP_THROW;
	LIR_dt(lir) = LIR_JDT_unknown;
	IR * p = CALL_param_list(*ir);
	ASSERT0(p);
	lir->vA = get_vreg(p);
	ASSERT0(IR_next(p) == NULL);
	*ir = IR_next(*ir);
	return (LIR*)lir;
}


//AA
LIR * IR2Dex::buildMonitorExit(IN IR ** ir)
{
	ASSERT0(is_builtin(*ir, BLTIN_MONITOR_EXIT));
	LIRAOp * lir = (LIRAOp*)_ymalloc(sizeof(LIRAOp));
	lir->opcode = LOP_MONITOR_EXIT;
	LIR_dt(lir) = LIR_JDT_unknown;
	IR * p = CALL_param_list(*ir);
	ASSERT0(p);

	lir->vA = get_vreg(p);
	ASSERT0(IR_next(p) == NULL);
	*ir = IR_next(*ir);
	return (LIR*)lir;
}


//AA
LIR * IR2Dex::buildMonitorEnter(IN IR ** ir)
{
	ASSERT0(is_builtin(*ir, BLTIN_MONITOR_ENTER));
	LIRAOp * lir = (LIRAOp*)_ymalloc(sizeof(LIRAOp));
	lir->opcode = LOP_MONITOR_ENTER;
	LIR_dt(lir) = LIR_JDT_unknown;
	IR * p = CALL_param_list(*ir);
	ASSERT0(p);

	lir->vA = get_vreg(p);
	ASSERT0(IR_next(p) == NULL);
	*ir = IR_next(*ir);
	return (LIR*)lir;
}


//AA
LIR * IR2Dex::buildMoveException(IN IR ** ir)
{
	ASSERT0(is_builtin(*ir, BLTIN_MOVE_EXP));
	LIRAOp * lir = (LIRAOp*)_ymalloc(sizeof(LIRAOp));
	lir->opcode = LOP_MOVE_EXCEPTION;

	ASSERT0((*ir)->hasReturnValue());

	Type const* ty = (*ir)->get_type();
	if (ty == m_tr->i32 || ty == m_tr->f32 ||
		ty == m_tr->u32 || ty == m_tr->b ||
		ty == m_tr->i16 || ty == m_tr->u16 ||
		ty == m_tr->i8 || ty == m_tr->u8) {
		LIR_dt(lir) = LIR_JDT_unknown;
	} else if (ty == m_tr->i64 || ty == m_tr->f64 || ty == m_tr->u64) {
		LIR_dt(lir) = LIR_JDT_wide;
	} else if (ty == m_tr->ptr) {
		LIR_dt(lir) = LIR_JDT_object;
	} else {
		ASSERT0(0);
	}

	lir->vA = get_vreg(CALL_prno(*ir));

	*ir = IR_next(*ir);
	return (LIR*)lir;
}


//AABBBB
LIR * IR2Dex::buildArrayLength(IN IR ** ir)
{
	ASSERT0(is_builtin(*ir, BLTIN_ARRAY_LENGTH));
	LIRABOp * lir = (LIRABOp*)_ymalloc(sizeof(LIRABOp));
	lir->opcode = LOP_ARRAY_LENGTH; //see genInstruction()
	LIR_dt(lir) = LIR_JDT_unknown; //see genInstruction()
	IR * tir = *ir;

	//vA
	ASSERT0(tir->hasReturnValue());
	lir->vA = get_vreg(CALL_prno(tir));

	//vB
	ASSERT0(CALL_param_list(tir));
	lir->vB = get_vreg(CALL_param_list(tir));
	ASSERT0(IR_next(CALL_param_list(tir)) == NULL);
	*ir = IR_next(*ir);
	return (LIR*)lir;
}


//AABBBB
LIR * IR2Dex::buildCheckCast(IN IR ** ir)
{
	ASSERT0(is_builtin(*ir, BLTIN_CHECK_CAST));
	LIRABOp * lir = (LIRABOp*)_ymalloc(sizeof(LIRABOp));
	lir->opcode = LOP_CHECK_CAST;
	IR * tir = *ir;
	IR * p = CALL_param_list(tir);
	ASSERT0(p);

	//object-ptr
	lir->vA = get_vreg(p);
	p = IR_next(p);
	ASSERT0(p);

	//class id.
	lir->vB = CONST_int_val(p);
	LIR_dt(lir) = LIR_JDT_unknown; //see genInstruction()
	p = IR_next(p);
	ASSERT0(p == NULL);
	*ir = IR_next(*ir);
	return (LIR*)lir;

}


//AABBBB
LIR * IR2Dex::buildConst(IN IR ** ir)
{
	IR * tir = *ir;
	ASSERT0(tir->is_stpr());
	ASSERT0(IR_type(STPR_rhs(tir)) == IR_CONST);
	UINT vx = get_vreg(STPR_no(tir));
	LIRConstOp * lir = (LIRConstOp*)_ymalloc(sizeof(LIRConstOp));
	lir->opcode = LOP_CONST;
	lir->vA = vx;
	lir->vB = CONST_int_val(STPR_rhs(tir));
	Type const* ty = IR_dt(STPR_rhs(tir));
	if (ty == m_tr->i32 || ty == m_tr->f32 ||
		ty == m_tr->u32 || ty == m_tr->b ||
		ty == m_tr->i16 || ty == m_tr->u16 ||
		ty == m_tr->i8 || ty == m_tr->u8) {
		//It seems dex does not distinguish
		//float and integer const.
		LIR_dt(lir) = LIR_JDT_unknown;
	} else if (ty == m_tr->i64 || ty == m_tr->f64 || ty == m_tr->u64) {
		LIR_dt(lir) = LIR_JDT_wide;
	} else {
		ASSERT0(0);
	}
	*ir = IR_next(*ir);
	return (LIR*)lir;
}


enum LIR_JDT_Kind IR2Dex::get_lir_ty(DATA_TYPE dt)
{
	switch (dt) {
	case D_B	: return LIR_JDT_boolean;
	case D_I8   : return LIR_JDT_char;
	case D_I16  : return LIR_JDT_short;
	case D_I32 	: return LIR_JDT_int;
	case D_I64  : return LIR_JDT_long;
	case D_U8   : return LIR_JDT_byte;
	case D_U32  : return LIR_JDT_none;
	case D_U64  : return LIR_JDT_wide;
	case D_F32	: return LIR_JDT_float;
	case D_F64  : return LIR_JDT_double;
	case D_PTR	: return LIR_JDT_object;
	default: ASSERT0(0);
	}
	return LIR_JDT_unknown;
}


UINT IR2Dex::findFieldId(IR * ir, IR * objptr)
{
	/*
	 e.g: CAnimal a = new CAnimal();
	 	DEX vs. IR:
		new_instance, (obj_ptr)v1 <- LCAnimal;
		CALL (r:PTR:4 ptbase:1) id:4
			ID ('#new' r:PTR:4 ptbase:1) callee id:1
			PARAM param0 id:3
				INTCONST r:U32:4 (1 0x1) id:2
			PR1 (r:PTR:4 ptbase:1) retv0 id:7

		invoke, LCAnimal;::<init>, arg(v1)
		CALL (r:I32:4) id:16
			ID ('LCAnimal;::<init>' r:I32:4) callee id:9
			PARAM param0 id:11
				INTCONST r:U32:4 (2 0x2) id:10
			PARAM param1 id:13
				INTCONST r:U32:4 (0 0x0) id:12
			PARAM param2 id:15
				PR1 (r:I32:4) id:14

		iget, INT, v0 <- (this_ptr)v1, (ofst)LCAnimal;::v
		ST (r:I32:4 ofst:0) id:21
			PR2 (r:I32:4) mem_addr id:18
			ILD (r:I32:4 ofst:0) store_value id:20
				PR1 (r:PTR:4 ptbase:1) mem_addr id:19
	*/
	ASSERT0(ir->is_stmt() && objptr->is_exp() && ir->is_kids(objptr));
	IR_DU_MGR * du_mgr = m_ru->get_du_mgr();
	ASSERT0(du_mgr);
	IR const* def = du_mgr->getExactAndUniqueDef(objptr);
	ASSERT0(def);

	ASSERT0(is_builtin(def, BLTIN_NEW));
	UINT field_id = CONST_int_val(CALL_param_list(def));
	return field_id;
}


//ABCCCC
LIR * IR2Dex::buildIget(IN IR ** ir)
{
	IR * tir = *ir;
	ASSERT0(tir->is_stpr());
	ASSERT0(IR_type(STPR_rhs(tir)) == IR_ILD);
	DATA_TYPE ty = TY_dtype(tir->get_type());
	UINT vx = get_vreg(STPR_no(tir));

	//base_ptr
	IR * objptr = ILD_base(STPR_rhs(tir));
	//UINT field_id = findFieldId(tir, objptr);
	UINT field_id = ILD_ofst(STPR_rhs(tir));
	field_id /= m_d2ir->get_ofst_addend();
	ASSERT0(IR_type(objptr) == IR_PR);
	UINT vy = get_vreg(objptr);
	LIRABCOp * lir = (LIRABCOp*)_ymalloc(sizeof(LIRABCOp));
	lir->opcode = LOP_IGET;
	lir->vA = vx; //result reg
	lir->vB = vy; //base_ptr
	lir->vC = field_id; //field_id
	LIR_dt(lir) = get_lir_ty(ty);
	*ir = IR_next(*ir);
	return (LIR*)lir;
}


//AABBCC
LIR * IR2Dex::buildArray(IN IR ** ir)
{
	IR * tir = *ir;
	ASSERT0(tir->is_stpr());

	//DEX only has one-dimension array.
	IR * base = ARR_base(STPR_rhs(tir));
	IR * ofst = ARR_sub_list(STPR_rhs(tir));
	ASSERT0(base->is_pr() && ofst->is_pr());
	LIRABCOp * lir = (LIRABCOp*)_ymalloc(sizeof(LIRABCOp));
	lir->opcode = LOP_AGET;
	LIR_dt(lir) = get_lir_ty(TY_dtype(tir->get_type()));
	lir->vA = get_vreg(STPR_no(tir));
	lir->vB = get_vreg(base);
	lir->vC = get_vreg(ofst);
	*ir = IR_next(*ir);
	return (LIR*)lir;
}


LIR * IR2Dex::buildBinRegLit(IN IR ** ir)
{
	IR const* tir = *ir;
	ASSERT0(tir->is_stpr());
	IR const* rhs = STPR_rhs(tir);
	IR * op0 = BIN_opnd0(rhs);
	IR * op1 = BIN_opnd1(rhs);

	UINT vA = get_vreg(STPR_no(tir));
	UINT vB = get_vreg(op0);
	UINT vC = CONST_int_val(op1);
	ASSERT0((is_us8(vA) && is_us8(vB) && is_s8(vC)) ||
			 (is_us4(vA) && is_us4(vA) && is_s16(vC)));
	enum _LIROpcode lty = LOP_NOP;
	switch (IR_type(rhs)) {
	case IR_ADD   : lty = LOP_ADD_LIT; break;
	case IR_SUB   : lty = LOP_SUB_LIT; break;
	case IR_MUL   : lty = LOP_MUL_LIT; break;
	case IR_DIV   : lty = LOP_DIV_LIT; break;
	case IR_REM   : lty = LOP_REM_LIT; break;
	case IR_BAND  : lty = LOP_AND_LIT; break;
	case IR_BOR   :	lty = LOP_OR_LIT; break;
	case IR_XOR   : lty = LOP_XOR_LIT; break;
	case IR_ASR   : lty = LOP_SHR_LIT; break;
	case IR_LSR   : lty = LOP_USHR_LIT; break;
	case IR_LSL   : lty = LOP_SHL_LIT; break;
	default: ASSERT0(0);
	}

	LIRABCOp * lir = (LIRABCOp*)_ymalloc(sizeof(LIRABCOp));
	lir->opcode = lty;
	lir->vA = vA;
	lir->vB = vB;
	lir->vC = vC;
	LIR_dt(lir) = get_lir_ty(TY_dtype(tir->get_type()));
	*ir = IR_next(*ir);
	return (LIR*)lir;
}


LIR * IR2Dex::buildBinRegReg(IN IR ** ir)
{
	IR const* tir = *ir;
	IR const* rhs = STPR_rhs(tir);
	ASSERT0(rhs->is_binary_op());
	ASSERT0(tir->is_stpr());
	IR * op0 = BIN_opnd0(rhs);
	IR * op1 = BIN_opnd1(rhs);
	if (tir->is_pr_equal(op1) && rhs->is_commutative()) {
		IR * t = op0;
		op0 = op1;
		op1 = t;
	}

	bool is_assign_equ = false;
	UINT vA = get_vreg(STPR_no(tir));
	UINT vB = get_vreg(op0);
	UINT vC = get_vreg(op1);
	ASSERT0(is_us8(vA));
	ASSERT0(is_us8(vB));
	ASSERT0(is_us8(vC));
	if (vA == vB && is_us4(vA) && is_us4(vC)) {
		is_assign_equ = true;
	}
	enum _LIROpcode lty = LOP_NOP;
	switch (IR_type(rhs)) {
	case IR_ADD   : lty = is_assign_equ ? LOP_ADD_ASSIGN : LOP_ADD; break;
	case IR_SUB   :
		lty = is_assign_equ ? LOP_SUB_ASSIGN : LOP_SUB;
		break;
	case IR_MUL   : lty = is_assign_equ ? LOP_MUL_ASSIGN : LOP_MUL; break;
	case IR_DIV   : lty = is_assign_equ ? LOP_DIV_ASSIGN : LOP_DIV; break;
	case IR_REM   : lty = is_assign_equ ? LOP_REM_ASSIGN : LOP_REM; break;
	case IR_BAND  : lty = is_assign_equ ? LOP_AND_ASSIGN : LOP_AND; break;
	case IR_BOR   :	lty = is_assign_equ ? LOP_OR_ASSIGN : LOP_OR; break;
	case IR_XOR   : lty = is_assign_equ ? LOP_XOR_ASSIGN : LOP_XOR; break;
	case IR_BNOT  : lty = is_assign_equ ? LOP_NOT : LOP_NOT; break;
	case IR_ASR   : lty = is_assign_equ ? LOP_SHR_ASSIGN : LOP_SHR; break;
	case IR_LSR   : lty = is_assign_equ ? LOP_USHR_ASSIGN : LOP_USHR; break;
	case IR_LSL   : lty = is_assign_equ ? LOP_SHL_ASSIGN : LOP_SHL; break;
	default: ASSERT0(0);
	}

	LIR * lir;
	if (is_assign_equ) {
		lir = (LIR*)_ymalloc(sizeof(LIRABOp));
		lir->opcode = lty;
		((LIRABOp*)lir)->vA = vA;
		((LIRABOp*)lir)->vB = vC;
	} else {
		lir = (LIR*)_ymalloc(sizeof(LIRABCOp));
		lir->opcode = lty;
		((LIRABCOp*)lir)->vA = vA;
		((LIRABCOp*)lir)->vB = vB;
		((LIRABCOp*)lir)->vC = vC;
	}
	LIR_dt(lir) = get_lir_ty(TY_dtype(tir->get_type()));
	*ir = IR_next(*ir);
	return lir;
}


//AABBCC
LIR * IR2Dex::buildBinOp(IN IR ** ir)
{
	IR * tir = *ir;
	ASSERT0(tir->is_stpr());
	IR * op0 = BIN_opnd0(STPR_rhs(tir));
	IR * op1 = BIN_opnd1(STPR_rhs(tir));
	if (op0->is_pr() && op1->is_pr()) {
		return buildBinRegReg(ir);
	} else if (op0->is_pr() && op1->is_const()) {
		return buildBinRegLit(ir);
	} else {
		ASSERT0(0);
	}
	return NULL;
}


//AB
LIR * IR2Dex::buildUniOp(IN IR ** ir)
{
	IR * tir = *ir;
	ASSERT0(tir->is_stpr());
	IR * op0 = UNA_opnd0(STPR_rhs(tir));
	ASSERT(op0->is_pr(), ("just support pr operation"));

	enum _LIROpcode lty = LOP_NOP;
	switch (IR_type(STPR_rhs(tir))) {
	case IR_NEG   : lty = LOP_NEG; break;
	case IR_BNOT  : lty = LOP_NOT; break;
	default:
		ASSERT0(0);
	}

	LIR * lir = (LIR*)_ymalloc(sizeof(LIRABOp));
	lir->opcode = lty;
	((LIRABOp*)lir)->vA = get_vreg(STPR_no(tir));
	((LIRABOp*)lir)->vB = get_vreg(op0);
	LIR_dt(lir) = get_lir_ty(TY_dtype(tir->get_type()));
	*ir = IR_next(*ir);
	return lir;
}


LIR * IR2Dex::convertStoreVar(IN OUT IR ** ir, IN IR2D_CTX * cont)
{
	IR * tir = *ir;
	ASSERT0(tir->is_st());
	IR * rhs = ST_rhs(tir);
	switch (IR_type(rhs)) {
	case IR_PR  :
		return buildSput(ir);
	default: ASSERT0(0);
	}
	return NULL;
}


LIR * IR2Dex::convertStorePR(IN OUT IR ** ir, IN IR2D_CTX * cont)
{
	IR * tir = *ir;
	ASSERT0(tir->is_stpr());
	IR * rhs = STPR_rhs(tir);
	switch (IR_type(rhs)) {
	case IR_LD:
		//vA<-ld(id)
		return buildSgetBasicTypeVar(ir);
	case IR_LDA:
		{
			IR * id = LDA_base(rhs);
			ASSERT0(IR_type(id) == IR_ID);
			if (id->is_str()) {
				return buildConstString(ir);
			}
			ASSERT0(id->is_mc());
			//vA<-&(obj)
			return buildSgetObj(ir);
		}
		break;
	case IR_CONST:
		return buildConst(ir);
	case IR_ILD:
		return buildIget(ir);
	case IR_ADD   :
	case IR_SUB   :
	case IR_MUL   :
	case IR_DIV   :
	case IR_REM   :
	case IR_BAND  :
	case IR_BOR   :
	case IR_XOR   :
	case IR_ASR   :
	case IR_LSR   :
	case IR_LSL   :
		return buildBinOp(ir);
	case IR_LAND  :
	case IR_LOR   :
		ASSERT0(0);
		break;
	case IR_LNOT  :
		ASSERT0(0);
		break;
	case IR_BNOT  :
	case IR_NEG   :
		return buildUniOp(ir);
	case IR_ARRAY :
		return buildArray(ir);
	case IR_CVT   :
		return buildCvt(ir);
	case IR_PR  :
		return buildMove(ir);
	default: ASSERT0(0);
	}
	ASSERT0(0);
	return NULL;
}


//Bulid aput: res -> op0(array base), op1(subscript)
LIR * IR2Dex::convertStoreArray(IN OUT IR ** ir, IN IR2D_CTX * cont)
{
	IR * tir = *ir;
	ASSERT0(tir->is_starray());
	LIRABCOp * lir = (LIRABCOp*)_ymalloc(sizeof(LIRABCOp));
	IR * rhs = STARR_rhs(tir);
	IR * lhs = ARR_base(tir);
	ASSERT0(rhs->is_pr());
	LIR_res(lir) = get_vreg(rhs);
	LIR_dt(lir) = get_lir_ty(TY_dtype(tir->get_type()));

	IR * base = ARR_base(tir);
	IR * ofst = ARR_sub_list(tir);
	ASSERT0(base->is_pr() && base->is_ptr());

	//ofst may be renamed with a signed type.
	ASSERT0(ofst->is_pr());
	LIR_op0(lir) = get_vreg(base);
	LIR_op1(lir) = get_vreg(ofst);
	lir->opcode = LOP_APUT;
	
	ASSERT0(IR_dt(tir) == IR_dt(rhs) ||
			tir->get_dtype_size(m_dm) == rhs->get_dtype_size(m_dm));
	*ir = IR_next(*ir);
	return (LIR*)lir;
}


//Bulid iput: res -> op0(obj-ptr), op1(ofst)
LIR * IR2Dex::convertIstore(IN OUT IR ** ir, IN IR2D_CTX * cont)
{
	IR * tir = *ir;
	ASSERT0(tir->is_ist());
	LIRABCOp * lir = (LIRABCOp*)_ymalloc(sizeof(LIRABCOp));
	IR * rhs = IST_rhs(tir);
	IR * lhs = IST_base(tir);
	ASSERT0(rhs->is_pr());
	LIR_res(lir) = get_vreg(rhs);
	LIR_dt(lir) = get_lir_ty(TY_dtype(tir->get_type()));

	ASSERT0(lhs->is_pr());
	LIR_op0(lir) = get_vreg(lhs);
	LIR_op1(lir) = IST_ofst(tir) / m_d2ir->get_ofst_addend();
	lir->opcode = LOP_IPUT;
	ASSERT0(IR_dt(tir) == IR_dt(rhs) ||
			 tir->get_dtype_size(m_dm) == rhs->get_dtype_size(m_dm));
	*ir = IR_next(*ir);
	return (LIR*)lir;
}


LIR * IR2Dex::buildInvoke(IN IR ** ir)
{
	IR * tir = *ir;
	LIRInvokeOp * lir = (LIRInvokeOp*)_ymalloc(sizeof(LIRInvokeOp));
	lir->opcode = LOP_INVOKE;

	IR * p = CALL_param_list(tir);
	ASSERT0(p);

	//Inoke-kind.
	ASSERT0(IR_type(p) == IR_CONST);
	INVOKE_KIND ik = (INVOKE_KIND)CONST_int_val(p);
	UINT flag = 0;
	switch (ik) {
	case INVOKE_VIRTUAL: flag = LIR_invoke_virtual; break;
	case INVOKE_SUPER: flag = LIR_invoke_super; break;
	case INVOKE_DIRECT: flag = LIR_invoke_direct; break;
	case INVOKE_STATIC: flag = LIR_invoke_static; break;
	case INVOKE_INTERFACE: flag = LIR_invoke_interface; break;
	case INVOKE_VIRTUAL_RANGE:
		flag = LIR_invoke_virtual; flag |= LIR_Range; break;
	case INVOKE_SUPER_RANGE:
		flag = LIR_invoke_super; flag |= LIR_Range; break;
	case INVOKE_DIRECT_RANGE:
		flag = LIR_invoke_direct; flag |= LIR_Range; break;
	case INVOKE_STATIC_RANGE:
		flag = LIR_invoke_static; flag |= LIR_Range; break;
	case INVOKE_INTERFACE_RANGE:
		flag = LIR_invoke_interface; flag |= LIR_Range; break;
	case INVOKE_UNDEF:
	default: ASSERT0(0);
	}
	LIR_dt(lir) = flag;
	p = IR_next(p);

	//Method id.
	ASSERT0(p && IR_type(p) == IR_CONST);
	lir->ref = CONST_int_val(p);
	p = IR_next(p);

	//Real invoke paramenters.
	UINT i = 0;
	IR * t = p;
	while (t != NULL) {
		ASSERT0(t->is_pr());
		i++;
		if (is_pair(t)) { i++; }
		t = IR_next(t);
	}
	if (i > 0) {
		lir->args = (USHORT*)_ymalloc(i * sizeof(USHORT));
	}
	lir->argc = i;
	INT j = 0;
	while (p != NULL) {
		ASSERT0(p->is_pr());
		UINT vx = get_vreg(p);
		lir->args[j] = vx;

		if (is_pair(p)) {
			j++;
			lir->args[j] = vx + 1;
		}

		p = IR_next(p);
		j++;
	}
	*ir = IR_next(*ir);
	return (LIR*)lir;
}


LIR * IR2Dex::buildNewInstance(IN IR ** ir)
{
	ASSERT0(is_builtin(*ir, BLTIN_NEW));
	LIRABOp * lir = (LIRABOp*)_ymalloc(sizeof(LIRABOp));
	lir->opcode = LOP_NEW_INSTANCE;
	IR * tir = *ir;
	//class-id
	ASSERT0(CALL_param_list(tir) &&
			 IR_type(CALL_param_list(tir)) == IR_CONST);
	lir->vB = CONST_int_val(CALL_param_list(tir));
	LIR_dt(lir) = LIR_JDT_object;

	//Method idx.
	ASSERT0(tir->hasReturnValue());
	lir->vA = get_vreg(CALL_prno(tir));
	*ir = IR_next(*ir);
	return (LIR*)lir;
}


/*
FMT:AABBBBBBBB
filld-array-data, vAA, type_id
AA: array obj-ptr.
BBBBBBBB: signed branch offset to table data.

Fill the given array with the indicated data.
NOTE: it is very different with filled-new-array.
*/
LIR * IR2Dex::buildFillArrayData(IN IR ** ir)
{
	ASSERT0(is_builtin(*ir, BLTIN_FILL_ARRAY_DATA));
	LIRSwitchOp * lir = (LIRSwitchOp*)_ymalloc(sizeof(LIRSwitchOp));
	lir->opcode = LOP_FILL_ARRAY_DATA;
	IR * tir = *ir;
	IR * p = CALL_param_list(tir);

	//The first parameter is array obj-ptr.
	ASSERT0(p && IR_type(p) == IR_PR);
	lir->value = get_vreg(p);
	p = IR_next(p);

	//The second parameter record the pointer to filling data.
	ASSERT0(p && p->is_uint(m_dm));
	lir->data = (UInt16*)CONST_int_val(p);

	#ifdef _DEBUG_
	//Check the legality of content at lir data.
	//pdata[0]: the magic number of code
	//0x100 PACKED_SWITCH, 0x200 SPARSE_SWITCH, 0x300 FILL_ARRAY_DATA
	USHORT const* pdata = (USHORT const*)lir->data;
	ASSERT0(pdata[0] == 0x300);
	//pdata[1]: size of each element.
	//pdata[2]: the number of element.
	UINT size_of_elem = pdata[1];
	UINT num_of_elem = pdata[2];
	UINT data_size = num_of_elem * size_of_elem;
	#endif
	*ir = IR_next(*ir);
	return (LIR*)lir;
}


/*
filled-new-array {parameters}, type_id
Generates a new array of type_id and fills it with the parameters5.
Reference to the newly generated array can be obtained by a
move-result-object instruction, immediately following the
filled-new-array instruction.
*/
LIR * IR2Dex::buildFilledNewArray(IN IR ** ir)
{
	ASSERT0(is_builtin(*ir, BLTIN_FILLED_NEW_ARRAY));
	LIRInvokeOp * lir = (LIRInvokeOp*)_ymalloc(sizeof(LIRInvokeOp));
	lir->opcode = LOP_FILLED_NEW_ARRAY;
	IR * tir = *ir;
	IR * p = CALL_param_list(tir);

	//first parameter is invoke-kind.
	ASSERT0(p && p->is_uint(m_dm));
	LIR_dt(lir) = CONST_int_val(p);
	p = IR_next(p);

	//second one is class-id.
	ASSERT0(p && p->is_int(m_dm));
	lir->ref = CONST_int_val(p);
	p = IR_next(p);

	//and else parameters.
	UINT i = 0;
	IR * t = p;
	while (t != NULL) {
		ASSERT0(IR_type(t) == IR_PR);
		t = IR_next(t);
		i++;
	}
	lir->argc = i;
	lir->args = (UInt16*)_ymalloc(sizeof(UInt16) * i);
	i = 0;
	while (p != NULL) {
		lir->args[i] = get_vreg(p);
		p = IR_next(p);
		i++;
	}
	*ir = IR_next(*ir);
	return (LIR*)lir;
}


/*
AABBBB
const-class vA <- type_id
Moves the class object of a class identified by
type_id (e.g. Object.class) into vA.
*/
LIR * IR2Dex::buildConstClass(IN IR ** ir)
{
	ASSERT0(is_builtin(*ir, BLTIN_CONST_CLASS));
	IR * tir = *ir;
	LIRABOp * lir = (LIRABOp*)_ymalloc(sizeof(LIRABOp));
	lir->opcode = LOP_CONST_CLASS;
	LIR_dt(lir) = LIR_JDT_unknown;

	//The number of array element.
	ASSERT0(tir->hasReturnValue());
	lir->vA = get_vreg(CALL_prno(tir));

	//Type id of array element.
	ASSERT0(CALL_param_list(tir) &&
			 IR_type(CALL_param_list(tir)) == IR_CONST);
	lir->vB = CONST_int_val(CALL_param_list(tir));
	ASSERT0(IR_next(CALL_param_list(tir)) == NULL);

	*ir = IR_next(*ir);
	return (LIR*)lir;
}


//ABCCCC
//new-array vA(res) <- vB(op0), LCAnimal(op1)
LIR * IR2Dex::buildNewArray(IN IR ** ir)
{
	ASSERT0(is_builtin(*ir, BLTIN_NEW_ARRAY));
	IR * tir = *ir;
	LIRABCOp * lir = (LIRABCOp*)_ymalloc(sizeof(LIRABCOp));
	lir->opcode = LOP_NEW_ARRAY;
	LIR_dt(lir) = LIR_JDT_unknown; //see dir2lir.c

	IR * p = CALL_param_list(tir);
	ASSERT0(p);

	//The number of array element.
	ASSERT0(IR_type(p) == IR_PR);
	lir->vB = get_vreg(p);
	p = IR_next(p);

	//Type id of array element.
	ASSERT0(IR_type(p) == IR_CONST);
	lir->vC = CONST_int_val(p);
	ASSERT0(IR_next(p) == NULL);

	ASSERT0(tir->hasReturnValue());
	lir->vA = get_vreg(CALL_prno(tir));
	ASSERT0(IR_dt(tir) == m_tr->ptr);
	*ir = IR_next(*ir);
	return (LIR*)lir;
}


/*
instance-of vA <- vB,type_id
ABCCCC
Checks whether vB is instance of a class identified by type_id.
Sets vA non-zero if it is, 0 otherwise.
*/
LIR * IR2Dex::buildInstanceOf(IN IR ** ir)
{
	ASSERT0(is_builtin(*ir, BLTIN_INSTANCE_OF));
	IR * tir = *ir;
	LIRABCOp * lir = (LIRABCOp*)_ymalloc(sizeof(LIRABCOp));
	lir->opcode = LOP_INSTANCE_OF;
	LIR_dt(lir) = LIR_JDT_unknown;

	//Result register.
	ASSERT0(tir->hasReturnValue());
	lir->vA = get_vreg(CALL_prno(tir));

	//Object-ptr register.
	IR * p = CALL_param_list(tir);
	ASSERT0(IR_type(p) == IR_PR);
	lir->vB = get_vreg(p);
	p = IR_next(p);

	//type-id
	ASSERT0(IR_type(p) == IR_CONST);
	lir->vC = CONST_int_val(p);
	ASSERT0(IR_next(p) == NULL);

	*ir = IR_next(*ir);
	return (LIR*)lir;
}


/*
AABBCC
cmpkind vAA <- vBB, vCC
IR will be:
	IR_CALL
		param0: cmp-kind.
		param1: vBB
		param2: vCC
		res: vAA
*/
LIR * IR2Dex::buildCmpBias(IN IR ** ir)
{
	ASSERT0(is_builtin(*ir, BLTIN_CMP_BIAS));
	IR * tir = *ir;
	LIRABCOp * lir = (LIRABCOp*)_ymalloc(sizeof(LIRABCOp));

	//cmp-kind
	IR * p = CALL_param_list(tir);
	ASSERT0(p && p->is_int(m_dm) && IR_dt(p) == m_tr->u32);
	CMP_KIND ck = (CMP_KIND)CONST_int_val(p);
	p = IR_next(p);

	ASSERT0(p && IR_type(p) == IR_PR);
	LIR_op0(lir) = get_vreg(p);
	p = IR_next(p);

	ASSERT0(p && IR_type(p) == IR_PR);
	LIR_op1(lir) = get_vreg(p);
	p = IR_next(p);

	ASSERT0(tir->hasReturnValue());
	LIR_res(lir) = get_vreg(CALL_prno(tir));

	switch (ck) {
	case CMPL_FLOAT:
		LIR_opcode(lir) = LOP_CMPL;
		LIR_dt(lir) = LIR_CMP_float;
		break;
	case CMPG_FLOAT:
		LIR_opcode(lir) = LOP_CMPG;
		LIR_dt(lir) = LIR_CMP_float;
		break;
	case CMPL_DOUBLE:
		LIR_opcode(lir) = LOP_CMPL;
		LIR_dt(lir) = LIR_CMP_double;
		break;
	case CMPG_DOUBLE:
		LIR_opcode(lir) = LOP_CMPG;
		LIR_dt(lir) = LIR_CMP_double;
		break;
	case CMP_LONG:
		LIR_opcode(lir) = LOP_CMP_LONG;
		LIR_dt(lir) = LIR_convert_unknown;
		break;
	default: ASSERT0(0);
	}
	*ir = IR_next(*ir);
	return (LIR*)lir;
}


LIR * IR2Dex::convertCall(IN OUT IR ** ir, IN IR2D_CTX * cont)
{
	VAR * v = CALL_idinfo(*ir);
	BLTIN_TYPE bt = (BLTIN_TYPE)m_var2blt->mget(v);
	switch (bt) {
	case BLTIN_INVOKE:
		return buildInvoke(ir);
	case BLTIN_NEW:
		return buildNewInstance(ir);
	case BLTIN_NEW_ARRAY:
		return buildNewArray(ir);
	case BLTIN_MOVE_EXP:
		return buildMoveException(ir);
	case BLTIN_MOVE_RES:
		return buildMoveResult(ir);
	case BLTIN_THROW:
		return buildThrow(ir);
	case BLTIN_CHECK_CAST:
		return buildCheckCast(ir);
	case BLTIN_FILLED_NEW_ARRAY:
		return buildFilledNewArray(ir);
	case BLTIN_FILL_ARRAY_DATA:
		return buildFillArrayData(ir);
	case BLTIN_CONST_CLASS:
		return buildConstClass(ir);
	case BLTIN_ARRAY_LENGTH:
		return buildArrayLength(ir);
	case BLTIN_MONITOR_ENTER:
		return buildMonitorEnter(ir);
	case BLTIN_MONITOR_EXIT:
		return buildMonitorExit(ir);
	case BLTIN_INSTANCE_OF:
		return buildInstanceOf(ir);
	case BLTIN_CMP_BIAS:
		return buildCmpBias(ir);
	default: ASSERT0(0);
	}
	return NULL;
}


LIR * IR2Dex::convertIcall(IN OUT IR ** ir, IN IR2D_CTX * cont)
{
	*ir = IR_next(*ir);
	return NULL;
}


//return vAA
LIR * IR2Dex::convertReturn(IN OUT IR ** ir, IN IR2D_CTX * cont)
{
	LIRAOp * lir = (LIRAOp*)_ymalloc(sizeof(LIRAOp));
	lir->opcode = LOP_RETURN;

	IR * retval = RET_exp(*ir);
	if (retval != NULL) {
		ASSERT0(IR_next(retval) == NULL);
		ASSERT0(IR_type(retval) == IR_PR);
		Type const* ty = IR_dt(retval);
		if (ty == m_tr->ptr) {
			LIR_dt(lir) = LIR_JDT_object;
		} else if (ty == m_tr->i32 || ty == m_tr->f32 ||
				   ty == m_tr->u32 || ty == m_tr->b ||
				   ty == m_tr->i16 || ty == m_tr->u16 ||
				   ty == m_tr->i8 || ty == m_tr->u8) {
			LIR_dt(lir) = LIR_JDT_unknown;
		} else if (ty == m_tr->i64 || ty == m_tr->f64 ||
				   ty == m_tr->u64) {
			LIR_dt(lir) = LIR_JDT_wide;
		}
		lir->vA = get_vreg(retval);
	} else {
		LIR_dt(lir) = LIR_JDT_void;
	}
	*ir = IR_next(*ir);
	return (LIR*)lir;
}


//AABBBBBBBB
LIR * IR2Dex::convertGoto(IN OUT IR ** ir, IN IR2D_CTX * cont)
{
	LIRGOTOOp * lir = (LIRGOTOOp*)_ymalloc(sizeof(LIRGOTOOp));
	lir->opcode = LOP_GOTO;
	lir->target = 0xFFFFffff; //backfill after all LIR generated.

	BackFillData * x = (BackFillData*)xmalloc(sizeof(BackFillData));
	x->ir = *ir;
	x->lir = (LIR*)lir;
	m_bf_list.append_tail(x);
	*ir = IR_next(*ir);
	return (LIR*)lir;
}


//ABCCCC
LIR * IR2Dex::convertBranch(bool is_truebr, IN OUT IR ** ir,
							 IN IR2D_CTX * cont)
{
	LIR * lir = NULL;
	IR * det = BR_det(*ir);
	if (IR_type(BIN_opnd1(det)) == IR_CONST &&
		CONST_int_val(BIN_opnd1(det)) == 0) {
		lir = (LIR*)_ymalloc(sizeof(LIRABOp));
		LIR_opcode(lir) = LOP_IFZ;
		LIR_res(lir) = get_vreg(BIN_opnd0(det));
		LIR_op0(lir) = (UInt32)0xFFFFffff; //target.
	} else {
		ASSERT0(IR_type(BIN_opnd1(det)) == IR_PR);
		lir = (LIR*)_ymalloc(sizeof(LIRABCOp));
		LIR_opcode(lir) = LOP_IF;
		LIR_res(lir) = get_vreg(BIN_opnd0(det));
		LIR_op0(lir) = get_vreg(BIN_opnd1(det));
		LIR_op1(lir) = (UInt32)0xFFFFffff; //target.
	}
	ASSERT0(IR_type(BIN_opnd0(det)) == IR_PR);
	if (is_truebr) {
		switch (IR_type(det)) {
		case IR_LT: LIR_dt(lir) = LIR_cond_LT; break;
		case IR_GT: LIR_dt(lir) = LIR_cond_GT; break;
		case IR_LE: LIR_dt(lir) = LIR_cond_LE; break;
		case IR_GE: LIR_dt(lir) = LIR_cond_GE; break;
		case IR_EQ: LIR_dt(lir) = LIR_cond_EQ; break;
		case IR_NE: LIR_dt(lir) = LIR_cond_NE; break;
		default: ASSERT0(0);
		}
	} else {
		switch (IR_type(det)) {
		case IR_LT: LIR_dt(lir) = LIR_cond_GE; break;
		case IR_GT: LIR_dt(lir) = LIR_cond_LE; break;
		case IR_LE: LIR_dt(lir) = LIR_cond_GT; break;
		case IR_GE: LIR_dt(lir) = LIR_cond_LT; break;
		case IR_EQ: LIR_dt(lir) = LIR_cond_NE; break;
		case IR_NE: LIR_dt(lir) = LIR_cond_EQ; break;
		default: ASSERT0(0);
		}
	}

	BackFillData * x = (BackFillData*)xmalloc(sizeof(BackFillData));
	x->ir = *ir;
	x->lir = lir;
	m_bf_list.append_tail(x);
	*ir = IR_next(*ir);
	return (LIR*)lir;
}


//AABBBBBBBB
LIR * IR2Dex::convertSwitch(IN OUT IR ** ir, IN IR2D_CTX * cont)
{
	LIRSwitchOp * lir = (LIRSwitchOp*)_ymalloc(sizeof(LIRSwitchOp));
	IR * vexp = SWITCH_vexp(*ir);
	ASSERT0(IR_type(vexp) == IR_PR);
	lir->value = get_vreg(vexp);
	IR * case_list = SWITCH_case_list(*ir);
	if (case_list != NULL) {
		IR * c = case_list;
		bool is_packed = true; //case value is continuous.
		INT base_val = CONST_int_val(CASE_vexp(c));
		while (c != NULL && IR_next(c) != NULL) {
			if ((CONST_int_val(CASE_vexp(c)) + 1) !=
				CONST_int_val(CASE_vexp(IR_next(c)))) {
				is_packed = false;
				break;
			}
			c = IR_next(c);
		}
		if (is_packed) {
			lir->opcode = LOP_TABLE_SWITCH;
			LIR_dt(lir) = 0x1;
		} else {
			lir->opcode = LOP_LOOKUP_SWITCH;
			LIR_dt(lir) = 0x2;
		}

		c = case_list;
		UINT num_of_case = 0;
		while (c != NULL) {
			num_of_case++;
			c = IR_next(c);
		}

		if (is_packed) {
			lir->data = (USHORT*)_ymalloc((1+1+2)*sizeof(USHORT) +
										   num_of_case*sizeof(UINT));
			LIR_switch_kind(lir) = 0x100;
			LIR_case_num(lir) = num_of_case;
			LIR_packed_switch_base_value(lir) = base_val;
			UINT * pcase_entry = LIR_packed_switch_case_entry(lir);
			for (UINT i = 0; i < num_of_case; i++) {
				pcase_entry[i] = 0xFFFFffff; //case entry is 32bit.
			}
		} else {
			lir->data = (USHORT*)_ymalloc((1+1)*sizeof(USHORT) +
										   num_of_case*sizeof(UINT)*2);
			LIR_switch_kind(lir) = 0x200;
			LIR_case_num(lir) = num_of_case;
			//((BYTE*)data)[4..4+num_of_case*4-1]: the case-value buffer.
			UINT * pcase_value = LIR_sparse_switch_case_value(lir);
			IR * x = case_list;
			for (UINT i = 0; i < num_of_case; i++) {
				pcase_value[i] = CONST_int_val(CASE_vexp(x));
				x = IR_next(x);
			}

			//((BYTE*)data)[4+num_of_case*4, 4+num_of_case*8-1]:
			//	the position of the index table is at current instruction.
			UINT * pcase_entry = LIR_sparse_switch_case_entry(lir);
			for (UINT i = 0; i < num_of_case; i++) {
				pcase_entry[i] = 0xFFFFffff; //case entry is 32bit.
			}
		}
	}

	BackFillData * x = (BackFillData*)xmalloc(sizeof(BackFillData));
	x->ir = *ir;
	x->lir = (LIR*)lir;
	m_bf_list.append_tail(x);
	*ir = IR_next(*ir);
	return (LIR*)lir;
}


LIR * IR2Dex::convert(IN OUT IR ** ir, IN IR2D_CTX * cont)
{
	ASSERT0((*ir)->is_stmt());
	switch (IR_type(*ir)) {
 	case IR_ST:
		return convertStoreVar(ir, cont);
	case IR_STPR:
		return convertStorePR(ir, cont);
	case IR_STARRAY:
		return convertStoreArray(ir, cont);
	case IR_IST:
		return convertIstore(ir, cont);
	case IR_CALL:
		return convertCall(ir, cont);
	case IR_ICALL:
		return convertIcall(ir, cont);
	case IR_GOTO:
		return convertGoto(ir, cont);
	case IR_LABEL:
		*ir = IR_next(*ir);
		break;
	case IR_SWITCH:
		return convertSwitch(ir, cont);
	case IR_TRUEBR:
		return convertBranch(true, ir, cont);
	case IR_FALSEBR:
		return convertBranch(false, ir, cont);
	case IR_RETURN:
		return convertReturn(ir, cont);
	case IR_SELECT:
	case IR_REGION:
		ASSERT(0, ("TODO"));
	default:
		ASSERT0(0);
	}
	return NULL;
}


void IR2Dex::reloc()
{
	for (BackFillData * b = m_bf_list.get_head();
		 b != NULL; b = m_bf_list.get_next()) {
		LIR * l =  b->lir;
		switch (LIR_opcode(l)) {
		case LOP_GOTO:
			{
				IR * g = b->ir;
				bool find;
				UINT idx = m_lab2idx.get(GOTO_lab(g), &find);
				ASSERT0(find);
				((LIRGOTOOp*)l)->target = idx;
			}
			break;
		case LOP_IFZ:
		case LOP_IF:
			{
				bool find;
				UINT idx = m_lab2idx.get(b->ir->get_label(), &find);
				ASSERT0(find);
				if (LIR_opcode(l) == LOP_IFZ) {
					LIR_op0(l) = idx;
				} else {
					LIR_op1(l) = idx;
				}
			}
			break;
		case LOP_TABLE_SWITCH:
			{
				IR * cv = SWITCH_case_list(b->ir);
				UINT * pcase_entry = LIR_packed_switch_case_entry(l);
				UINT num_of_case = LIR_case_num(l);
				for (UINT i = 0; i < num_of_case; i++) {
					ASSERT0(cv);
					bool find;
					UINT idx = m_lab2idx.get(CASE_lab(cv), &find);
					ASSERT0(find);
					pcase_entry[i] = idx;
					cv = IR_next(cv);
				}
			}
			break;
		case LOP_LOOKUP_SWITCH:
			{
				IR * cv = SWITCH_case_list(b->ir);
				UINT * pcase_entry = LIR_sparse_switch_case_entry(l);
				UINT num_of_case = LIR_case_num(l);
				for (UINT i = 0; i < num_of_case; i++) {
					ASSERT0(cv);
					bool find;
					UINT idx = m_lab2idx.get(CASE_lab(cv), &find);
					ASSERT0(find);
					pcase_entry[i] = idx;
					cv = IR_next(cv);
				}
			}
			break;
		default: ASSERT0(0);
		}
	}
}


void IR2Dex::dump_output(List<LIR*> & newlirs, Prno2UINT & prno2v)
{
	if (g_tfile == NULL) { return; }
	if (m_lab2idx.get_elem_count() != 0) {
		INT c;
		fprintf(g_tfile, "\n==== RU:%s, IR2Dex DUMP lab2idx ====",
				m_ru->get_ru_name());
		for (LabelInfo const* li = m_lab2idx.get_first(c);
			 li != NULL; li = m_lab2idx.get_next(c)) {
			dumpLabel(li);
			fprintf(g_tfile, " -->> %d", m_lab2idx.get(li, NULL));
		}
	}
	fprintf(g_tfile, "\n==== RU:%s, DUMP lir list after reloc === vregnum:%d ",
			m_ru->get_ru_name(), prno2v.maxreg + 1);
	/*
	if (prno2v.maxreg >= 0) {
		fprintf(g_tfile, "(");
		for (INT i = prno2v.maxreg + 1 - prno2v.paramnum;
			 i <= prno2v.maxreg; i++) {
			ASSERT0(i >= 0);
			fprintf(g_tfile, "v%d,", i);
		}
		fprintf(g_tfile, ")");
	}
	fprintf(g_tfile, " ====");
	*/

	UINT i = 0;
	for (LIR * lir = newlirs.get_head();
		 lir; lir = newlirs.get_next(), i++) {
		fprintf(g_tfile, "\n%d:", i);
		dump_lir2(lir, m_df, i);
	}
	fprintf(g_tfile, "\n");
	fflush(g_tfile);
}


extern bool g_dd;
void IR2Dex::convert(IR * ir_list, List<LIR*> & newlirs, Prno2UINT & prno2v)
{
	bool dump = false;
	if (dump && g_tfile != NULL) {
		fprintf(g_tfile, "\n\n==== IR->DEX CONVERT %s =====",
				m_ru->get_ru_name());
	}
	IR2D_CTX cont;
	UINT idx = 0;
	while (ir_list != NULL) {
		if (g_tfile != NULL) { fprintf(g_tfile, "\n---"); }

		if (dump) { dump_ir(ir_list, m_dm); }

		if (IR_type(ir_list) == IR_LABEL) {
			m_lab2idx.set(LAB_lab(ir_list), idx);
			ir_list = IR_next(ir_list);
			continue;
		}
		LIR * lir = convert(&ir_list, &cont);
		if (lir == NULL) { continue; }
		newlirs.append_tail(lir);
		idx++;

		if (dump) { dump_lir(lir, m_df, idx); }
	}
	reloc();

	if (dump) {
		dump_output(newlirs, prno2v);
	}

	#ifdef _DEBUG_
	if (g_dd) {
		FILE * log = fopen("ir2dex.log", "a+");
		fprintf(log, "\n== %s ==", m_ru->get_ru_name());
		FILE * t = g_tfile;
		g_tfile = log;
		UINT j = 0;
		for (LIR * lir = newlirs.get_head(); lir; lir = newlirs.get_next()) {
			dump_lir(lir, m_df, -1); //do not use j as idx, since it will mess up
									 //the diff with other log.
			j++;
		}
		g_tfile = t;
		fclose(log);
	}
	#endif
	return;
}

