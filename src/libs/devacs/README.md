# Aniva userspace device access driver

This library is supposed to enable processes to interact with devices on the Dev/ path. 
This is an intermediate driver, and it's not really supposed to be used by any process, rather
it's goal is to abstract the devices from the kernel for any higher level libraries that
would like to work with them (audio, video, input, et cetera). Only if a process knows what it's doing
it can use this library in order to talk to devices at a low level.
