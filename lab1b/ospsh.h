#ifndef CS111_OSPSH_H
#define CS111_OSPSH_H

/* 
 * UCLA CS 111 - Fall 2007 - Lab 1
 * Header file for Lab 1B - Shell processing
 * This file contains the definitions required for executing commands
 * parsed in part A.
 */
 
struct zombie_list{
	pid_t *z_list;
	int z_size = 0;
	int MAX_Z_SIZE = 10;
};


#include "cmdline.h"
#include "bgproc.h"

/* Execute the command list. */
int command_line_exec(command_t *);

#endif
