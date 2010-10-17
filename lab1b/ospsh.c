/* 
 * UCLA CS 111 - Fall 2007 - Lab 1
 * Skeleton code for Lab 1 - Shell processing
 * This file contains skeleton code for executing commands parsed in part A.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "cmdline.h"
#include "ospsh.h"

/** 
 * Reports the creation of a background job in the following format:
 *  [job_id] process_id
 * to stderr.
 */
 void abortClean(){
	abort();
}
void report_background_job(int job_id, int process_id);

/* command_exec(cmd, pass_pipefd)
 *
 *   Execute the single command specified in the 'cmd' command structure.
 *
 *   The 'pass_pipefd' argument is used for pipes.
 *   On input, '*pass_pipefd' is the file descriptor used to read the
 *   previous command's output.  That is, it's the read end of the previous
 *   pipe.  It equals STDIN_FILENO if there was no previous pipe.
 *   On output, command_exec should set '*pass_pipefd' to the file descriptor
 *   used for reading from THIS command's pipe.
 *   If this command didn't have a pipe -- that is, if cmd->commandop != PIPE
 *   -- then it should set '*pass_pipefd = STDIN_FILENO'.
 *
 *   Returns the process ID of the forked child, or < 0 if some system call
 *   fails.
 *
 *   You must also handle the internal commands "cd" and "exit".
 *   These are special because they must execute in the shell process, rather
 *   than a child.  (Why?)
 *
 *   However, these special commands still have a status!
 *   For example, "cd DIR" should return status 0 if we successfully change
 *   to the DIR directory, and status 1 otherwise.
 *   Thus, "cd /tmp && echo /tmp exists" should print "/tmp exists" to stdout
 *   iff the /tmp directory exists.
 *   Not only this, but redirections should work too!
 *   For example, "cd /tmp > foo" should create an empty file named 'foo';
 *   and "cd /tmp 2> foo" should print any error messages to 'foo'.
 *
 *   How can you return a status, and do redirections, for a command executed
 *   in the parent shell?
 *   Hint: It is easiest if you fork a child ANYWAY!
 *   You should divide functionality between the parent and the child.
 *   Some functions will be executed in each process.
 */
static pid_t
command_exec(command_t *cmd, int *pass_pipefd)
{
	pid_t pid = -1;		// process ID for child
	int pipefd[2];		// file descriptors for this process's pipe

	/* EXERCISE: Complete this function!
	 * We've written some of the skeleton for you, but feel free to
	 * change it.
	 */

	// Create a pipe, if this command is the left-hand side of a pipe.
	// Return -1 if the pipe fails.
	if (cmd->controlop == CMD_PIPE) {
		/* Your code here. */
		if (pipe(pipefd) < 0)
			return -1;
		*pass_pipefd = pipefd[0];
	}
	
	if (cmd->argv[0] && !strcmp(cmd->argv[0],"exit")){
		if (cmd->argv[1] && strcmp(cmd->argv[1],"0"))
			exit(atoi(cmd->argv[1]));
		exit(0);
	}
	
	pid = fork();
	int fd;
	
	if (pid < 0){
		fprintf(stderr,"FORK WRONG!\n");
		return -1;
	} else if (pid == 0){

		// Check for Redirection and set it up
		if (cmd->redirect_filename[0]){
			fd = open(cmd->redirect_filename[0], O_RDONLY);
			if (fd < 0){
				fprintf(stderr,"OPEN WRONG!\n");
				abortClean();
			}
			dup2(fd,0);
			close(fd);
		}
		if (cmd->redirect_filename[1]){
			fd = open(cmd->redirect_filename[1], O_WRONLY|O_CREAT);
			if (fd < 0){
				fprintf(stderr,"OPEN WRONG!\n");
				abortClean();
			}
			dup2(fd,1);
			close(fd);
		}
		if (cmd->redirect_filename[2]){
			fd = open(cmd->redirect_filename[2], O_WRONLY|O_CREAT);
			if (fd < 0){
				fprintf(stderr,"OPEN WRONG!\n");
				abortClean();
			}
			dup2(fd,2);
			close(fd);
		}
		
		// Parenthesis special case
		if (cmd->subshell){
			if (command_line_exec(cmd->subshell) < 0){
				fprintf(stderr, "SUBSHELL WRONG!\n");
				return -1;
			}
		}
		if (!(cmd->argv[0]) && !(cmd->subshell))
			_exit(0);
		
		if (!strcmp(cmd->argv[0],"cd")){
			char *path;
			if (cmd->argv[1])
				path = cmd->argv[1];
			else
				path = "~";
			
			if (chdir(path)<0){
				exit(1);
			}
			exit(0);
		}
			
		if (execvp(cmd->argv[0], cmd->argv) < 0){
			fprintf(stderr,"PROGRAM WRONG!\n");
			abortClean();
		}
	}
	
	if (!(cmd->argv[0]) && !(cmd->subshell))
			exit(0);
		
		if (!strcmp(cmd->argv[0],"cd")){
			char *path;
			if (cmd->argv[1])
				path = cmd->argv[1];
			else
				path = "~";
			
			if (chdir(path)<0){
				fprintf(stderr, "Invalid File Path\n");
			}
		}
	//fprintf(stderr,"Child is done\n");
	// Fork the child and execute the command in that child.
	// You will handle all redirections by manipulating file descriptors.
	//
	// This section is fairly long.  It is probably best to implement this
	// part in stages, checking it after each step.  For instance, first
	// implement just the fork and the execute in the child.  This should
	// allow you to execute simple commands like 'ls'.  Then add support
	// for redirections: commands like 'ls > foo' and 'cat < foo'.  Then
	// add parentheses, then pipes, and finally the internal commands
	// 'cd' and 'exit'.
	//
	// In the child, you should:
	//    1. Set up stdout to point to this command's pipe, if necessary.
	//    2. Set up stdin to point to the PREVIOUS command's pipe (that
	//       is, *pass_pipefd), if appropriate.
	//    3. Close some file descriptors.  Hint: Consider the read end
	//       of this process's pipe.
	//    4. Set up redirections.
	//       Hint: For output redirections (stdout and stderr), the 'mode'
	//       argument of open() should be set to 0666.
	//    5. Execute the command.
	//       There are some special cases:
	//       a. Parentheses.  Execute cmd->subshell.  (How?)
	//       b. A null command (no subshell, no arguments).
	//          Exit with status 0.
	//       c. "exit".
	//       d. "cd".
	//
	// In the parent, you should:
	//    1. Close some file descriptors.  Hint: Consider the write end
	//       of this command's pipe, and one other fd as well.
	//    2. Handle the special "exit" and "cd" commands.
	//    3. Set *pass_pipefd as appropriate.
	//
	// "cd" error note:
	// 	- Upon syntax errors: Display the message
	//	  "cd: Syntax error on bad number of arguments"
	// 	- Upon system call errors: Call perror("cd")
	//
	// "cd" Hints:
	//    For the "cd" command, you should change directories AFTER
	//    the fork(), not before it.  Why?
	//    Design some tests with 'bash' that will tell you the answer.
	//    For example, try "cd /tmp ; cd $HOME > foo".  In which directory
	//    does foo appear, /tmp or $HOME?  If you chdir() BEFORE the fork,
	//    in which directory would foo appear, /tmp or $HOME?
	//
	//    EXTRA CREDIT: Our "cd" solution changes the
	//    directory both in the parent process and in the child process.
	//    This introduces a potential race condition.
	//    Explain what that race condition is, and fix it.
	//    Hint: Investigate fchdir().

	/* Your code here. */

	// return the child process ID
	return pid;
}


