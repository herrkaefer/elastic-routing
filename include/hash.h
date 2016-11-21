/*  =========================================================================
    hash - hash table container

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#ifndef __HASH_H_INCLUDED__
#define __HASH_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

// Create a new hash table
hash_t *hash_new (hashfunc_t hashfunc, matcher_t key_matcher);

// Destroy a hash table
void hash_free (hash_t **self_p);

// Set destructors of key and value structures. Default: NULL.
// If set, hash table is responsible for freeing the key or value after using it.
void hash_set_destructors (hash_t *self,
                           destructor_t key_destructor,
                           destructor_t value_destructor);

// Set duplicators of key and value structures. Default: NULL.
// If set, key or value will be duplications are inserted to hash table rather
// than the original one.
void hash_set_duplicators (hash_t *self,
                           duplicator_t key_duplicator,
                           duplicator_t value_duplicator);

// Get the number of entries in a hash table
size_t hash_size (hash_t *self);

// Get item's key by item handle
void *hash_key (void *handle);

// Get item's value by item handle
void *hash_value (void *handle);

// Insert item into a hash table. If the item already exists, DO NOT update it.
// Return the item handle.
void *hash_insert (hash_t *self, void *key, void *value);

// Insert item without query. Caller should ensure the uniqueness of the key.
// Return the item handle.
void *hash_insert_nq (hash_t *self, void *key, void *value);

// Update item by key. If item does not exist, insert it.
// Return the item handle.
void *hash_update (hash_t *self, void *key, void *value);

// Look up item in a hash table by key.
// Return the item handle.
void *hash_lookup_item (hash_t *self, const void *key);

// Look up a value in a hash table by key.
// Return the value.
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
