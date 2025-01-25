# yeah lwnd yeah

This guy is the compositor for lightos. Processes can open a window by sending a upi packet
to the lwnd process. At that point the process is able to receive window events. These include window
resize, mouse click, keyboard click, ect. Next to that the process will receive a canvas it can paint in. 
These canvases are all collected by lwnd and blitted into the main framebuffer. lwnd will then keep track
of which layer the window is in and the convex hull the window is a part of.
