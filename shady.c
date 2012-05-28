#include <string.h>
#include "dr_api.h"
#include "dr_ir_macros.h"

#define REDZONE 0xdeadbeef

static void event_exit(void);
static dr_emit_flags_t event_basic_block(void *drcontext, void *tag,
        instrlist_t *bb, bool for_trace, bool translating);

#define char_buf_size 128
static char* opnd_string(opnd_t);
static void instr_print(void*, instr_t *);
static bool instr_is_stack_op(instr_t *instr);
static instrlist_t * create_mem_checker(void* drcontext, instr_t * orig);
static void create_mem_branch(void* drcontext, instr_t * orig, instrlist_t * checker);

#ifdef WINDOWS
# define DISPLAY_STRING(msg) dr_messagebox(msg)
#else
# define DISPLAY_STRING(msg) dr_printf("%s\n", msg)
#endif

//void *as_built_lock;

DR_EXPORT void
dr_init(client_id_t id)
{
    dr_register_exit_event(event_exit);
    dr_register_bb_event(event_basic_block);

    //as_built_lock = dr_mutex_create();
}

static void
event_exit()
{
    /* free mutex */
    //dr_mutex_destroy(as_built_lock);
    DISPLAY_STRING("Exit.");
}

static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag,
        instrlist_t *bb, bool for_trace, bool translating)
{
    /* count the number of instructions in this block */
    instr_t *instr;
    for (instr = instrlist_first(bb); instr != NULL; instr = instr_get_next(instr))
    {

        if (instr_reads_memory(instr) && ! instr_is_stack_op(instr)) {
            DISPLAY_STRING("Got memory read.");

            // Create new basic block.  Does creation add it to the program?
            instrlist_t *checkMem = create_mem_checker(drcontext, instr);

            // insert a compare and jump before the offending instruction.
            create_mem_branch(drcontext, instr, checkMem);

        }
        if (instr_writes_memory(instr) && ! instr_is_stack_op(instr)) {
            DISPLAY_STRING("Memory write encountered.");
            // TODO: Insert checks
        }
    }
    return DR_EMIT_DEFAULT;
}

static instrlist_t *
create_mem_checker(void* drcontext, instr_t * orig)
{
    instrlist_t * newbb = instrlist_create(drcontext);
    /* The mem-checker should look like the following:
     *
     * clean-call in to the main mem-checker.
     * if OK, jump to orig
     * store clean-call result in the target of the instruction.
     * jump to orig + 1 instr
     */

    return newbb;
}

static void
create_mem_branch(void* drcontext, instr_t * orig, instrlist_t * checker)
{
    /* The mem-checker branch should look as follows:
     *
     * comp mem MAGIC
     * jez  checker
     * orig
     */

    return;
}

static void
memory_operation(void *drcontext, instr_t *instr)
{
    instr_print(drcontext, instr);
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
    } else if (opnd_is_near_pc(opnd)) {
        strcpy(buf, "Near PC");
    } else if (opnd_is_far_pc(opnd)) {
        strcpy(buf, "Far PC");
    } else if (opnd_is_instr(opnd)) {
        sprintf(buf, "Instr: %p", instr_get_app_pc(opnd_get_instr(opnd)));
    } else if (opnd_is_base_disp(opnd)) {
        sprintf(buf, "Disp (%s, 0x%x, 0x%x) %p", get_register_name(opnd_get_base(opnd)), opnd_get_disp(opnd), opnd_get_scale(opnd), opnd_get_addr(opnd));
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
    opnd_t op;
    int i;
    int count;

    static char buf[char_buf_size];
    instr_disassemble_to_buffer(drcontext, instr, buf, char_buf_size);
    DISPLAY_STRING(buf);

    count = instr_num_srcs(instr);
    for (i = 0; i < count; i++) {
        op = instr_get_src(instr, i);
        dr_printf("Src %d: %s\n", i, opnd_string(op));
    }
    count = instr_num_dsts(instr);
    for (i = 0; i < count; i++) {
        op = instr_get_dst(instr, i);
        dr_printf("Dst %d: %s\n", i, opnd_string(op));
    }
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
