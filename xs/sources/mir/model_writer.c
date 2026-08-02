/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "model_internal.h"

#include <stdint.h>
#include <stdio.h>

static XsMirStatus write_signature(const XsMirFunction *function, FILE *stream, XsMirError *error)
{
    if(fprintf(stream, "%s fn %s(", function->is_definition ? "define" : "declare", function->qualified_name) < 0)
        return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR function signature");
    for(size_t parameter = 0; parameter < function->parameter_count; ++parameter)
    {
        if(parameter != 0 && fputs(", ", stream) == EOF)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR function parameter separator");
        if(fputs(xs_lil_type_name(function->parameters[parameter]), stream) == EOF)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR function parameter type");
    }
    if(fprintf(stream, ") -> %s", xs_lil_type_name(function->return_type)) < 0)
        return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR function return type");
    return XS_MIR_OK;
}

static XsMirStatus write_terminator(const XsMirBlock *block, FILE *stream, XsMirError *error)
{
    switch(block->terminator.kind)
    {
    case XS_MIR_TERMINATOR_RETURN:
        if(block->terminator.has_value)
        {
            if(fprintf(stream, "  return %u\n", block->terminator.value) < 0)
                return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR return terminator");
            return XS_MIR_OK;
        }
        if(fputs("  return\n", stream) == EOF)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR return terminator");
        return XS_MIR_OK;
    case XS_MIR_TERMINATOR_GOTO:
        if(fprintf(stream, "  goto bb%u\n", block->terminator.target) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR goto terminator");
        return XS_MIR_OK;
    case XS_MIR_TERMINATOR_BRANCH:
        if(fprintf(stream, "  branch v%u, bb%u, bb%u\n", block->terminator.value, block->terminator.target,
                   block->terminator.else_target) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR branch terminator");
        return XS_MIR_OK;
    case XS_MIR_TERMINATOR_UNREACHABLE:
        if(fputs("  unreachable\n", stream) == EOF)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR unreachable terminator");
        return XS_MIR_OK;
    case XS_MIR_TERMINATOR_PANIC:
        if(fputs("  panic\n", stream) == EOF)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR panic terminator");
        return XS_MIR_OK;
    case XS_MIR_TERMINATOR_NONE:
        if(fputs("  <missing terminator>\n", stream) == EOF)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR missing terminator marker");
        return XS_MIR_OK;
    }
    return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "unknown MIR terminator kind");
}

static const char *local_kind_name(XsMirLocalKind kind)
{
    switch(kind)
    {
    case XS_MIR_LOCAL_PARAMETER:
        return "param";
    case XS_MIR_LOCAL_VARIABLE:
        return "var";
    case XS_MIR_LOCAL_TEMPORARY:
        return "temp";
    }
    return "local";
}

static XsMirStatus write_instruction(const XsMirInstruction *instruction, FILE *stream, XsMirError *error)
{
    switch(instruction->kind)
    {
    case XS_MIR_INSTRUCTION_CONST_I64:
        if(fprintf(stream, "  v%u = const.i64 %lld\n", instruction->result, (long long)instruction->immediate_i64) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR const.i64 instruction");
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_CONST_I32:
        if(fprintf(stream, "  v%u = const.i32 %lld\n", instruction->result, (long long)instruction->immediate_i64) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR const.i32 instruction");
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_CONST_BOOL:
        if(fprintf(stream, "  v%u = const.bool %s\n", instruction->result,
                   instruction->immediate_i64 != 0 ? "true" : "false") < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR const.bool instruction");
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_ADD_I64:
        if(fprintf(stream, "  v%u = add.i64 v%u, v%u\n", instruction->result, instruction->operand_left,
                   instruction->operand_right) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR add.i64 instruction");
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_SUB_I64:
        if(fprintf(stream, "  v%u = sub.i64 v%u, v%u\n", instruction->result, instruction->operand_left,
                   instruction->operand_right) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR sub.i64 instruction");
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_MUL_I64:
        if(fprintf(stream, "  v%u = mul.i64 v%u, v%u\n", instruction->result, instruction->operand_left,
                   instruction->operand_right) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR mul.i64 instruction");
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_DIV_I64:
        if(fprintf(stream, "  v%u = div.i64 v%u, v%u\n", instruction->result, instruction->operand_left,
                   instruction->operand_right) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR div.i64 instruction");
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_REM_I64:
        if(fprintf(stream, "  v%u = rem.i64 v%u, v%u\n", instruction->result, instruction->operand_left,
                   instruction->operand_right) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR rem.i64 instruction");
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_AND_I64:
        if(fprintf(stream, "  v%u = and.i64 v%u, v%u\n", instruction->result, instruction->operand_left,
                   instruction->operand_right) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR and.i64 instruction");
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_OR_I64:
        if(fprintf(stream, "  v%u = or.i64 v%u, v%u\n", instruction->result, instruction->operand_left,
                   instruction->operand_right) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR or.i64 instruction");
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_XOR_I64:
        if(fprintf(stream, "  v%u = xor.i64 v%u, v%u\n", instruction->result, instruction->operand_left,
                   instruction->operand_right) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR xor.i64 instruction");
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_SHL_I64:
        if(fprintf(stream, "  v%u = shl.i64 v%u, v%u\n", instruction->result, instruction->operand_left,
                   instruction->operand_right) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR shl.i64 instruction");
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_SHR_I64:
        if(fprintf(stream, "  v%u = shr.i64 v%u, v%u\n", instruction->result, instruction->operand_left,
                   instruction->operand_right) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR shr.i64 instruction");
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_EQ_I64:
        if(fprintf(stream, "  v%u = eq.i64 v%u, v%u\n", instruction->result, instruction->operand_left,
                   instruction->operand_right) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR eq.i64 instruction");
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_NE_I64:
        if(fprintf(stream, "  v%u = ne.i64 v%u, v%u\n", instruction->result, instruction->operand_left,
                   instruction->operand_right) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR ne.i64 instruction");
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_LT_I64:
        if(fprintf(stream, "  v%u = lt.i64 v%u, v%u\n", instruction->result, instruction->operand_left,
                   instruction->operand_right) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR lt.i64 instruction");
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_LE_I64:
        if(fprintf(stream, "  v%u = le.i64 v%u, v%u\n", instruction->result, instruction->operand_left,
                   instruction->operand_right) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR le.i64 instruction");
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_GT_I64:
        if(fprintf(stream, "  v%u = gt.i64 v%u, v%u\n", instruction->result, instruction->operand_left,
                   instruction->operand_right) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR gt.i64 instruction");
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_GE_I64:
        if(fprintf(stream, "  v%u = ge.i64 v%u, v%u\n", instruction->result, instruction->operand_left,
                   instruction->operand_right) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR ge.i64 instruction");
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_ADD_I32:
        if(fprintf(stream, "  v%u = add.i32 v%u, v%u\n", instruction->result, instruction->operand_left,
                   instruction->operand_right) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR add.i32 instruction");
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_SUB_I32:
        if(fprintf(stream, "  v%u = sub.i32 v%u, v%u\n", instruction->result, instruction->operand_left,
                   instruction->operand_right) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR sub.i32 instruction");
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_MUL_I32:
        if(fprintf(stream, "  v%u = mul.i32 v%u, v%u\n", instruction->result, instruction->operand_left,
                   instruction->operand_right) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR mul.i32 instruction");
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_DIV_I32:
        if(fprintf(stream, "  v%u = div.i32 v%u, v%u\n", instruction->result, instruction->operand_left,
                   instruction->operand_right) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR div.i32 instruction");
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_REM_I32:
        if(fprintf(stream, "  v%u = rem.i32 v%u, v%u\n", instruction->result, instruction->operand_left,
                   instruction->operand_right) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR rem.i32 instruction");
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_AND_I32:
        if(fprintf(stream, "  v%u = and.i32 v%u, v%u\n", instruction->result, instruction->operand_left,
                   instruction->operand_right) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR and.i32 instruction");
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_OR_I32:
        if(fprintf(stream, "  v%u = or.i32 v%u, v%u\n", instruction->result, instruction->operand_left,
                   instruction->operand_right) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR or.i32 instruction");
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_XOR_I32:
        if(fprintf(stream, "  v%u = xor.i32 v%u, v%u\n", instruction->result, instruction->operand_left,
                   instruction->operand_right) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR xor.i32 instruction");
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_SHL_I32:
        if(fprintf(stream, "  v%u = shl.i32 v%u, v%u\n", instruction->result, instruction->operand_left,
                   instruction->operand_right) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR shl.i32 instruction");
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_SHR_I32:
        if(fprintf(stream, "  v%u = shr.i32 v%u, v%u\n", instruction->result, instruction->operand_left,
                   instruction->operand_right) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR shr.i32 instruction");
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_EQ_I32:
        if(fprintf(stream, "  v%u = eq.i32 v%u, v%u\n", instruction->result, instruction->operand_left,
                   instruction->operand_right) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR eq.i32 instruction");
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_NE_I32:
        if(fprintf(stream, "  v%u = ne.i32 v%u, v%u\n", instruction->result, instruction->operand_left,
                   instruction->operand_right) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR ne.i32 instruction");
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_LT_I32:
        if(fprintf(stream, "  v%u = lt.i32 v%u, v%u\n", instruction->result, instruction->operand_left,
                   instruction->operand_right) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR lt.i32 instruction");
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_LE_I32:
        if(fprintf(stream, "  v%u = le.i32 v%u, v%u\n", instruction->result, instruction->operand_left,
                   instruction->operand_right) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR le.i32 instruction");
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_GT_I32:
        if(fprintf(stream, "  v%u = gt.i32 v%u, v%u\n", instruction->result, instruction->operand_left,
                   instruction->operand_right) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR gt.i32 instruction");
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_GE_I32:
        if(fprintf(stream, "  v%u = ge.i32 v%u, v%u\n", instruction->result, instruction->operand_left,
                   instruction->operand_right) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR ge.i32 instruction");
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_NOT_BOOL:
        if(fprintf(stream, "  v%u = not.bool v%u\n", instruction->result, instruction->operand_left) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR not.bool instruction");
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_CALL:
        if(instruction->result == UINT32_MAX)
        {
            if(fprintf(stream, "  call %s(", instruction->callee) < 0)
                return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR call instruction");
        }
        else if(fprintf(stream, "  v%u = call %s(", instruction->result, instruction->callee) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR call instruction");
        for(size_t index = 0; index < instruction->argument_count; ++index)
        {
            if(fprintf(stream, "%sv%u", index == 0 ? "" : ", ", instruction->arguments[index]) < 0)
                return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR call argument");
        }
        if(fprintf(stream, ")\n") < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not finish MIR call instruction");
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_LOAD:
        if(fprintf(stream, "  v%u = load place%u\n", instruction->result, instruction->place) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR load instruction");
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_STORE:
        if(fprintf(stream, "  store place%u, v%u\n", instruction->place, instruction->operand_left) < 0)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR store instruction");
        return XS_MIR_OK;
    }
    return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "unknown MIR instruction kind");
}

XsMirStatus xs_mir_module_write_text(const XsMirModule *module, FILE *stream, XsMirError *error)
{
    xs_mir_clear_error(error);
    if(module == nullptr || stream == nullptr)
        return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "MIR module and stream are required");
    if(fprintf(stream, "mir module %s\n", module->name) < 0)
        return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR module header");
    for(size_t i = 0; i < module->function_count; ++i)
    {
        const XsMirFunction *function = &module->functions[i];
        XsMirStatus status = write_signature(function, stream, error);
        if(status != XS_MIR_OK)
            return status;
        if(!function->is_definition)
        {
            if(fputc('\n', stream) == EOF)
                return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not finish MIR declaration");
            continue;
        }
        if(fputs(" {\n", stream) == EOF)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not open MIR function body");
        for(size_t local = 0; local < function->local_count; ++local)
        {
            const XsMirLocal *entry = &function->locals[local];
            if(fprintf(stream, "local %zu %s %s%s: %s\n", local, local_kind_name(entry->kind),
                       entry->is_mutable ? "mut " : "", entry->name, xs_lil_type_name(entry->type)) < 0)
                return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR local");
        }
        for(size_t block_index = 0; block_index < function->block_count; ++block_index)
        {
            const XsMirBlock *block = function->blocks[block_index];
            if(fprintf(stream, "bb%u %s:\n", block->id, block->label) < 0)
                return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not write MIR block label");
            for(size_t instruction = 0; instruction < block->instruction_count; ++instruction)
            {
                status = write_instruction(&block->instructions[instruction], stream, error);
                if(status != XS_MIR_OK)
                    return status;
            }
            status = write_terminator(block, stream, error);
            if(status != XS_MIR_OK)
                return status;
        }
        if(fputs("}\n", stream) == EOF)
            return xs_mir_set_error(error, XS_MIR_IO_ERROR, "could not close MIR function body");
    }
    return XS_MIR_OK;
}
