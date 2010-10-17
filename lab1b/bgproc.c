#include <stdlib.h>
#include "bgproc.h"

/* Initializes a bg_queue */
int bgq_init(bg_queue_t *bgq, pid_t pid)
{
    if (bgq)
    {
        bgq->pid = pid;
        bgq->first = NULL;
        bgq->last = NULL;
        
        return 0;
    }
    
    return 1;
}

/* Push a process on to the queue */
int bgq_push(bg_queue_t *bgq, pid_t pid)
{
    if (bgq)
    {
        wait_node_t *node = (wait_node_t *) malloc(sizeof(wait_node_t));
        node->pid = pid;
        node->next = NULL;
        
        if (bgq->last)
        {
            bgq->last->next = node;
            bgq->last = node;
        }
        else
        {
            bgq->first = node;
            bgq->last = node;
        }
        
        return 0;
    }
    return 1;
}

/* Get the next process ID in the queue */
pid_t bgq_next_id(bg_queue_t *bgq)
{
    if (bgq && bgq->first)
    {
        wait_node_t *node = bgq->first;
        pid_t pid = node->pid;
        bgq->first = node->next;
        if (bgq->first == NULL)
            bgq->last = NULL;
            
        free (node);
        
        return pid;
    }
    return 0;
}

/* Clears all allocated memory in bg_queue */
int bgq_clear(bg_queue_t *bgq)
{
    if (bgq)
    {
        wait_node_t *node = bgq->first;
        wait_node_t *next;
        
        while(node)
        {
            next = node->next;
            free(node);
            node = next;
        }
        
        bgq->first = NULL;
        bgq->last = NULL;
        
        return 0;
    }
    return 1;
}