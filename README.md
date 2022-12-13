# small_shell

## Overview
Small Shell (smallsh) is a custom shell program written in C. Smallsh functions similar to other shells such as bash and provides cusstom handling of the commands 'exit, 'cd', and 'status', while also providing execution of other commands through C's exec family of commands. Other features of smallsh include custom handling of input/output redirection, support for running commands in foreground and background, and custom handling of stop and interrupt signal commands SIGINT and SIGSTP. 


## How to compile and run:
Compile with: gcc -std=gnu99 -Wall -Wextra -g -o smallsh smallsh.c
Run with: ./smallsh
