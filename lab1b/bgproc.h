#ifndef CS111_OSPSH_H
#define CS111_OSPSH_H

#include "cmdline.h"

#define MAXBGJOBS 512

/* Linked list node used in bg_queue_t */
struct wait_node
{
    pid_t pid;
    wait_node *next;
} wait_node_t;

/* A linked list of processes waiting to run after the process stored
   in pid */
struct bg_queue
{
    pid_t pid;
    wait_node *first;
    wait_node *last;
} bg_queue_t;

/* Initializes a bg_queue */
int bgq_init(bg_queue_t *bgq);

/* Push a process on to the queue */
int bgq_push(pid_t pid);

/* Get the next process ID in the queue */
pid_t bgq_next_id();

/* Clears all allocated memory in bg_queue */
int bgq_clear();

/* Returns the background process's id */
pid_t bgq_get_bgid();