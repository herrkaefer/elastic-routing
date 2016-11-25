/*  =========================================================================
    arrayset - implementation

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#include "classes.h"

#define ARRAYSET_DEFAULT_ALLOC_SIZE 16


typedef struct {
    size_t id; // i.e. index in entries
    bool valid; // true: in set, false: removed from set
    void *data;
    void *hash_handle; // handle of entry in hash table of foreign key
} arrayset_entry_t;


struct _arrayset_t {
    arrayset_entry_t *entries;
    size_t size; // number of entries
    size_t length; // number of entries plus holes
    size_t alloced; // size of alloced memory
    size_t cursor; // entry id for iteration

    queue_t *holes; // record of removed nodes. A FIFO queue.

    hash_t *hash; // an optional hash table for indexing data by foreign key
    destructor_t destructor; // free function for data
};


// ---------------------------------------------------------------------------
static void arrayset_set_entry (arrayset_t *self,
                                arrayset_entry_t *entry,
                                size_t id,
                                bool valid,
                                void *data,
                                void *hash_handle) {
    assert (self);
    assert (entry);
    assert (data);

    entry->id = id;
    entry->valid = valid;
    if (entry->data != NULL && self->destructor != NULL)
        self->destructor (&entry->data);
    entry->data = data;
    entry->hash_handle = hash_handle;
}


static void arrayset_enlarge (arrayset_t *self) {
    assert (self);

    // Double the pool size
    unsigned int newsize = self->alloced * 2;
    arrayset_entry_t *new_entry_pool =
        (arrayset_entry_t *) realloc (self->entries,
                                      sizeof (arrayset_entry_t) * newsize);
    assert (new_entry_pool);

    self->entries = new_entry_pool;
    self->alloced = newsize;
}


// Insert data to arrayset at index
static void arrayset_insert_at (arrayset_t *self,
                                size_t index,
                                void *data,
                                void *foreign_key) {
    assert (index <= self->length);
    assert (data);

    if (index == self->length && self->length + 1 > self->alloced)
        arrayset_enlarge (self);

    arrayset_entry_t *entry = self->entries + index;

    void *hash_handle = NULL;

    // If foreign key is indexed, add it to hash table
    if (foreign_key) {
        assert (self->hash);
        hash_handle = hash_insert_nq (self->hash, foreign_key, entry);
    }

    ++self->size;

    // if position is used for the first time (or length is increased)
    if (index == self->length) {
        ++self->length;
        entry->data = NULL;
    }

    arrayset_set_entry (self, entry, index, true, data, hash_handle);
}


// Add data without query
static size_t arrayset_insert (arrayset_t *self,
                               void *data,
                               void *foreign_key) {
    assert (data);
    size_t id;

    // try to get a hole
    void *hole = queue_pop_head (self->holes);

    // If hole exists, place the entry there
    if (hole != NULL) {
        arrayset_entry_t *entry = (arrayset_entry_t *) hole;
        id = entry->id;
        arrayset_insert_at (self, id, data, foreign_key);
        return id;
    }

    // If no hole exists, append entry to the end
    id = self->length;
    arrayset_insert_at (self, id, data, foreign_key);
    return id;
}


// ---------------------------------------------------------------------------
arrayset_t *arrayset_new (size_t alloc_size) {
    arrayset_t *self = (arrayset_t *) malloc (sizeof (arrayset_t));
    assert (self);

    if (alloc_size == 0)
        alloc_size = ARRAYSET_DEFAULT_ALLOC_SIZE;
    self->alloced = alloc_size;
    self->size = 0;
    self->length = 0;

    self->entries = malloc (alloc_size * sizeof (arrayset_entry_t));
    assert (self->entries);

    self->holes = queue_new ();
    self->hash = NULL;
    self->destructor = NULL;

    print_info ("arrayset created.\n");
    return self;
}


void arrayset_free (arrayset_t **self_p) {
    assert (self_p);
    if (*self_p) {
        arrayset_t *self = *self_p;
        if (self->hash)
            hash_free (&self->hash);

        queue_free (&self->holes);

        if (self->destructor != NULL) {
            for (size_t id = 0; id < self->length; id++)
                self->destructor (&self->entries[id].data);
        }
        free (self->entries);

        free (self);
        *self_p = NULL;
    }
    print_info ("arrayset freed.\n");
}


void arrayset_set_data_destructor (arrayset_t *self,
                                   destructor_t data_destructor) {
    assert (self);
    assert (self->destructor == NULL);
    self->destructor = data_destructor;
}


void arrayset_set_hash (arrayset_t *self,
                        hashfunc_t hash_func,
                        matcher_t foreign_key_matcher,
                        destructor_t foreign_key_destructor) {
    assert (self);
    assert (hash_func);
    assert (foreign_key_matcher);
    assert (self->hash == NULL);

    self->hash = hash_new (hash_func, foreign_key_matcher);
    assert (self->hash);
    hash_set_destructors (self->hash, foreign_key_destructor, NULL);
    assert (self->hash);
}


void *arrayset_data (arrayset_t *self, size_t id) {
    assert (self);
    if (id >= self->length || !(self->entries[id].valid))
        return NULL;
    else
        return self->entries[id].data;
}


size_t arrayset_size (arrayset_t *self) {
    assert (self);
    return self->size;
}


size_t arrayset_max_id (arrayset_t *self) {
    assert (self);

    if (self->size == 0)
        return ID_NONE;

    size_t max_id = self->size - 1;
    for (size_t id = self->length - 1; id > self->size - 1; id--) {
        if (self->entries[id].valid) {
            max_id = id;
            break;
        }
    }
    return max_id;
}


size_t arrayset_add (arrayset_t *self, void *data, void *foreign_key) {
    assert (self);
    assert (data);
    assert ((foreign_key == NULL) || (foreign_key && self->hash));

    // If foreign key is indexed and data already exists, update the data
    if (foreign_key && arrayset_query (self, foreign_key) != ID_NONE) {
        print_warning ("data already exists in arrayset. Adding data failed.\n");
        return ID_NONE;
    }

    // If foreign key is not indexed, or data does not exists, insert it.
    return arrayset_insert (self, data, foreign_key);
}


size_t arrayset_update (arrayset_t *self, void *data, void *foreign_key) {
    assert (self);
    assert (data);
    assert ((foreign_key == NULL) || (foreign_key && self->hash));

    size_t id;

    // If foreign key is indexed and data already exists, update the data
    if (foreign_key) {
        id = arrayset_query (self, foreign_key);
        if (id != ID_NONE) {
            print_warning ("data already exists in arrayset with id: %zu."
                           "Replaced with new data.\n", id);
            arrayset_entry_t *entry = &self->entries[id];
            assert (entry->valid);

            // update entry's data
            if (entry->data != NULL && self->destructor != NULL)
                self->destructor (&entry->data);
            entry->data = data;

            return id;
        }
    }

    // If foreign key is not indexed, or data does not exists,
    return arrayset_insert (self, data, foreign_key);
}


void arrayset_remove (arrayset_t *self, size_t id) {
    assert (self);
    assert (id < self->length);

    arrayset_entry_t *entry = &self->entries[id];
    assert (entry->valid);

    entry->valid = false;

    if (self->hash) {
        hash_remove_item (self->hash, entry->hash_handle);
        entry->hash_handle = NULL;
    }

    // Free data if needed
    if (entry->data != NULL && self->destructor != NULL)
        self->destructor (&entry->data);

    // Add entry to holes queue
    queue_push_tail (self->holes, (void *)entry);

    --self->size;
}


size_t arrayset_query (arrayset_t *self, const void *foreign_key) {
    assert (self);
    assert (foreign_key);
    assert (self->hash);

    arrayset_entry_t *entry =
        (arrayset_entry_t *) hash_lookup (self->hash, foreign_key);
    return entry ? entry->id : ID_NONE;
}


size_t *arrayset_id_array (arrayset_t *self) {
    assert (self);

    size_t *array = (size_t *) malloc (sizeof (size_t) * self->size);
    assert (array);

    size_t count = 0;
    for (size_t id = 0; id < self->length; id++) {
        if (self->entries[id].valid) {
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

    size_t cnt = 0;
    arrayset_entry_t *entry = NULL;
    for (size_t id = 0; id < self->length; id++) {
        entry = &self->entries[id];
        if (entry->valid) {
            array[cnt] = entry->data;
            cnt++;
        }
    }
    assert (cnt == self->size);
    return array;
}


void *arrayset_first (arrayset_t *self) {
    assert (self);
    arrayset_entry_t *entry;
    for (self->cursor = 0; self->cursor < self->length; self->cursor++) {
        entry = &self->entries[self->cursor];
        if (entry->valid)
            return entry->data;
    }
    return NULL;
}


void *arrayset_next (arrayset_t *self) {
    assert (self);
    arrayset_entry_t *entry;
    for (self->cursor++; self->cursor < self->length; self->cursor++) {
        entry = &(self->entries[self->cursor]);
        if (entry->valid)
            return entry->data;
    }
    return NULL;
}


void *arrayset_last (arrayset_t *self) {
    assert (self);
    arrayset_entry_t *entry;
    for (self->cursor = self->length; self->cursor -- > 0; ) {
        entry = &self->entries[self->cursor];
        if (entry->valid)
            return entry->data;
    }
    return NULL;
}


void *arrayset_prev (arrayset_t *self) {
    assert (self);
    arrayset_entry_t *entry;
    for (; self->cursor -- > 0; ) {
        entry = &(self->entries[self->cursor]);
        if (entry->valid)
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

    arrayset_set_hash (as,
                       (hashfunc_t) string_hash,
                       (matcher_t) string_equal,
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
