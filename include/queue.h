/*  =========================================================================
    queue - simple queue container

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#ifndef __QUEUE_H_INCLUDED__
#define __QUEUE_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

// Create a queue
queue_t *queue_new (void);

// Destroy a queue
// Note that all datas are popped and not freed by the queue
void queue_free (queue_t **self_p);

// Push data to the head
// Return 0 if OK or -1 if this failed for some reason (out of memory).
int queue_push_head (queue_t *self, void *data);

// Pop data from the head
void *queue_pop_head (queue_t *self);

// Get data from the head without removing it from the queue
void *queue_peek_head (queue_t *self);

// Push data to the tail
// Return 0 if OK or -1 if this failed for some reason (out of memory).
int queue_push_tail (queue_t *self, void *data);

// Pop data from the tail
void *queue_pop_tail (queue_t *self);

// Get data from the tail without removing it from the queue
void *queue_peek_tail (queue_t *self);

// Check if the queue is empty
bool queue_is_empty (queue_t *self);

// Self test
void queue_test (bool verbose);

#ifdef __cplusplus
}
#endif

#endif
