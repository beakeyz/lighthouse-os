
/*!
 * @brief: LightOS display/environment manager and compositer
 *
 * Most likely called by init, but source is not important.
 * Needs lightos pipes to be set up
 *
 * Graphicals programs can use the dispmgr.lib library to interact with dispmgr
 *
 * dispmgr will:
 * 0) Do a validity check of it's environment and load defaults
 * 1) Set up the correct video drivers
 * 2) Set up communication pipes for processes
 * 3) Set up default input sources
 *      - NOTE: Defaults can be saved inside a PVR file at Root/Users/Admin/Core/dispmgr.pvr
 *        This file will then get loaded by dispmgr into it's environment.
 */
int main()
{
    for (;;)
        ;
    return -1;
}
