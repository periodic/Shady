#include <string.h>
#include "shady_util.h"

bool
is_stack_address(dr_mcontext_t * mcontext, app_pc addr)
{
    /* This currently contains an egregious hack to detect the base of the
     * stack.  Because I don't know how to get that information I just added 1
     * MB onto the base-pointer. */
    return ((ptr_int_t)addr < mcontext->xbp + (1<<20) && (ptr_int_t)addr > mcontext->xsp - 32);
}

app_pc
align_ptr(app_pc addr)
{
    ptr_int_t unaligned = (ptr_int_t) addr;
    // Get word-aligned address.
    return (app_pc) (unaligned - unaligned % sizeof(ptr_int_t));
}

char *
opnd_string(opnd_t opnd)
{
    static char buf[char_buf_size];

    if (opnd_is_null(opnd)) {
        // Skip it.
    } else if (opnd_is_immed_int(opnd)) {
        strcpy(buf, "Immed int:");
    } else if (opnd_is_immed_float(opnd)) {
        strcpy(buf, "Immed float:");
    } else if (opnd_is_base_disp(opnd)) {
        sprintf(buf, "Disp (%s, 0x%x, 0x%x) %p", get_register_name(opnd_get_base(opnd)), opnd_get_disp(opnd), opnd_get_scale(opnd), opnd_get_addr(opnd));
    } else if (opnd_is_near_pc(opnd)) {
        strcpy(buf, "Near PC");
    } else if (opnd_is_far_pc(opnd)) {
        strcpy(buf, "Far PC");
    } else if (opnd_is_instr(opnd)) {
        sprintf(buf, "Instr: %p", instr_get_app_pc(opnd_get_instr(opnd)));
    } else if (opnd_is_abs_addr(opnd)) {
        sprintf(buf, "Addr: %p", opnd_get_addr(opnd));
    } else if (opnd_is_memory_reference(opnd)) {
        sprintf(buf, "Mem: %p", opnd_get_addr(opnd));
    } else if (opnd_is_reg(opnd)) {
        strncpy(buf, get_register_name(opnd_get_reg(opnd)), char_buf_size);
    } else {
        strcpy(buf, "Unknown operand type.");
    }

    return buf;
}

void
instr_print(void* drcontext, instr_t *instr)
{
    static char buf[char_buf_size];
    if (instr_disassemble_to_buffer(drcontext, instr, buf, char_buf_size) > 0)
        dr_printf(buf);
}

bool
instr_is_stack_op(instr_t *instr)
{
    opnd_t op;
    int i;
    int count;
    count = instr_num_srcs(instr);
    for (i = 0; i < count; i++) {
        op = instr_get_src(instr, i);
        if (opnd_is_reg(op)
                && (opnd_get_reg(op) == DR_REG_RSP
                    || opnd_get_reg(op) == DR_REG_ESP))
            return true;
    }
    count = instr_num_dsts(instr);
    for (i = 0; i < count; i++) {
        op = instr_get_dst(instr, i);
        if (opnd_is_reg(op)
                && (opnd_get_reg(op) == DR_REG_RSP
                    || opnd_get_reg(op) == DR_REG_ESP))
            return true;
    }

    return false;
}
