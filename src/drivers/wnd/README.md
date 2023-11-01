# ANIVA lwnd

The aniva Light WiNdow Driver

This is one of the few service drivers that supply a complete user environment right from ring 0. The userspace graphics library can directly
interface with this driver to get the stuff that it wants. In order to support things like SDL, we're going to need to look into how that port 
is going to go, but it probably won't be that hard to let lwnd emulate some sort of generic display driver.

## Archtecture

lwnd is built on top of screens and windows.

For every screen that is connected to the system (for now only the main screen) we construct a window-stack. The compositing and tiling will
be rather simple, so there won't be that much interesting. Every window gets its own dummy buffer that gets a standard 32 bpp. Every render
pass we try to synchronise the frame buffers into the main frame buffer. This buffer is managed by the kernel GPU driver and it can either be
single buffered or double buffered. We currently copy everything into the buffer ourselves, but we want to be able to pageflip on a vblank interrupt
and make use of fast bitblt provided to us by the GPU (driver).

Compositing right now is decently speedy, so nothing sweaty there yet. As for other parts of the desktop, we'll seperate a desktop into two parts:

 - Windows: Apps, taskbar, status-bar, ect.
 - Dynamics: Cursor, pop-ups, ect.

Both of these things are managed by the screen object. Compositing is done in this order:

```
Z-less Dynamics -> Windows from front to back -> Z-full Dynamics -> Background
```

Where we keep track of what we've already painted using a pixel bitmap. All the pixels that are not already painted at the end of the render
pass, will get filled in with the appropriate background color for that pixel. (The background is just a lwnd-owned window that sits at the
bottom of the window-stack lmaoooo)

## Customization

lwnd will use profile variables to expose customization to the user. Processes can set variables like:

 - BCKGRND_IMG: to change the background picture
 - CRSR_IMG: to change the cursor picture
 - CRSR_SZE: to change the cursor size (with a minimum of 8 and a maximum of 32)
 - MAIN_CLR: to change the color we use for tinting backgrounds
 - CLR_THEME: to choose a color theme (When NONE is supplied, we use MAIN_CLR)

