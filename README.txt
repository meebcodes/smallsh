=========================
	  = SMALLSH =
=========================

smallsh is a simple Unix shell written in C. This tool was originally
written for Oregon State University's CS 344 Operating Systems I class.

smallsh can:
- gracefully handle comments (starting with #), typos, and blank lines
- allow for variable expansion with $$, printing the current process's PID
- execute a collection of built-in commands (listed below)
- execute any other commands by forking a new process
- permit input and output redirection with > and <
- run background processes, printing their exit status at completion
- detect SIGINT and SIGTSTP with custom handlers

Built-in commands:
	cd [path]
		Change directory.
	status
		Print the exit status of the most recently terminated process.
	exit
		Exit the shell.

Additional notes:

	To run a background process, add & to the end of the command. E.g.:
		sleep 10 &

---------------
| COMPILATION |
---------------

To compile:
	gcc -std=gnu99 -o smallsh main.c shell.c