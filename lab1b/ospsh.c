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
#include <signal.h>
#include "cmdline.h"
#include "ospsh.h"
#include "bgproc.h"

/* Global variables */

volatile bg_queue_t *bg_jobs = NULL; // List of background processes organized by job ID
volatile int max_jobs = 0;           // The highest numbered job ID
volatile int job_index = 1;          // We start from index 1, because there is no job 0
volatile sig_atomic_t after_sig = 0; // Used in exec_after child process. Parent should never touch this
volatile int top_level = 1;                   // Keeps track of if we're on top level shell

//Signal Handlers for the 'after' command

/* Catches SIGUSR2
 * Waiting process can now execute command.
 */
void
set_after_sig(int sig)
{
    after_sig = 1;
}

/* Catches SIGUSR1. Background process has completed.
 * Wake any waiting processes through SIGUSR2.
 */
void
bg_complete(int sig, siginfo_t *info, void *context)
{
    pid_t pid;
    int job_id;
   
    // Find pid of process that threw signal
    if (info->si_code > 0)
        return;
    pid = info->si_pid;
     
    //Look up background process id in bg_jobs
    job_id = 1;
    while (job_id < job_index)
    {
        if (bg_jobs[job_id].pid == pid)
            break;
        job_id++;
    }
    
    // Return if we did not find process in bg_jobs
    if (job_id >= job_index)
        return;
    
    //Send a signal to each member on the queue
    pid = bgq_next_id((bg_queue_t *) &(bg_jobs[job_id]));
    while (pid > 0)
    {
        kill(pid, SIGUSR2);
        pid = bgq_next_id((bg_queue_t *) &(bg_jobs[job_id]));
    }
    
    //Since the process is done, set pid to 0
    bg_jobs[job_id].pid = 0;
}

/** 
 * Reports the creation of a background job in the following format:
 *  [job_id] process_id
 * to stderr.
 */
void report_background_job(int job_id, int process_id);

void cleanValues();

void set_redirects(command_t *cmd, int *pass_pipefd, int *pipefd)
{
	int fd;
    
    if (*pass_pipefd != STDIN_FILENO) {
				dup2(*pass_pipefd,STDIN_FILENO);
				close(*pass_pipefd);
        }
			
		if (cmd->controlop == CMD_PIPE) {
			if (pipefd[1] != STDOUT_FILENO) {
				dup2(pipefd[1],STDOUT_FILENO);
				close(pipefd[1]);
			}
			close(pipefd[0]);
		}

		// Check for Redirection and set it up
		if (cmd->redirect_filename[0]){
			fd = open(cmd->redirect_filename[0], O_RDONLY);
			if (fd < 0){
				fprintf(stderr,"OPEN WRONG!\n");
				abort();
			}
			dup2(fd,STDIN_FILENO);
			close(fd);
		}
		if (cmd->redirect_filename[1]){
			fd = open(cmd->redirect_filename[1], O_WRONLY|O_CREAT|O_TRUNC, 0666);
			if (fd < 0){
				fprintf(stderr,"OPEN WRONG!\n");
				abort();
			}
			dup2(fd,STDOUT_FILENO);
			close(fd);
		}
		if (cmd->redirect_filename[2]){
			fd = open(cmd->redirect_filename[2], O_WRONLY|O_CREAT|O_TRUNC, 0666);
			if (fd < 0){
				fprintf(stderr,"OPEN WRONG!\n");
				abort();
			}
			dup2(fd,STDERR_FILENO);
			close(fd);
		}
}

void create_bg_listener(void)
{
    // Fork again, since we need someone to notify the parent
    // when this background process completes
    pid_t pid = fork();
    
    // If we are child, exit the function and continue as normal
    // Do the following if we are parent.
    if (pid != 0) {
        int wp_status;
        
        if (waitpid(pid, &wp_status, 0) < 0) {
            fprintf(stderr,"BACKGROUND WAIT WRONG!\n");
            exit(1);
        }
        
        //Background process has finished running.
        //Signal the parent and then exit with same status
        kill(getppid(), SIGUSR1);
        
    
        if (WIFEXITED(wp_status))
            exit(WEXITSTATUS(wp_status));
        exit(1);
    }
}

