/*  =========================================================================
    hash - implementation

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#include "classes.h"


typedef struct _hash_item_t {
	void *key;
	void *value;
	struct _hash_item_t *prev;
	struct _hash_item_t *next;
} s_hash_item_t;


struct _hash_t {
	s_hash_item_t *table;
	size_t table_size;
	size_t num_items;
	size_t prime_index;
	s_hash_item_t *cursor; // pointer for iteration
	size_t cursor_index; // chain index for iteration

	hashfunc_t   hashfunc;
	matcher_t 	 key_matcher;
	destructor_t key_destructor;
	destructor_t value_destructor;
	duplicator_t key_duplicator;
	duplicator_t value_duplicator;
};


// Set of good hash table prime numbers. See:
// http://planetmath.org/goodhashtableprimes
static const size_t hash_primes[] = {
	53, 97, 193, 389, 769, 1543, 3079, 6151, 12289, 24593, 49157, 98317,
	196613, 393241, 786433, 1572869, 3145739, 6291469, 12582917, 25165843,
	50331653, 100663319, 201326611, 402653189, 805306457, 1610612741,
};


static const size_t hash_primes_size = sizeof (hash_primes) / sizeof (size_t);


// Allocate the table
static void hash_alloc_table (hash_t *self) {
	assert (self);

	size_t new_table_size;

	// Determine the table size based on the current prime index
	if (self->prime_index < hash_primes_size)
		new_table_size = hash_primes[self->prime_index];
	else
		new_table_size = self->num_items * 10;

	self->table_size = new_table_size;

	// Allocate the table and initialise to NULL for all items
	self->table = calloc (self->table_size, sizeof (s_hash_item_t));
	assert (self->table);
}


// Destroy an item
static void hash_free_item (hash_t *self, s_hash_item_t *item) {
	assert (self);
	assert (item);

	// Destroy the key structure
	if (self->key_destructor != NULL)
		self->key_destructor (&(item->key));

	// Destroy the value structure
	if (self->value_destructor != NULL)
		self->value_destructor (&(item->value));

	// Destroy the item structure
	free (item);
}


// Set item
static void hash_set_item (hash_t *self,
						   s_hash_item_t *item,
						   void *key, void *value) {
	if (self->key_duplicator)
		key = self->key_duplicator (key);
	if (self->value_duplicator)
		value = self->value_duplicator (value);
	item->key = key;
	item->value = value;
}


// Link item into the first position at index
static void hash_link_item (hash_t *self, s_hash_item_t *item, size_t index) {
	s_hash_item_t *first_item = self->table[index].next;
	item->prev = &self->table[index];
	item->next = first_item;
	self->table[index].next = item;
	if (first_item != NULL)
		first_item->prev = item;
}


// Enlarge hash table size
static void hash_enlarge (hash_t *self) {
	assert (self);

	// Store a copy of the old table
	s_hash_item_t *old_table = self->table;
	size_t old_table_size = self->table_size;

	// Allocate a new, larger table
	++self->prime_index;
	hash_alloc_table (self);

	s_hash_item_t *rover, *next;
	size_t old_index, index;

	// Link all items from all chains into the new table
	for (old_index = 0; old_index < old_table_size; old_index++) {
		rover = old_table[old_index].next;
		while (rover != NULL) {
			next = rover->next;
			// Link rover into the chain of the new table
			index = self->hashfunc (rover->key) % self->table_size;
			hash_link_item (self, rover, index);
			rover = next;
		}
	}

	free (old_table);
}


//----------------------------------------------------------------------------


hash_t *hash_new (hashfunc_t hashfunc, matcher_t key_matcher) {
	assert (hashfunc);
	assert (key_matcher);

	hash_t *self = (hash_t *) malloc (sizeof (hash_t));
	assert (self);

	self->num_items = 0;
	self->prime_index = 0;
	self->cursor = NULL;
	self->cursor_index = 0;
	self->hashfunc = hashfunc;
	self->key_matcher = key_matcher;
	self->key_destructor = NULL;
	self->value_destructor = NULL;
	self->key_duplicator = NULL;
	self->value_duplicator = NULL;

	// Allocate the table
	hash_alloc_table (self);

	print_info ("hash created.\n");
	return self;
}


void hash_free (hash_t **self_p) {
	assert (self_p);
    if (*self_p) {
        hash_t *self = *self_p;

        // Destroy all items in all chains
        s_hash_item_t *rover;
		s_hash_item_t *next;
		for (size_t index = 0; index < self->table_size; ++index) {
			rover = self->table[index].next;
			while (rover != NULL) {
				next = rover->next;
				hash_free_item (self, rover);
				rover = next;
			}
		}

		// Destroy the table
		free (self->table);

        free (self);
        *self_p = NULL;
    }
    print_info ("hash freed.\n");
}


void hash_set_destructors (hash_t *self,
						   destructor_t key_destructor,
                           destructor_t value_destructor) {
	assert (self);
	assert (self->key_destructor == NULL);
	assert (self->value_destructor == NULL);
	self->key_destructor = key_destructor;
	self->value_destructor = value_destructor;
}


void hash_set_duplicators (hash_t *self,
                           duplicator_t key_duplicator,
                           duplicator_t value_duplicator) {
	assert (self);
	assert (self->key_duplicator == NULL);
	assert (self->value_duplicator == NULL);
	self->key_duplicator = key_duplicator;
	self->value_duplicator = value_duplicator;
}


size_t hash_size (hash_t *self) {
	assert (self);
	return self->num_items;
}


void *hash_insert_nq (hash_t *self, void *key, void *value) {
	assert (self);
	assert (key);
	assert (value);

	// If table is more than 1/2 full, enlarge it
	if ((self->num_items * 2) / self->table_size > 0)
		hash_enlarge (self);

	// Compute the index into the table
	size_t index = self->hashfunc (key) % self->table_size;

	// Create and insert a new item.
	s_hash_item_t *newitem =
		(s_hash_item_t *) malloc (sizeof (s_hash_item_t));
	assert (newitem);
	hash_set_item (self, newitem, key, value);
	hash_link_item (self, newitem, index);

	// Adjust the number of items
	++self->num_items;
	return newitem;
}


void *hash_insert (hash_t *self, void *key, void *value) {
	assert (self);
	assert (key);
	assert (value);
	s_hash_item_t *item = (s_hash_item_t *) hash_lookup_item (self, key);
	if (item)
		return item;
	else
		return hash_insert_nq (self, key, value);
}


void *hash_update (hash_t *self, void *key, void *value) {
	assert (self);
	assert (key);
	assert (value);
	s_hash_item_t *item = (s_hash_item_t *) hash_lookup_item (self, key);
	if (item) {
		if (item->key != key && self->key_destructor != NULL)
			self->key_destructor (&item->key);
		if (item->value != value && self->value_destructor != NULL)
			self->value_destructor (&item->value);
		hash_set_item (self, item, key, value);
		return item;
	}
	else
		return hash_insert_nq (self, key, value);
}


void *hash_lookup_item (hash_t *self, const void *key) {
	assert (self);
	size_t index = self->hashfunc (key) % self->table_size;
	s_hash_item_t *rover = self->table[index].next;
	while (rover != NULL) {
		if (self->key_matcher (key, rover->key))
			return rover;
		rover = rover->next;
	}
	return NULL;
}


void *hash_lookup (hash_t *self, const void *key) {
	assert (self);
	s_hash_item_t *item = (s_hash_item_t *) hash_lookup_item (self, key);
	return item ? item->value : NULL;
}


void hash_remove (hash_t *self, void *key) {
	assert (self);
	assert (key);
	void *item = hash_lookup_item (self, key);
	if (item) {
		hash_remove_item (self, item);
		return;
	}
	print_warning ("item not found.\n");
}


void hash_remove_item (hash_t *self, void *handle) {
	assert (self);
	assert (handle);

	s_hash_item_t *item = (s_hash_item_t *) handle;

	// Unlink item from list
	item->prev->next = item->next;
	if (item->next != NULL)
		item->next->prev = item->prev;

	// Free item
	hash_free_item (self, item);

	// Track count of items
	--self->num_items;
}


void hash_update_item (hash_t *self, void *handle, void *key, void *value) {
	assert (self);
	assert (handle);
	assert (key);
	assert (value);

	s_hash_item_t *item = (s_hash_item_t *) handle;
	assert (self->key_matcher (item->key, key));

	if (item->key != key && self->key_destructor != NULL)
		self->key_destructor (&item->key);
	if (item->value != value && self->value_destructor != NULL)
		self->value_destructor (&item->value);

	hash_set_item (self, item, key, value);
}


void *hash_first (hash_t *self) {
	assert (self);
	self->cursor = NULL;
	for (self->cursor_index = 0;
		 self->cursor_index < self->table_size;
		 self->cursor_index ++) {
		self->cursor = self->table[self->cursor_index].next;
		if (self->cursor != NULL)
			return self->cursor;
	}
	return NULL;
}


void *hash_next (hash_t *self) {
	assert (self);
	if (self->cursor == NULL)
		return NULL;

	self->cursor = self->cursor->next;
	if (self->cursor != NULL)
		return self->cursor;

	// Advance the chain index
	for (self->cursor_index ++;
		 self->cursor_index < self->table_size;
		 self->cursor_index ++) {
		self->cursor = self->table[self->cursor_index].next;
		if (self->cursor != NULL)
			return self->cursor;
	}
	return self->cursor;
}


void *hash_key (void *handle) {
	assert (handle);
	return ((s_hash_item_t *)handle)->key;
}


void *hash_value (void *handle) {
	assert (handle);
	return ((s_hash_item_t *)handle)->value;
}


void hash_test (bool verbose) {
	print_info (" * hash: \n");
    hash_t *hash = hash_new ((hashfunc_t) string_hash,
    						 (matcher_t) string_equal);

	assert (hash);
	assert (hash_size (hash) == 0);
	assert (hash_first (hash) == NULL);

	// Insert
	void *handle;
	handle = hash_insert (hash, "PADDINGTON", "goes to market");
	assert (handle);
	assert (streq (hash_key (handle), "PADDINGTON"));
	assert (streq (hash_value (handle), "goes to market"));

	handle = hash_insert (hash, "MIFFY", "on a scoot");
	assert (handle);
	handle = hash_insert (hash, "MAISY", "goes shopping");
	assert (handle);
	handle = hash_insert (hash, "BUDDY", "plays a ball");
	assert (handle);
	assert (hash_size (hash) == 4);

	// Look for existing items
	char *item = (char *) hash_lookup (hash, "PADDINGTON");
	assert (streq (item, "goes to market"));
	item = (char *) hash_lookup (hash, "MAISY");
	assert (streq (item, "goes shopping"));
	item = (char *) hash_lookup (hash, "BUDDY");
	assert (streq (item, "plays a ball"));
	item = (char *) hash_lookup (hash, "MIFFY");
	assert (streq (item, "on a scoot"));

	// Iteration
	handle = hash_first (hash);
	while (handle != NULL) {
		string_print (hash_key (handle));
		string_print (hash_value (handle));
		handle = hash_next (hash);
	}

	// Look for non-existent items
	item = (char *) hash_lookup (hash, "PIGGY");
	assert (item == NULL);

	// Try to insert duplicate items (no update)
	hash_insert (hash, "MIFFY", "visit a friend");
	item = (char *) hash_lookup (hash, "MIFFY");
	assert (streq (item, "on a scoot"));

	// Delete a item
	hash_remove (hash, "MAISY");
	item = (char *) hash_lookup (hash, "MAISY");
	assert (item == NULL);
	assert (hash_size (hash) == 3);



	// Check that the queue is robust against random usage
	struct {
		char name [100];
	 	bool exists;
	} testset [200];
	memset (testset, 0, sizeof (testset));
	int testmax = 200, testnbr, iteration;

	rng_t *rng = rng_new ();
	for (iteration = 0; iteration < 25000; iteration++) {
		testnbr = rng_random_int (rng, 0, testmax);
		if (testset [testnbr].exists) {
			item = (char *) hash_lookup (hash, testset [testnbr].name);
			assert (item);
			hash_remove (hash, testset [testnbr].name);
			testset [testnbr].exists = false;
		}
		else {
			sprintf (testset [testnbr].name, "%.5f-%.5f",
						rng_random (rng), rng_random (rng));
			handle = hash_insert (hash, testset [testnbr].name, "");
			assert (handle);
			testset [testnbr].exists = true;
		}
	}
	rng_free(&rng);

	// Test 10K lookups
	for (iteration = 0; iteration < 10000; iteration++)
		item = (char *) hash_lookup (hash, "HAIDIMANYOU");

    hash_free (&hash);
    assert (hash == NULL);
    print_info ("OK\n");
}
