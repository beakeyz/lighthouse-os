#include "lightos/proc/ipc/pipe/shared.h"
#include <lightos/lib/lightos.h>
#include <lightos/proc/ipc/pipe/pipe.h>

/*
 * Pipe that communicates input packets
 * Singleduplex, broadcast
 */
lightos_pipe_t input_pipe;
/*
 * Pipe used to Send graphics requests
 * Fullduplex, reverse broadcast (xD)
 */
lightos_pipe_t graphics_pipe;

/*!
 * @brief: Sets up default communication measures with dispmgr
 *
 * When a process requires this library and it's loading fails, the process sadly can't run, which means
 * this library can be considered a 'critical system component' or some shit like that.
 *
 * TODO: We need to figure out how we can let kterm emulate dispmgr behaviour, so we can simply let graphical
 * applications use dispmgr libraries, but with a dynamic backend
 */
LIGHTENTRY int lib_entry()
{
    return 0;
}
