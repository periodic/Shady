#include <dr_ir_macros.h>

#include "defines.h"
#include "shady_util.h"
#include "inst_readwrite.h"

static void event_exit();
static dr_emit_flags_t event_basic_block(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating);

static void instrument_read(void * drcontext, instrlist_t * bb, instr_t * orig);
static void instrument_write(void * drcontext, instrlist_t * bb, instr_t * orig);

static void read_callback(app_pc addr, uint i);
static void write_callback(app_pc addr, uint i);

static uint read_count = 0;
static uint write_count = 0;

void
readwrite_init(client_id_t id)
{
    dr_register_exit_event(event_exit);
    dr_register_bb_event(event_basic_block);
}

static void
event_exit()
{
    dr_printf("Reads: %u, Writes: %u\n", read_count, write_count);
}

static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag,
        instrlist_t *bb, bool for_trace, bool translating)
{
    instr_t *instr, *next_instr;

    /* count the number of instructions in this block */
    for (instr = instrlist_first(bb); instr != NULL; instr = next_instr) {
        next_instr = instr_get_next(instr);

        if (instr_reads_memory(instr)) {
            instrument_read(drcontext, bb, instr);
        }
        if (instr_writes_memory(instr)) {
            instrument_write(drcontext, bb, instr);
        }
    }

    return DR_EMIT_DEFAULT;
}


static void
read_callback(app_pc addr, uint i)
{
    instr_t instr;

    // Increment the counter
    read_count++;

    // Get the drcontext
    void* drcontext = dr_get_current_drcontext();

    // Load the memory context.
    dr_mcontext_t mc;
    mc.size = sizeof(mc);
    mc.flags = DR_MC_CONTROL | DR_MC_INTEGER;
    dr_get_mcontext(drcontext, &mc);

    // Get the instruction
    instr_init(drcontext, &instr);
    decode(drcontext, addr, &instr);

    // Get memory address
    /*
    ptr_uint_t unaligned = (ptr_uint_t) opnd_compute_address(instr_get_src(&instr, i), &mc);

    ptr_uint_t aligned = unaligned - unaligned % 8;

    int* accessed_mem = (int*) aligned;
    */
    // Get word-aligned address.
    app_pc accessed_mem = align_ptr(opnd_compute_address(instr_get_src(&instr, i), &mc));
    int mem_val = *(int*)accessed_mem;

    if (mem_val == SENTINEL) {
        if (is_stack_address(&mc, accessed_mem)) {
            dr_printf("Stack read of %p => 0x%x (pc = %p, sp = %p, bp = %p)\n", accessed_mem, *(int*)accessed_mem, addr, mc.xsp, mc.xbp);
        } else {
            dr_printf("Read of %p => 0x%x (pc = %p, sp = %p, bp = %p)\n", accessed_mem, *(int*)accessed_mem, addr, mc.xsp, mc.xbp);
        }
    }
}

static void
write_callback(app_pc addr, uint i)
{

    instr_t instr;

    // Increment the counter.
    write_count++;

    // Get the drcontext
    void* drcontext = dr_get_current_drcontext();

    // Load the memory context.
    dr_mcontext_t mc;
    mc.size = sizeof(mc);
    mc.flags = DR_MC_ALL;
    dr_get_mcontext(drcontext, &mc);

    // Get the instruction
    instr_init(drcontext, &instr);
    decode(drcontext, addr, &instr);

    // Get word-aligned address.
    app_pc accessed_mem = align_ptr(opnd_compute_address(instr_get_dst(&instr, i), &mc));

    if (* (int*)accessed_mem == SENTINEL) {
        if (is_stack_address(&mc, accessed_mem)) {
            dr_printf("Stack write of %p => 0x%x (pc = %p, sp = %p, bp = %p)\n", accessed_mem, *(int*)accessed_mem, addr, mc.xsp, mc.xbp);
        } else {
            dr_printf("Write of %p => 0x%x (pc = %p, sp = %p, bp = %p)\n", accessed_mem, *(int*)accessed_mem, addr, mc.xsp, mc.xbp);
            // Redirect execution.
            app_pc next = (app_pc)decode_next_pc(drcontext, (byte *)addr);
            dr_printf("Skipping write, redirecting from %p to %p\n", addr, next);
            mc.pc = next;
            dr_redirect_execution(&mc);
        }
    }


}

static void
instrument_read(void * drcontext, instrlist_t * bb, instr_t * orig)
{
    uint i;
    opnd_t o;

    for (i = 0; i < instr_num_srcs(orig); i++) {
        o = instr_get_src(orig, i);


        if (opnd_is_memory_reference(o)) {
            dr_insert_clean_call(
                    drcontext,
                    bb,
                    orig,
                    (void *)read_callback,
                    false /*no fp save*/,
                    2,
                    OPND_CREATE_INTPTR(instr_get_app_pc(orig)),
                    OPND_CREATE_INT32(i)
                    );
        }

    }
}

static void
instrument_write(void * drcontext, instrlist_t * bb, instr_t * orig)
{
    uint i;
    opnd_t o;

    for (i = 0; i < instr_num_dsts(orig); i++) {
        o = instr_get_dst(orig, i);

        if (opnd_is_memory_reference(o)) {
            dr_insert_clean_call(
                    drcontext,
                    bb,
                    orig,
                    (void *)write_callback,
                    false /*no fp save*/,
                    2,
                    OPND_CREATE_INTPTR(instr_get_app_pc(orig)),
                    OPND_CREATE_INT32(i)
                    );
        }

    }
}
