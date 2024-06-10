#include "sys/types.h"
#include <LibGfx/include/driver.h>
#include <LibGfx/include/lgfx.h>

lwindow_t game_window;

#define WINDOW_WIDTH 300
#define WINDOW_HEIGHT 300

/*!
 * @brief: Minesweeper entrypoint
 *
 * - Initialize gfx lib
 * - Create a window
 * - Enter game loop
 */
int main()
{
    BOOL res;

    /* Create a window for our game */
    res = request_lwindow(&game_window, WINDOW_WIDTH, WINDOW_HEIGHT, NULL);

    if (!res)
        return -1;

    /* Initialize our resources */

    /* Render a start screen */

    /* Enter the game loop and render shit */

    /* Close the window like a good process */
    close_lwindow(&game_window);
    return 0;
}
