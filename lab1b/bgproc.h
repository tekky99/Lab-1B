#ifndef CS111_BGPROC_H
#define CS111_BGPROC_H

#include "cmdline.h"

#define I_BGJOBS 512

/* Linked list node used in bg_queue_t */
typedef struct wait_node wait_node_t;

struct wait_node
{
    pid_t pid;
    wait_node_t *next;
};

/* A linked list of processes waiting to run after the process stored
   in pid */
typedef struct
{
    pid_t pid;
    wait_node_t *first;
    wait_node_t *last;
} bg_queue_t;

/* Initializes a bg_queue */
int bgq_init(bg_queue_t *bgq, pid_t pid);

/* Push a process on to the queue */
int bgq_push(bg_queue_t *bgq, pid_t pid);

/* Get the next process ID in the queue */
pid_t bgq_next_id(bg_queue_t *bgq);

/* Clears all allocated memory in bg_queue */
int bgq_clear(bg_queue_t *bgq);

#endif
