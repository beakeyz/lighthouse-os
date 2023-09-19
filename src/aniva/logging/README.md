# The aniva logging system

Here we outline the basic functions that logging drivers need to implement in order to be registered
into the logger list

## How is the system going to look?

I've had this idea to implement clean system logging for a while now. The basic idea revolves around having different 
loggers for different "endpoints" (things that can display or store logs, i.e. screens or files) where we can register
one or more logger(s) to be the "default", so that all the logging traffic gets routed through them

### One or more??

Yes. The idea is to have a bunch of loggers in a sort of pipeline that all get fed logs as they come in.
For example: we could have a logger that handles I/O to the screen and another logger that pipes every log
to a file so we can look at them later (in case of a crash for example).
