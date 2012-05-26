#include "dr_api.h"
#include "dr_ir_macros.h"

#define REDZONE 0xdeadbeef

static void event_exit(void);
static dr_emit_flags_t event_basic_block(void *drcontext, void *tag, 
        instrlist_t *bb, bool for_trace, bool translating);

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
        if (instr_reads_memory(instr)) {
            // Insert check after load.
            
            opnd_t src = instr_get_src(instr, 0); // can movs have mor than one source?
            opnd_t dst = instr_get_target(instr);

            if (opnd_is_memory_reference(dst)) {
                DISPLAY_STRING("Inserting new instruction:");
                instr_t *newInstr = INSTR_CREATE_test(drcontext, opnd_create_immed_int(REDZONE, OPSZ_PTR), dst); // Is this the right opsz?
                instrlist_postinsert(bb, instr, newInstr);
            }
        }
        if (instr_writes_memory(instr)) {
            DISPLAY_STRING("Memory write encountered.");
            // Insert check before write.
        }
    }
    return DR_EMIT_DEFAULT;
}
