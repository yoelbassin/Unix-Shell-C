# Unix Shell
A Unix shell written in C. Based on project I of chapter 3 from the book Operating System Concepts 10th

This project consists of designing a C program to serve as a shell interface
that accepts user commands and then executes each command in a separate
process. Your implementation will support input and output redirection, as
well as pipes as a form of IPC between a pair of commands. Completing this
project will involve using the UNIX fork(), exec(), wait(), dup2(), and
pipe() system calls and can be completed on any Linux, UNIX, or macOS
system.

The shell uses the `GNU Readline` library, so for compiling the program use `gcc shell.c -o shell -lreadline`, and `./shell` for running it.
If the computer doesn't have the library installed, use `sudo apt-get install libreadline-dev` to install it.
