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
hash_t *hash_new (hash_func_t hash_func,
                  equal_func_t equal_func,
                  free_func_t key_free_func,
                  free_func_t value_free_func);

// Destroy a hash table
void hash_free (hash_t **self_p);

// Get the number of entries in a hash table
size_t hash_size (hash_t *self);

// Insert a key-value pair into a hash table.
// Return handle of the entry which you can save for fast update or removal.
// If the entry is guaranteed new one (entry with the key does not exist in
// the table), setting guaranteed_new to be true will accelerate the insertion.
// Otherwise set it to be false, and then the old entry will be updated by the
// given one if their keys are equal.
void *hash_insert (hash_t *self, void *key, void *value, bool guaranteed_new);

// Look up a value in a hash table by key
void *hash_lookup (hash_t *self, const void *key);

// Remove an entry from hash table by key
void hash_remove (hash_t *self, void *key);

// Remove an entry from hash table by handle
// This is much faster than removal by key.
void hash_remove_entry (hash_t *self, void *handle);

// Update a hash table entry by handle
// Note: old and new key must be equal
void hash_update_entry (hash_t *self, void *handle, void *key, void *value);

// Get handle of the first entry in hash table.
// Return NULL if hash table is empty.
// Use hash_first / hash_next() for simple iteration.
void *hash_first (hash_t *self);

// Get handle of the next entry in hash table (order inrelevant)
// Return NULL if no more entry exists.
void *hash_next (hash_t *self);

// Get key from hash entry
void *hash_entry_key (void *handle);

// Get value from hash entry
void *hash_entry_value (void *handle);

// Self test
void hash_test (bool verbose);

#ifdef __cplusplus
}
#endif

#endif