int alloc_bg_jobs();
pid_t exec_after(command_t *cmd, int *pass_pipefd, int *pipefd);

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
		
	}
	
	if (cmd->argv[0] && !strcmp(cmd->argv[0],"exit")){
		if (cmd->argv[1] && strcmp(cmd->argv[1],"0"))
			exit(atoi(cmd->argv[1]));
		exit(0);
	}
	
	if (cmd->argv[0] && !strcmp(cmd->argv[0],"after") && top_level){
		pid = exec_after(cmd, pass_pipefd, pipefd);
	}
    else 
        pid = fork();
	
	if (pid < 0){
		fprintf(stderr,"FORK WRONG!\n");
		return -1;
	} else if (pid == 0){
	
        // Check if this is a background process
        if (cmd->controlop == CMD_BACKGROUND) {
            create_bg_listener();
        }
	
        // File redirections
		set_redirects(cmd, pass_pipefd, pipefd);
		
		// Parenthesis special case
		if (cmd->subshell){
            int exit_status;
            int is_top = top_level;
            
            top_level = 0;
			exit_status = command_line_exec(cmd->subshell);
            top_level = 1;
            exit(exit_status);
		}
		if (!(cmd->argv[0]) && !(cmd->subshell))
			exit(0);
		
		if (!strcmp(cmd->argv[0],"cd")){
			char *path;
			if (cmd->argv[1])
				path = cmd->argv[1];
			else
				path = getenv("HOME");
			
			if (chdir(path)<0){
				exit(1);
			}
			exit(0);
		}
			
		if (execvp(cmd->argv[0], cmd->argv) < 0){
			fprintf(stderr,"PROGRAM WRONG!\n");
			abort();
		}
	}
	
	
	if (cmd->controlop == CMD_PIPE) {
		dup2(pipefd[0], *pass_pipefd);
		if (pipefd[0] != STDIN_FILENO) {
			close(pipefd[0]);
		}
		close(pipefd[1]);
	}
	else {
		dup2 (STDIN_FILENO, *pass_pipefd);
	}
		
	if (cmd->argv[0] && !strcmp(cmd->argv[0],"cd")){
		char *path;
		if (cmd->argv[1])
			path = cmd->argv[1];
		else
			path = getenv("HOME");
		
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
    //       e. "after". (Roman is implementing this)
	//
	// In the parent, you should:
	//    1. Close some file descriptors.  Hint: Consider the write end
	//       of this command's pipe, and one other fd as well.
	//    2. Handle the special "exit" and "cd" commands. (As well as "after")
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
    
    //Set handler for detecting exited child processes
    if (top_level) {
        struct sigaction bg_action;
        bg_action.sa_sigaction = bg_complete;
        sigemptyset (&bg_action.sa_mask);
        bg_action.sa_flags = SA_SIGINFO|SA_RESTART;
        sigaction(SIGUSR1, &bg_action, NULL);
    }
    
	while (cmdlist) {
		int wp_status;      // Hint: use for waitpid's status argument!
                            // Read the manual page for waitpid() to
                            // see how to get the command's exit
                            // status (cmd_status) from this value.
		pid_t pid = command_exec(cmdlist, &pipefd);
		
		if (pid < 0)
			abort();
		if (pid != 0){
			switch(cmdlist->controlop){
				case CMD_END:
				case CMD_SEMICOLON:
					if (waitpid(pid, &wp_status, 0) < 0){
						fprintf(stderr,"SEMICOLON WAIT WRONG!\n");
						return -1;
					}
					if (WIFEXITED(wp_status))
						cmd_status = WEXITSTATUS(wp_status);
					break;
				case CMD_AND:
					if (waitpid(pid, &wp_status, 0) < 0){
						fprintf(stderr,"AND WAIT WRONG!\n");
						return -1;
					}
					if (!WIFEXITED(wp_status) || WEXITSTATUS(wp_status) != 0) {
						cmd_status = WEXITSTATUS(wp_status);
						goto done;
					}
					
					break;
				case CMD_OR:
					if (waitpid(pid, &wp_status, 0) < 0){
						fprintf(stderr,"OR WAIT WRONG!\n");
						return -1;
					}
					if (!WIFEXITED(wp_status) || WEXITSTATUS(wp_status) == 0) {
						cmd_status = WEXITSTATUS(wp_status);
						goto done;
					}
					break;
                case CMD_BACKGROUND:
                    if (top_level) {
                        int i = 1;
                        
                        //Allocate more space if we are out
                        if (job_index >= max_jobs) {
                            if (alloc_bg_jobs() != 0)
                                abort();
                        }
                        
                        //Cycle through bg_jobs until we find an empty one
                        while (i < job_index) {
                            if (bg_jobs[i].pid == 0)
                                break;
                            i++;
                        }
                                
                        //If we couldn't find an empty one, increment job_index
                        if (i == job_index)
                            job_index++;
                            
                        //Initialize the new job
                        bgq_init((bg_queue_t *) &bg_jobs[i], pid);
                        report_background_job(i, pid);
                    }
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
	if (top_level)
		cleanValues();
	return cmd_status;
}

/* exec_after(cmd)
 *
 * Executes the 'after' command stored in cmd
 *
 * Does some error checking first to make sure the parameters are correct.
 * Returns process id 0 if there was a syntax error.
 * 
 * If the first argument is a valid job id (> 0) then after will check
 * if that background job it exists or is still running. If it is not
 * running, after will immediately fork and have the child execute the
 * given command
 *
 * If the job is still running, exec_after will fork the child, and the child
 * will yield until it receives a signal from its parent. Once it does, it will
 * execute the given command.
 *
 * The parent will add the child's process ID to the bg_queue_t bg_job struct
 * and exit with the child process' ID. The code can handle the child
 * appropriately back in command_exec and command_line_exec.
 *
 * When the background process specified in the job id finally exits, it will
 * throw a signal to the parent. The parent will catch the signal and look up
 * the appropriate job in bg_jobs. Each of the processes waiting on the queue
 * will be signaled to wake up and execute.
 */
pid_t
exec_after(command_t *cmd, int *pass_pipefd, int *pipefd)
{
    int job_id;
    int run_now = 0;    // whether specified command should run now
    bg_queue_t *bg_job;
    pid_t before_id;
    pid_t after_id;
    sigset_t bg_block;
    
    // Block any signals coming from child background processes
    // The background processes will be signalling with SIGUSR1,
    // so that's what we'll block. Also block SIGUSR2 so that our
    // child process can set up first.
    sigemptyset(&bg_block);
    if (sigaddset(&bg_block, SIGUSR1 | SIGUSR2) == -1)
        fprintf(stderr, "after: Invalid call to sigaddset()\n");
    if (sigprocmask(SIG_BLOCK,&bg_block,NULL) == -1)
        fprintf(stderr, "after: Invalid call to sigprocmask()\n");

    //Error Checking
    
    
    //after program must contain at least one argument
    if (strcmp(cmd->argv[0], "after") || cmd->argv[1] == NULL) 
    {
        //Don't run at all. Exit with error
        run_now = -1;
    }
    else {
        job_id = atoi(cmd->argv[1]);
        if (job_id <= 0)
            run_now = -1;
    }
    
    //If job number doesn't exist or has already completed, run command immediately
    if (run_now == -1)
    {
        //Do nothing
    }
    else if (job_id >= max_jobs || bg_jobs[job_id].pid == 0)
        run_now = 1;
    else {
        bg_job = (bg_queue_t *) &bg_jobs[job_id];
        before_id = bg_job->pid;
    }
    
    after_id = fork();
    
    if (after_id == 0)
    {
        sigset_t cont_block;
        sigset_t old_set;
        sigemptyset(&cont_block);
        sigaddset(&cont_block, SIGUSR2);
        
        set_redirects(cmd, pass_pipefd, pipefd);
        
        if (run_now == -1) {
            //Output an error to stderr and return 0
            fprintf(stderr, "after: Syntax error\n");
            exit(1);
        }
        
        if (!run_now)
        {
            //Set signal handler
            struct sigaction cont_action;
            cont_action.sa_handler = set_after_sig;
            sigemptyset (&cont_action.sa_mask);
            cont_action.sa_flags = 0;
            sigaction(SIGUSR2, &cont_action, NULL);
            
            //Unblock SIGUSR2
            sigprocmask(SIG_UNBLOCK,&cont_block,NULL);
            
            //Suspend until signal has been received
            sigprocmask(SIG_BLOCK,&cont_block,&old_set);
            while (!after_sig)
                sigsuspend(&old_set);
        }
        
        // Check if this is a background process
        if (cmd->controlop == CMD_BACKGROUND) {
            create_bg_listener();
        }
        
        if (cmd->argv[2]) {
            //Execute command and return the value (execvp shouldn't return!)
            exit(execvp(cmd->argv[2], &(cmd->argv[2])));
        }
        else {
            exit(0);
        }
    }
    
    //Still in parent if we reach here
    //Push waiting process onto queue
    if (!run_now)
        bgq_push(bg_job, after_id);
    
    //Unblock signals from background processes
    if (sigprocmask(SIG_UNBLOCK,&bg_block,NULL) == -1)
        fprintf(stderr, "after: Invalid call to sigprocmask()\n");
        
    return after_id;
}

/* Allocates more memory for bg_jobs */
int alloc_bg_jobs()
{
    int i = job_index;
    
    //Initialize bg_jobs the first time
    if (!bg_jobs)
    {
        max_jobs = I_BGJOBS;
        bg_jobs = (bg_queue_t *) malloc(max_jobs*sizeof(bg_queue_t));
        if (!bg_jobs)
            goto error;
    }
    else //Allocate a new space twice the size of the old one
    {
        bg_queue_t *old = (bg_queue_t *) bg_jobs;
        max_jobs = 2*max_jobs;
        bg_jobs = (bg_queue_t *) malloc(max_jobs*sizeof(bg_queue_t));
        if (!bg_jobs) {
            bg_jobs = old;
            goto error;
        }
            
        //Transfer old structs to new
        memcpy((bg_queue_t *) bg_jobs, old, i * sizeof(bg_queue_t));

        free(old);
    }
    
    return 0;
    
    error:
        fprintf(stderr, "IO Error: Too many background processes!\n");
        return -1;
}

/* Frees all memory in bg_jobs */
int free_bg_jobs()
{
    int i = 1;
    while (i < max_jobs && i < job_index)
    {
        bgq_clear((bg_queue_t *) &bg_jobs[i]);
        i++;
    }
    free((bg_queue_t *) bg_jobs);
    
    bg_jobs = NULL;
    max_jobs = 0;
    job_index = 1;
    
    return 0;
}

void report_background_job(int job_id, int process_id) {
    fprintf(stderr, "[%d] %d\n", job_id, process_id);
}

void cleanValues(){
}
