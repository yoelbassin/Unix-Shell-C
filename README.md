# Unix Shell
A Unix shell written in C

This project consists of designing a C program to serve as a shell interface
that accepts user commands and then executes each command in a separate
process. Implementation supports input and output redirection, as
well as pipes as a form of IPC between multiple commands. The project is based on the UNIX fork(), exec(), wait(), dup2(), and
pipe() system calls for UNIX.

The shell uses the `GNU Readline` library, so for compiling the program use `gcc shell.c -o shell -lreadline`, and `./shell` for running it.
If the computer doesn't have the library installed, use `sudo apt-get install libreadline-dev` to install it.
