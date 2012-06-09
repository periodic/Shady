#include <dr_api.h>

#include "inst_malloc.h"
#include "inst_readwrite.h"

static void event_exit(void);
DR_EXPORT void
dr_init(client_id_t id)
{
    //malloc_init(id);
    readwrite_init(id);
    dr_register_exit_event(event_exit);

    // Set intel syntax over the default SUN syntax.
    disassemble_set_syntax(DR_DISASM_INTEL);
}

static void
event_exit()
{
    dr_printf("Exit.\n");
}
