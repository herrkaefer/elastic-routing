/*  =========================================================================
    arrayset - set with a built-in hash table

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
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

// Create a new arrayset.
// alloc_size is the initial size, or 0 to use a default value.
arrayset_t *arrayset_new (size_t alloc_size);

// Destroy an arrayset
void arrayset_free (arrayset_t **self_p);

// Set data destructor. Default: NULL.
// If this is set, arrayset is responsible for freeing data after using them.
void arrayset_set_data_destructor (arrayset_t *self,
                                   destructor_t data_destructor);

// Set hash functions if you want to index a foreign key associated with data.
// Default: no hash.
// Set foreign_key_destructor so that arrayset is responsible for freeing the
// key structure, or NULL if caller will free the key structure.
void arrayset_set_hash (arrayset_t *self,
                        hashfunc_t hash_func,
                        matcher_t foreign_key_matcher,
                        destructor_t foreign_key_destructor);

// Get data by id.
void *arrayset_data (arrayset_t *self, size_t id);

// Get number of entries.
size_t arrayset_size (arrayset_t *self);

// Get maximum id value of data in the set.
// Return ID_NONE if the set is empty.
size_t arrayset_max_id (arrayset_t *self);

// Add data to arrayset, and fail if data exists.
// Return data id if added, ID_NONE if failed.
// If foreign_key is not set, data is always added.
size_t arrayset_add (arrayset_t *self, void *data, void *foreign_key);

// Add data to arrayset, and update data if it exists.
// Return data id.
size_t arrayset_update (arrayset_t *self, void *data, void *foreign_key);

// Remove data from arrayset by id
void arrayset_remove (arrayset_t *self, size_t id);

// Query data by foreign key.
// Return id if data is found; ID_NONE if not found.
size_t arrayset_query (arrayset_t *self, const void *foreign_key);

// Get an array of data ids.
// Owner: caller.
size_t *arrayset_id_array (arrayset_t *self);

// Get an array of data (references array, datas are not copied out).
// Owner: caller.
void **arrayset_data_array (arrayset_t *self);

// Get first data item in arrayset (with lowest id).
// Return NULL if the set is empty.
// Combine arrayset_first() and arrayset_next() for iteration. But note that
// iterations can not be nested.
void *arrayset_first (arrayset_t *self);

// Get the next data item (ascending id order).
// Return NULL if no more data exists.
void *arrayset_next (arrayset_t *self);

// Get last data item in arrayset (with highest id).
// Return NULL if the set is empty.
// Combine arrayset_last() and arrayset_prev() for iteration. But note that
// iterations can not be nested.
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
