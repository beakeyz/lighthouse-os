# The prism in-kernel compiler and interpreter
This is a little concept mini-project to see how useful a built-in language directly inside a kernel actually is. GNU/Linux has bash and shell scripts, 
but that is managed by a userspace interface. I want to expose kernel API to supply program/script execution directly from the kernel.

# The language
This is an overview of the language

## System
When a user wants to talk to the kernel programmatically, the following flow happens:

```
Kernel invokation by the user -> Kernel prism compiler -> Kernel prism interpreter -> user feedback
``` 

We first compile the top-level prism code down to an assembly-like intermediate (bytecode) which is then interpreted. The bytecode can either
be discarded OR it can be cached in case the script is ran again unchanged.

## Top-level Syntax

### Keywords

| Keyword  | Description
|--------- | --------------------
| If       | Ask a question
|--------- | --------------------
| TODO     | TODO


## Bottom-level Syntax
This is a table of all the bytecode opcodes and their context for the interpreter

| Op-name  | Bytecode  | Size | Param count | Description
|--------- | --------- | ---- | ----------- | --------------------
| ADD      | 0x01      | 3    | 2           | Adds two integers
|--------- | --------- | ---- | ----------- | --------------------
| SUB      | 0x02      | 3    | 2           | Subtract two integers
|--------- | --------- | ---- | ----------- | --------------------
| TODO     | TODO      | TODO | TODO        | TODO

