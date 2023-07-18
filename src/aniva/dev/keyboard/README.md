# Aniva keyboard device interface

This part of the kernel is dedicated to ease the development of keyboard drivers. Keyboard drivers supply the kernel with keyevents, which the kernel can then 
pass on to userspace, either through dedicated listeners for keyevents in us, or through the stdin stream. Here, we specify the interface that drivers will
use to communicate with the kernel. 

 - key_event (Single buffer that contains the last character pressed and signals a press)
 - key_stream (Buffer or file where keyevents can be streamed to)
 - ...
