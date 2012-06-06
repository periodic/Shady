#include <string.h>
#include "dr_api.h"
#include "dr_ir_macros.h"

#define SENTINEL 0xdeadbeef

static void event_exit(void);
static dr_emit_flags_t event_basic_block(void *drcontext, void *tag,
        instrlist_t *bb, bool for_trace, bool translating);

#define char_buf_size 128
static char* opnd_string(opnd_t);
static void instr_print(void*, instr_t *);
static bool instr_is_stack_op(instr_t *instr);
static void instrument_read(void * drcontext, instrlist_t * bb, instr_t * orig);

//void *as_built_lock;
static int cleancall_count = 0;

DR_EXPORT void
dr_init(client_id_t id)
{
    dr_register_exit_event(event_exit);
    dr_register_bb_event(event_basic_block);

    disassemble_set_syntax(DR_DISASM_INTEL);
    //as_built_lock = dr_mutex_create();
}

static void
event_exit()
{
    /* free mutex */
    //dr_mutex_destroy(as_built_lock);
    dr_printf("Clean calls: %d\n", cleancall_count);
    dr_printf("Exit.");
}

static void
read_callback(app_pc addr, uint i)
{

    instr_t instr;

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
    int* accessed_mem = (opnd_compute_address(instr_get_src(&instr, i), &mc));

    // Get word-aligned address.
    //accessed_mem = accessed_mem - (accessed_mem % 4);
    if (* accessed_mem == SENTINEL) {
        dr_printf("Got read of %p => 0x%x (pc = %p)\n", accessed_mem, *accessed_mem, addr);
    }

    cleancall_count++;
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
    }

    return DR_EMIT_DEFAULT;
}

static void
instrument_read(void * drcontext, instrlist_t * bb, instr_t * orig)
{
    int i;
    opnd_t o;
    app_pc pc = instr_get_app_pc(orig);

    //instr_print(drcontext, orig);
    //dr_printf("    (PC = %p)\n", pc);

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
                    OPND_CREATE_INT64(i)
                    );
        }

    }
}


static char *
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

static void
instr_print(void* drcontext, instr_t *instr)
{
    static char buf[char_buf_size];
    if (instr_disassemble_to_buffer(drcontext, instr, buf, char_buf_size) > 0)
        dr_printf(buf);
}

static bool
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
