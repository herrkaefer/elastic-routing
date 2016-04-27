/*  =========================================================================
    arrayset - database-table-like set

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

#include "classes.h"


typedef struct {
    size_t id; // i.e. index in entry_pool
    bool is_valid; // false: removed
    void *data;
    hash_handle_t hash_handle; // hash table entry of index of
                                        // foreign key
} arrayset_entry_t;


struct _arrayset_t {
    arrayset_entry_t *entry_pool;
    size_t size; // number of entries
    size_t length; // number of entries and holes
    size_t alloced; // size of alloced memory
    size_t cursor; // entry id for iteration

    queue_t *holes; // Record of removed nodes. A FIFO queue.

    hash_t *hash; // A hash table of data by foreign key.
                  // This is optional.

    free_func_t data_free_func;
};


// ---------------------------------------------------------------------------
static void arrayset_set_entry (arrayset_t *self,
                                arrayset_entry_t *entry,
                                size_t id,
                                bool is_valid,
                                void *data,
                                hash_handle_t hash_handle) {
    assert (self);
    assert (entry);
    assert (data);

    entry->id = id;
    entry->is_valid = is_valid;
    if (entry->data != NULL && self->data_free_func != NULL)
        self->data_free_func (&entry->data);
    entry->data = data;
    entry->hash_handle = hash_handle;
}


static void arrayset_enlarge (arrayset_t *self) {
    assert (self);

    // Double the pool size
    unsigned int newsize = self->alloced * 2;
    arrayset_entry_t *new_entry_pool =
        (arrayset_entry_t *) realloc (self->entry_pool,
                                      sizeof (arrayset_entry_t) * newsize);
    assert (new_entry_pool);

    self->entry_pool = new_entry_pool;
    self->alloced = newsize;
}


// Insert data to arrayset at index
static void arrayset_insert (arrayset_t *self,
                             size_t index,
                             void *data,
                             void *foreign_key) {
    assert (self);
    assert (index <= self->length);
    assert (data);
    // assert (foreign_key);

    if (index == self->length && self->length + 1 > self->alloced)
        arrayset_enlarge (self);

    arrayset_entry_t *entry = self->entry_pool + index;

    hash_handle_t hash_handle = NULL;

    // If foreign key is indexed, add it to hash table
    if (foreign_key) {
        assert (self->hash);
        hash_handle = hash_insert (self->hash,
                                  foreign_key,
                                  (void *) entry,
                                  true);
    }

    ++self->size;

    // if entry is used for the first time (or length is increased)
    if (index == self->length) {
        ++self->length;
        entry->data = NULL;
    }

    arrayset_set_entry (self, entry, index, true, data, hash_handle);
}


// ---------------------------------------------------------------------------
arrayset_t *arrayset_new (size_t alloc_size) {
    if (alloc_size == 0)
        alloc_size = 128;

    arrayset_t *self = (arrayset_t *) malloc (sizeof (arrayset_t));
    assert (self);

    self->alloced = alloc_size;
    self->size = 0;
    self->length = 0;

    self->entry_pool = malloc (alloc_size * sizeof (arrayset_entry_t));
    assert (self->entry_pool);

    self->holes = queue_new ();
    assert (self->holes);

    self->hash = NULL;
    self->data_free_func = NULL;

    print_info ("arrayset created.\n");
    return self;
}


void arrayset_set_data_free_func (arrayset_t *self,
                                  free_func_t data_free_func) {
    assert (self);
    assert (self->data_free_func == NULL);
    self->data_free_func = data_free_func;
}


void arrayset_set_hash_funcs (arrayset_t *self,
                              hash_func_t hash_func,
                              equal_func_t foreign_key_equal_func,
                              free_func_t foreign_key_free_func) {
    assert (self);
    assert (hash_func);
    assert (foreign_key_equal_func);
    assert (self->hash == NULL);

    self->hash = hash_new (hash_func,
                           foreign_key_equal_func,
                           foreign_key_free_func,
                           NULL);
    assert (self->hash);
}


void arrayset_free (arrayset_t **self_p) {
    assert (self_p);
    if (*self_p) {
        arrayset_t *self = *self_p;
        if (self->hash)
            hash_free (&self->hash);

        queue_free (&self->holes);

        if (self->data_free_func != NULL) {
            for (size_t id = 0; id < self->length; id++)
                self->data_free_func (&self->entry_pool[id].data);
        }
        free (self->entry_pool);

        free (self);
        *self_p = NULL;
    }
    print_info ("arrayset freed.\n");
}


void *arrayset_data (arrayset_t *self, size_t id) {
    assert (self);
    if (id >= self->length || !self->entry_pool[id].is_valid)
        return NULL;
    else
        return self->entry_pool[id].data;
}


size_t arrayset_size (arrayset_t *self) {
    assert (self);
    return self->size;
}


size_t arrayset_max_id (arrayset_t *self) {
    assert (self);
    assert (self->size > 0);
    size_t max_id = self->size - 1;
    for (size_t id = self->length - 1; id >= self->size - 1; id--) {
        if (self->entry_pool[id].is_valid) {
            max_id = id;
            break;
        }
    }
    return max_id;
}


size_t arrayset_add (arrayset_t *self, void *data, void *foreign_key) {
    assert (self);
    assert (data);

    size_t id;

    // If foreign key is indexed, and data already exists,
    // update the data
    if (foreign_key) {
        id = arrayset_query (self, foreign_key);
        if (id != ID_NONE) {
            print_warning ("Same data already exists in arrayset. id: %zu\n", id);
            arrayset_entry_t *entry = &self->entry_pool[id];
            assert (entry->is_valid);

            // update entry's data
            if (entry->data != NULL && self->data_free_func != NULL)
                self->data_free_func (&entry->data);
            entry->data = data;

            return id;
        }
    }

    // If foreign key is not indexed, or data does not exists,
    // try to get a hole
    void *hole = queue_pop_head (self->holes);

    // hole exists, place the entry there
    if (hole != NULL) {
        arrayset_entry_t *entry = (arrayset_entry_t *) hole;
        id = entry->id;
        arrayset_insert (self, id, data, foreign_key);
        return id;
    }

    // No hole exists, append entry to the end
    id = self->length;
    arrayset_insert (self, id, data, foreign_key);
    return id;
}


void arrayset_remove (arrayset_t *self, size_t id) {
    assert (self);
    assert (id < self->length);

    arrayset_entry_t *entry = &self->entry_pool[id];
    assert (entry->is_valid);

    entry->is_valid = false;

    if (self->hash) {
        hash_remove_entry (self->hash, entry->hash_handle);
        entry->hash_handle = NULL;
    }

    // Free data
    if (entry->data != NULL && self->data_free_func != NULL) {
        self->data_free_func (&entry->data);
        // entry->data = NULL;
    }

    // Add entry to holes queue
    queue_push_tail (self->holes, (void *)entry);

    --self->size;
}


// void *arrayset_query (arrayset_t *self, void *foreign_key, size_t *id) {
//     assert (self);
//     assert (foreign_key);
//     assert (self->hash);
//     // assert (id);

//     arrayset_entry_t *entry =
//         (arrayset_entry_t *) hash_lookup (self->hash, foreign_key);
//     if (entry) {
//         if (id != NULL)
//             *id = entry->id;
//         return entry->data;
//     }
//     else
//         return NULL;
// }


size_t arrayset_query (arrayset_t *self, const void *foreign_key) {
    assert (self);
    assert (foreign_key);
    assert (self->hash);

    arrayset_entry_t *entry =
        (arrayset_entry_t *) hash_lookup (self->hash, foreign_key);
    if (entry)
        return entry->id;
    else
        return ID_NONE;
}


size_t *arrayset_id_array (arrayset_t *self) {
    assert (self);

    size_t *array = (size_t *) malloc (sizeof (size_t) * self->size);
    assert (array);

    size_t count = 0;
    for (size_t id = 0; id < self->length; id++) {
        if (self->entry_pool[id].is_valid) {
            array[count] = id;
            count++;
        }
    }
    assert (count == self->size);
    return array;
}


void **arrayset_data_array (arrayset_t *self) {
    assert (self);

    void **array =
        (void **) malloc (sizeof (void *) * self->size);
    assert (array);

    size_t count = 0;
    arrayset_entry_t *entry = NULL;
    for (size_t id = 0; id < self->length; id++) {
        entry = &self->entry_pool[id];
        if (entry->is_valid) {
            array[count] = entry->data;
            count++;
        }
    }
    assert (count == self->size);
    return array;
}


void *arrayset_first (arrayset_t *self) {
    assert (self);
    arrayset_entry_t *entry;
    for (self->cursor = 0; self->cursor < self->length; self->cursor++) {
        entry = &self->entry_pool[self->cursor];
        if (entry->is_valid)
            return entry->data;
    }
    return NULL;
}


void *arrayset_next (arrayset_t *self) {
    assert (self);
    arrayset_entry_t *entry;
    for (self->cursor++; self->cursor < self->length; self->cursor++) {
        entry = &(self->entry_pool[self->cursor]);
        if (entry->is_valid)
            return entry->data;
    }
    return NULL;
}


void *arrayset_last (arrayset_t *self) {
    assert (self);
    arrayset_entry_t *entry;
    for (self->cursor = self->length; self->cursor -- > 0; ) {
        entry = &self->entry_pool[self->cursor];
        if (entry->is_valid)
            return entry->data;
    }
    return NULL;
}


void *arrayset_prev (arrayset_t *self) {
    assert (self);
    arrayset_entry_t *entry;
    for (; self->cursor -- > 0; ) {
        entry = &(self->entry_pool[self->cursor]);
        if (entry->is_valid)
            return entry->data;
    }
    return NULL;
}


// @todo
void arrayset_test (bool verbose) {
    print_info (" * arrayset: \n");
    arrayset_t *as = arrayset_new (0);
    assert (as);
    assert (arrayset_size (as) == 0);

    arrayset_set_hash_funcs (as,
                             (hash_func_t) string_hash,
                             (equal_func_t) string_equal,
                             NULL);

    size_t id;

    id = arrayset_add (as, "one", "1");
    assert (id == 0);
    assert (arrayset_size (as) == 1);

    id = arrayset_add (as, "two", "2");
    assert (id == 1);
    assert (arrayset_size (as) == 2);

    id = arrayset_add (as, "three", "3");
    assert (id == 2);
    assert (arrayset_size (as) == 3);

    arrayset_remove (as, 1);
    assert (arrayset_size (as) == 2);

    arrayset_remove (as, 0);
    assert (arrayset_size (as) == 1);

    id = arrayset_add (as, "four", "4");
    assert (id == 1);
    assert (arrayset_size (as) == 2);

    id = arrayset_add (as, "five", "5");
    assert (id == 0);
    assert (arrayset_size (as) == 3);

    id = arrayset_add (as, "six", "6");
    assert (id == 3);
    assert (arrayset_size (as) == 4);

    size_t *id_array = arrayset_id_array (as);
    void **data_array = arrayset_data_array (as);
    for (size_t ind = 0; ind < arrayset_size (as); ind++)
        printf ("id: %zu, data: %s\n", id_array[ind], (char *) data_array[ind]);


    // iteration
    for (void *data = arrayset_first (as);
        data != NULL;
        data = arrayset_next (as)) {
        printf ("iter data: %s\n", (char *) data);
    }

    for (void *data = arrayset_last (as);
        data != NULL;
        data = arrayset_prev (as)) {
        printf ("iter data: %s\n", (char *) data);
    }

    free (id_array);
    free (data_array);
    arrayset_free (&as);
    print_info ("OK\n");
}
