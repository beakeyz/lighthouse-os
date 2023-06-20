# The aniva scheduling logic

Everything that has to do with process and thread lifetime and life-cycle goes here. Every part of the 
scheduler should get its own source- and header file, which should then be documented here.

## scheduler.c

Main bulk of the scheduler. Handles process cycling and state

## profiler.c

Keeps track of the schedule priority of threads. What the name implies, it profiles the behaviour of a singular thread and translates it into a ranking of schedule priority.
