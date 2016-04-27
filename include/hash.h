/*  =========================================================================
    hash - hash table container

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/
/*
@todo
- 修改hash_entry_t为void *handle
*/

#ifndef __HASH_H_INCLUDED__
#define __HASH_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _hash_t hash_t;

typedef void *hash_handle_t;

// Create a new hash table
hash_t *hash_new (hash_func_t hash_func,
                  equal_func_t equal_func,
                  free_func_t key_free_func,
                  free_func_t value_free_func);

// Destroy a hash table
void hash_free (hash_t **self_p);

// Get the number of entries in a hash table
size_t hash_size (hash_t *self);

// Insert a value into a hash table.
// Return the hash entry which you can save for fast update or removal.
// If the entry is guaranteed new one (entry with the key does not exist in
// the table), setting guaranteed_new to be true will make the insertion
// faster. Otherwise set it to be false, and then the old entry will be
// updated by the given one if their keys are equal.
hash_handle_t hash_insert (hash_t *self,
                           void *key,
                           void *value,
                           bool guaranteed_new);

// Look up a value in a hash table by key
void *hash_lookup (hash_t *self, const void *key);

// Remove an entry from a hash table by key
void hash_remove (hash_t *self, void *key);

// Remove an entry in a hash table
// This is much faster than removal by key.
void hash_remove_entry (hash_t *self, hash_handle_t handle);

// update an hash table entry given the entry
// Note: old and new key must be equal
void hash_update_entry (hash_t *self, hash_handle_t handle,
                        void *key, void *value);

// Get the first entry in hash table.
// Return NULL if hash table is empty.
// Use hash_first / hash_next() for simple iteration.
hash_handle_t hash_first (hash_t *self);

// Get next entry in hash table (order inrelevant)
// Return NULL if no more entry exists.
hash_handle_t hash_next (hash_t *self);

// Get key from hash entry
void *hash_key_from_entry (hash_handle_t handle);

// Get value from hash entry
void *hash_value_from_entry (hash_handle_t handle);

// Self test
void hash_test (bool verbose);

#ifdef __cplusplus
}
#endif

#endif
