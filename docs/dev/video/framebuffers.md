# Framebuffers

One of the types of video devices are those that utilize framebuffers: VESA/VGA, GOP, et cetera. For these devices we provide 
a driver structure that registers a video device with a framebuffer. These devices are often limited in their features, which makes 
them a good option for early bootmenus or simply getting pixels on the screen in most cases. Framebuffers provide functions that
govern correct writing to the framebuffer. Any compositing should be done by outside users of the fb drivers. 

## Functions

The functions provided by framebuffer drivers should provide easy I/O for the framebuffer, while also maintaining the correct
format. Since framebuffers can use every type of pixel format that the underlying device wants, the driver provides 

* f_clr_transform

This function should also be used internally by the framebuffer when writes happen and takes in the standardised fb_color_t which is just a fancy dword
