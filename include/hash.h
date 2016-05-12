/*  =========================================================================
    hash - hash table container

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

#ifndef __HASH_H_INCLUDED__
#define __HASH_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _hash_t hash_t;

// Create a new hash table
hash_t *hash_new (hashfunc_t hash_func, matcher_t key_equal_func);

// Destroy a hash table
void hash_free (hash_t **self_p);

// Set destructors of key and value structures
void hash_set_destructors (hash_t *self,
                           destructor_t key_destructor,
                           destructor_t value_destructor);

// Set duplicators of key and value structures
void hash_set_duplicators (hash_t *self,
                           duplicator_t key_duplicator,
                           duplicator_t value_duplicator);

// Get the number of entries in a hash table
size_t hash_size (hash_t *self);

// Get key by item handle
void *hash_key (void *handle);

// Get value by item handle
void *hash_value (void *handle);

// Insert a key-value pair into a hash table.
// Return item handle which you can save for fast update or removal.
// If the item is guaranteed new one (item with the key does not exist in
// the table), setting guaranteed_new to be true will accelerate the insertion.
// Otherwise set it to be false, and then the old item will be updated by the
// given one if their keys are equal.
void *hash_insert (hash_t *self, void *key, void *value, bool guaranteed_new);

// Look up a value in a hash table by key
void *hash_lookup (hash_t *self, const void *key);

// Remove an item from hash table by key
void hash_remove (hash_t *self, void *key);

// Remove an item from hash table by handle
// This is much faster than removal by key.
void hash_remove_item (hash_t *self, void *handle);

// Update a hash table item by handle
// Note: old and new key must be equal
void hash_update_item (hash_t *self, void *handle, void *key, void *value);

// Get handle of the first item in hash table.
// Return NULL if hash table is empty.
// Use hash_first / hash_next() for simple iteration.
void *hash_first (hash_t *self);

// Get handle of the next item in hash table (order inrelevant)
// Return NULL if no more item exists.
void *hash_next (hash_t *self);

// Self test
void hash_test (bool verbose);

#ifdef __cplusplus
}
#endif

#endif
