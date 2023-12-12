#include "doomkeys.h"
#include "m_argv.h"
#include "doomgeneric.h"

#include <stdio.h>
#include <unistd.h>

#include <stdbool.h>

/*!
 * @brief: Initialize the game graphics core
 *
 * - Connect to the window server
 * - Setup framebuffer shit
 */
void DG_Init()
{
}

/*!
 * @brief: Copy shit from the back buffer to the lwnd driver
 */
void DG_DrawFrame()
{
}

void DG_SleepMs(uint32_t ms)
{
}

uint32_t DG_GetTicksMs()
{
  return 1000;
}

int DG_GetKey(int* pressed, unsigned char* doomKey)
{
  return 0;
}

void DG_SetWindowTitle(const char * title)
{
}

/* Our own argv and argc vars, since our system does not have those yet =)))) */
char* argv[] = {
  "doom",
  "-iwad",
  "Root/Apps/doom1.wad",
};
const int argc = 3;

/*!
 * @brief: Main entry for this app
 */
int main(/* int argc, char **argv */)
{
  doomgeneric_Create(argc, argv);

  for (int i = 0; i < 100; i++)
  {
      doomgeneric_Tick();
  }
  
  return 0;
}
