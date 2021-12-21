# Operating Systems 1: Smallsh
Portfolio assignment from my Operating Systems 1 class. Submitted in February 2021. 

## Assignment Instructions Overview:
In this assignment I was tasked to write smallsh, my own shell in C. smallsh should implement a subset of features of well-known shells, such as bash. The program should

1. Provide a prompt for running commands
2. Handle blank lines and comments, which are lines beginning with the # character
3. Provide expansion for the variable $$
4. Execute 3 commands exit, cd, and status via code built into the shell
5. Execute other commands by creating new processes using a function from the exec family of functions
6. Support input and output redirection
7. Support running commands in foreground and background processes
8. Implement custom handlers for 2 signals, SIGINT and SIGTSTP
