#include "inst_readwrite.h"

#include <hashtable.h>
#include <dr_ir_macros.h>

#include "defines.h"
#include "shady_util.h"

#define MAX_TRACE_ERRORS 1

static void event_exit();
static dr_emit_flags_t event_basic_block(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating);

static void instrument_read(void * drcontext, instrlist_t * bb, instr_t * orig);
static void instrument_write(void * drcontext, instrlist_t * bb, instr_t * orig);

static void read_callback(app_pc addr, uint i);
static void write_callback(app_pc addr, uint i);

static uint read_count = 0;
static uint write_count = 0;

static void skip_instruction(void* drcontext, dr_mcontext_t* mc, app_pc addr);
static void skip_read(void* drcontext, dr_mcontext_t* mc, app_pc addr, instr_t* instr);
static void skip_write(void* drcontext, dr_mcontext_t* mc, app_pc addr, instr_t* instr);
static bool try_read(app_pc ptr, int* val);
static bool instr_is_str_op(instr_t* instr);

/* Hash table stuff. */
static int get_read_value(app_pc addr);

static hashtable_t read_return_values[1];
/* ----------------- */

void
readwrite_init(client_id_t id)
{
    dr_register_exit_event(event_exit);
    dr_register_bb_event(event_basic_block);

    hashtable_init_ex(read_return_values,
            4, /* 16 buckets initially */
            HASH_INTPTR, /* keys are ptrs */
            0, /* don't duplicate string keys */
            0, /* don't synchronize */
            NULL, /* no free function (TODO) */
            NULL, /* use default key hash fn */
            NULL /* use default key cmp fn */
            );
}

static void
event_exit()
{
    DEBUG("Reads: %u, Writes: %u\n", read_count, write_count);
}

static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag,
        instrlist_t *bb, bool for_trace, bool translating)
{
    instr_t *instr, *next_instr;

    //DEBUG("Instrumenting block %p.\n", tag);

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

    return DR_EMIT_STORE_TRANSLATIONS;
}


static void
read_callback(app_pc addr, uint i)
{
    instr_t instr;

    TRACE("Read callback at %p.\n", addr);

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
    app_pc accessed_mem = align_ptr(opnd_compute_address(instr_get_src(&instr, i), &mc));

    int accessed_val;

    if (! try_read(accessed_mem, &accessed_val)) {
        DEBUG("Read of unaccessable value at %p (pc = %p, sp = %p, bp = %p)\n", accessed_mem, addr, mc.xsp, mc.xbp);
        read_count++;
        //skip_read(drcontext, &mc, addr, &instr);
    } else 
    if (accessed_val == SENTINEL) {
        // Increment the counter
        DEBUG("Read of sentinel at %p (pc = %p, sp = %p, bp = %p)\n", accessed_mem, addr, mc.xsp, mc.xbp);
        read_count++;
        //skip_read(drcontext, &mc, addr, &instr);
    }

    TRACE("Read callback complete for %p.\n", addr);
}

static void
write_callback(app_pc addr, uint i)
{

    instr_t instr;

    TRACE("Write callback at %p.\n", addr);

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

    int accessed_val;

    if (! try_read(accessed_mem, &accessed_val)) {
        DEBUG("Write of unaccessable value at %p (pc = %p, sp = %p, bp = %p)\n", accessed_mem, addr, mc.xsp, mc.xbp);
        write_count++;
        //skip_write(drcontext, &mc, addr, &instr);
    } else 
    if (accessed_val == SENTINEL) {
        // Increment the counter.
        DEBUG("Write of sentinel at %p (pc = %p, sp = %p, bp = %p)\n", accessed_mem, addr, mc.xsp, mc.xbp);
        write_count++;
        //skip_write(drcontext, &mc, addr, &instr);
    }

    TRACE("Write callback complete for %p.\n", addr);
}

static void
instrument_read(void * drcontext, instrlist_t * bb, instr_t * orig)
{
    uint i;
    opnd_t o;

    for (i = 0; i < instr_num_srcs(orig); i++) {
        o = instr_get_src(orig, i);


        if (opnd_is_memory_reference(o)
            && ! instr_is_str_op(orig)) {
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


        if (opnd_is_memory_reference(o)
            && ! instr_is_str_op(orig)) {
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

static int
get_read_value(app_pc addr)
{
    void *lookup = hashtable_lookup(read_return_values, addr);

    hashtable_add_replace(read_return_values, addr, lookup + 1);

    return (int) lookup;
}

static void
skip_instruction(void* drcontext, dr_mcontext_t* mc, app_pc addr)
{
    app_pc next = (app_pc)decode_next_pc(drcontext, addr);
    mc->pc = next;
    dr_redirect_execution(mc);
}

static void
skip_read(void* drcontext, dr_mcontext_t* mc, app_pc addr, instr_t * instr)
{
    if (instr_num_dsts(instr) > 0) {
        opnd_t dst = instr_get_dst(instr, 0);
        if (opnd_is_reg(dst)) {
            // set register value.
            int val = get_read_value(addr);
            reg_set_value(opnd_get_reg(dst), mc, val);

            // Skip it.
            DEBUG("Replacing read with %i.\n", val);
            skip_instruction(drcontext, mc, addr);
        }
    }
}

static void
skip_write(void* drcontext, dr_mcontext_t* mc, app_pc addr, instr_t * instr)
{
    DEBUG("Skipping write.\n");
    skip_instruction(drcontext, mc, addr);
}

static bool
try_read(app_pc ptr, int* val)
{
    size_t bytes_read;
    return dr_safe_read(ptr, 4, (void*) val, &bytes_read);
}

static bool
instr_is_str_op(instr_t* instr)
{
    int opcode = instr_get_opcode(instr);

    return (opcode == OP_ins
         || opcode == OP_rep_ins
         || opcode == OP_outs
         || opcode == OP_rep_outs
         || opcode == OP_movs
         || opcode == OP_rep_movs
         || opcode == OP_stos
         || opcode == OP_rep_stos
         || opcode == OP_lods
         || opcode == OP_rep_lods
         || opcode == OP_cmps
         || opcode == OP_rep_cmps
         || opcode == OP_repne_cmps
         || opcode == OP_scas
         || opcode == OP_rep_scas
         || opcode == OP_repne_scas);
}
