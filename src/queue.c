/*  =========================================================================
    queue - implementation

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#include "classes.h"


typedef struct _queue_entry_t queue_entry_t;


struct _queue_entry_t {
    void *data;
    queue_entry_t *prev;
    queue_entry_t *next;
};


struct _queue_t {
    queue_entry_t *head;
    queue_entry_t *tail;
};


queue_t *queue_new (void) {
    queue_t *self = (queue_t *) malloc (sizeof (queue_t));
    assert (self);

    self->head = NULL;
    self->tail = NULL;

    print_info ("queue created.\n");
    return self;
}


void queue_free (queue_t **self_p) {
    assert (self_p);

    if (*self_p) {
        queue_t *self = *self_p;
        while (!queue_is_empty (self))
            queue_pop_head (self);
        free (self);
        *self_p = NULL;
    }
    print_info ("queue freed.\n");
}


int queue_push_head (queue_t *self, void *data) {
    assert (self);
    assert (data);

    queue_entry_t *new_entry =
        (queue_entry_t *) malloc (sizeof (queue_entry_t));
    if (new_entry == NULL) {
        print_error ("Out of memory.\n");
        return -1;
    }

    new_entry->data = data;
    new_entry->prev = NULL;
    new_entry->next = self->head;

    // insert entry to the queue
    if (self->head == NULL) { // queue is empty
        self->head = new_entry;
        self->tail = new_entry;
    } else {
        self->head->prev = new_entry;
        self->head = new_entry;
    }

    return 0;
}


void *queue_pop_head (queue_t *self) {
    assert (self);

    if (queue_is_empty (self))
        return NULL;

    // Get the first entry
    queue_entry_t *entry = self->head;
    self->head = entry->next;
    void *data = entry->data;

    if (self->head == NULL) { // queue is empty
        self->tail = NULL;
    } else {
        self->head->prev = NULL;
    }

    free (entry);
    return data;
}


void *queue_peek_head (queue_t *self) {
    assert (self);
    if (queue_is_empty (self))
        return NULL;
    else
        return self->head->data;
}


int queue_push_tail (queue_t *self, void *data) {
    assert (self);
    assert (data);

    queue_entry_t *new_entry =
        (queue_entry_t *) malloc (sizeof (queue_entry_t));

    if (new_entry == NULL) {
        print_error ("Out of memory.\n");
        return -1;
    }

    new_entry->data = data;
    new_entry->prev = self->tail;
    new_entry->next = NULL;

    // Insert entry into queue
    if (self->tail == NULL) { // queue is empty
        self->head = new_entry;
        self->tail = new_entry;
    } else {
        self->tail->next = new_entry;
        self->tail = new_entry;
    }

    return 0;
}


void *queue_pop_tail (queue_t *self) {
    assert (self);

    if (queue_is_empty (self))
        return NULL;

    queue_entry_t *entry = self->tail;
    self->tail = entry->prev;
    void *data = entry->data;

    if (self->tail == NULL) // queue is empty
        self->head = NULL;
    else
        self->tail->next = NULL;

    free(entry);
    return data;
}


void *queue_peek_tail (queue_t *self) {
    assert (self);
    if (queue_is_empty (self))
        return NULL;
    else
        return self->tail->data;
}


bool queue_is_empty (queue_t *self) {
    assert (self);
    return self->head == NULL;
}


void queue_test (bool verbose) {
    print_info (" * queue: \n");
    queue_t *queue = queue_new ();
    assert (queue);
    assert (queue_is_empty (queue));

    char *cheese = "boursin";
    char *bread = "baguette";
    char *wine = "bordeaux";

    // 1
    queue_push_head (queue, cheese);
    assert (!queue_is_empty (queue));
    queue_push_head (queue, bread);
    queue_push_head (queue, wine);

    assert (queue_peek_head (queue) == wine);
    assert (queue_peek_tail (queue) == cheese);

    assert (queue_pop_head (queue) == wine);
    assert (queue_pop_head (queue) == bread);
    assert (queue_pop_head (queue) == cheese);
    assert (queue_is_empty (queue));

    // 2
    queue_push_head (queue, cheese);
    assert (!queue_is_empty (queue));
    queue_push_head (queue, bread);
    queue_push_head (queue, wine);

    assert (queue_pop_tail (queue) == cheese);
    assert (queue_pop_tail (queue) == bread);
    assert (queue_pop_tail (queue) == wine);
    assert (queue_is_empty (queue));

    // 3
    queue_push_tail (queue, cheese);
    assert (!queue_is_empty (queue));
    queue_push_tail (queue, bread);
    queue_push_tail (queue, wine);

    assert (queue_pop_tail (queue) == wine);
    assert (queue_pop_tail (queue) == bread);
    assert (queue_pop_tail (queue) == cheese);
    assert (queue_is_empty (queue));

    // 4
    queue_push_tail (queue, cheese);
    assert (!queue_is_empty (queue));
    queue_push_tail (queue, bread);
    queue_push_tail (queue, wine);

    assert (queue_pop_head (queue) == cheese);
    assert (queue_pop_head (queue) == bread);
    assert (queue_pop_head (queue) == wine);
    assert (queue_is_empty (queue));

    // 5
    queue_push_tail (queue, cheese);
    queue_push_tail (queue, bread);
    assert (queue_peek_head (queue) == cheese);
    assert (queue_peek_tail (queue) == bread);
    queue_push_head (queue, wine);
    assert (queue_peek_head (queue) == wine);
    assert (queue_pop_tail (queue) == bread);
    assert (queue_pop_head (queue) == wine);
    assert (queue_pop_head (queue) == cheese);

    queue_free (&queue);
    assert (queue == NULL);
    print_info ("OK\n");
}
