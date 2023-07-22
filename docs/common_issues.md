This quick document will outline the issues I frequently encounter while looking at either old code, or even some 
newly written code. I will do this in such a way that I can avoid making these design-, structure- and logical mistakes
that make my life really hard trying to understand, refactor or debug any code.

- Functionality being locked behind deep functions: with this issue I find functionality deeply nested inside functions that
are used often. This makes it so that we essentially lack any nuance when we need it. This is due to a design principle that 
I like to use sometimes where we integrate functionality inside some function that tries to accomplish something else, or even
a series of actions. A function might open a file, read from it and close it again (crude example). I might have wrote a utility function
for this that does this in its own way, all at once, while we really want these 3 things to be their own functions with their own 
nuances. In short: try to lead functionality down a concrete pipeline where we separate high-level logic from low-level implementation