/* command_line_exec(cmdlist)
 *
 *   Execute the command list.
 *
 *   Execute each individual command with 'command_exec'.
 *   String commands together depending on the 'cmdlist->controlop' operators.
 *   Returns the exit status of the entire command list, which equals the
 *   exit status of the last completed command.
 *
 *   The operators have the following behavior:
 *
 *      CMD_END, CMD_SEMICOLON
 *                        Wait for command to exit.  Proceed to next command
 *                        regardless of status.
 *      CMD_AND           Wait for command to exit.  Proceed to next command
 *                        only if this command exited with status 0.  Otherwise
 *                        exit the whole command line.
 *      CMD_OR            Wait for command to exit.  Proceed to next command
 *                        only if this command exited with status != 0.
 *                        Otherwise exit the whole command line.
 *      CMD_BACKGROUND, CMD_PIPE
 *                        Do not wait for this command to exit.  Pretend it
 *                        had status 0, for the purpose of returning a value
 *                        from command_line_exec.
 */
int
command_line_exec(command_t *cmdlist)
{
	int cmd_status = 0;	    // status of last command executed
	int pipefd = STDIN_FILENO;  // read end of last pipe
	
	while (cmdlist) {
		int wp_status;	    // Hint: use for waitpid's status argument!
				    // Read the manual page for waitpid() to
				    // see how to get the command's exit
				    // status (cmd_status) from this value.
		pid_t pid = command_exec(cmdlist, &pipefd);
		
		if (pid < 0)
			abortClean();
		if (pid != 0){
			switch(cmdlist->controlop){
				case CMD_END:
				case CMD_SEMICOLON:
					if (waitpid(pid, &wp_status, 0) < 0){
						fprintf(stderr,"WAIT WRONG!\n");
						return -1;
					}
					if (!WIFEXITED(wp_status))
						cmd_status = WEXITSTATUS(wp_status);
					break;
				case CMD_AND:
					if (waitpid(pid, &wp_status, 0) < 0){
						fprintf(stderr,"WAIT WRONG!\n");
						return -1;
					}
					if (!WIFEXITED(wp_status) || WEXITSTATUS(wp_status) != 0) {
						cmd_status = WEXITSTATUS(wp_status);
						goto done;
					}
					
					break;
				case CMD_OR:
					if (waitpid(pid, &wp_status, 0) < 0){
						fprintf(stderr,"WAIT WRONG!\n");
						return -1;
					}
					if (!WIFEXITED(wp_status) || WEXITSTATUS(wp_status) == 0) {
						cmd_status = WEXITSTATUS(wp_status);
						goto done;
					}
					break;
				default:
					cmd_status = 0;
			}
		}
		// EXERCISE: Fill out this function!
		// If an error occurs in command_exec, feel free to abort().
		
		/* Your code here. */

		cmdlist = cmdlist->next;
	}

done:
	return cmd_status;
}

void report_background_job(int job_id, int process_id) {
    fprintf(stderr, "[%d] %d\n", job_id, process_id);
}


