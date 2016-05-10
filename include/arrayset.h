/*  =========================================================================
    arrayset - set with a built-in hash table

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/
/* @todo

考虑：

- 当不index external key时，目前不定义data唯一性，因此其实不是一个set，就是普通array，
所以是否把唯一性定义加上？需要定义equal_func(data1, data2)，并放到arrayset中，而不是
hash table中。

- [x]对照数据库知识，是否修改ext_key为foreign_key? id为primary key？hash为index？

- 增加支持多个index？

- arrayset似乎不太合适？重命名class：fasttable? dbtable?

- [x]add iterate support

- 是否有必要始终维持一个id array？

*/

#ifndef __ARRAYSET_H_INCLUDED__
#define __ARRAYSET_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _arrayset_t arrayset_t;

// Create a new arrayset.
// alloc_size is the initial size, or 0 to use a default value.
arrayset_t *arrayset_new (size_t alloc_size);

// Destroy an arrayset
void arrayset_free (arrayset_t **self_p);

// Set data free function.
// If data free function is set, arrayset is responsible for freeing data.
void arrayset_set_data_free_func (arrayset_t *self, free_func_t data_free_func);

// Set hash functions if you want to index a foreign key associated with data.
// Set foreign_key_free_func so that arrayset is responsible for freeing the key
// structure.
void arrayset_set_hash_funcs (arrayset_t *self,
                              hash_func_t hash_func,
                              equal_func_t foreign_key_equal_func,
                              free_func_t foreign_key_free_func);

// Get data by id.
void *arrayset_data (arrayset_t *self, size_t id);

// Get number of entries.
size_t arrayset_size (arrayset_t *self);

// Get maximum id value of data in the set.
// Return ID_NONE if the set is empty.
size_t arrayset_max_id (arrayset_t *self);

// Add a data to arrayset.
// foreign_key is an foreign key to be indexed by which the uniqueness
// is guaranteed.
// Set foreign_key to NULL if foreign key is not used, so that data is never
// replaced.
// Return id of data in the arrayset.
size_t arrayset_add (arrayset_t *self, void *data, void *foreign_key);

// Remove a data from arrayset
void arrayset_remove (arrayset_t *self, size_t id);

// Query data by foreign key
// Return data, and id as data's id if data is found; NULL if data not found.
// void *arrayset_query (arrayset_t *self, void *foreign_key, size_t *id);

// Query data by foreign key
// Return id if data is found; ID_NONE if not found
size_t arrayset_query (arrayset_t *self, const void *foreign_key);

// Get an array of data ids
size_t *arrayset_id_array (arrayset_t *self);

// Get an array of data
void **arrayset_data_array (arrayset_t *self);

// Get first data item in arrayset (with lowest id)
// Return NULL if the set is empty
// Combine arrayset_first() and arrayset_next() for iteration
void *arrayset_first (arrayset_t *self);

// Get the next data item (ascending id order).
// Return NULL if no more data exists.
void *arrayset_next (arrayset_t *self);

// Get last data item in arrayset (with highest id)
// Return NULL if the set is empty
void *arrayset_last (arrayset_t *self);

// Get the previous data item (descending id order).
// Return NULL if no more data exists.
void *arrayset_prev (arrayset_t *self);

// Self test
void arrayset_test (bool verbose);


#ifdef __cplusplus
}
#endif

#endif
