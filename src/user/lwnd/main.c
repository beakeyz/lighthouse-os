#include "lightos/api/ipc/pipe.h"
#include "lightos/proc/ipc/pipe/pipe.h"
#include "lwindow/api.h"
#include <lightos/api/handle.h>

static lightos_pipe_t __main_pipe;

int main(HANDLE self)
{
    /* Setup the upi transfer pipe */
    init_lightos_pipe(&__main_pipe, "main", NULL, 0, sizeof(lwindow_packet_t));
    /* Get a handle to the current video device */
    /*  */
    return 0;
}
