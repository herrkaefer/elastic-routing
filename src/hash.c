/*  =========================================================================
    hash - hash table container

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

	This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

#include "classes.h"


typedef struct _hash_entry_t {
	void *key;
	void *value;
	struct _hash_entry_t *prev;
	struct _hash_entry_t *next;
} hash_entry_t;


struct _hash_t {
	hash_entry_t *table;
	size_t table_size;
	size_t num_entries;
	size_t prime_index;
	hash_entry_t *cursor; // pointer for iteration
	size_t cursor_index; // chain index of cursor
	hash_func_t hash_func;
	equal_func_t equal_func;
	free_func_t key_free_func;
	free_func_t value_free_func;
};


// This is a set of good hash table prime numbers, from:
// http://planetmath.org/encyclopedia/GoodHashPrimes.html
// Each prime is roughly double the previous value, and as far as
// possible from the nearest powers of two.
static const size_t hash_primes[] = {
	193, 389, 769, 1543, 3079, 6151, 12289, 24593, 49157, 98317,
	196613, 393241, 786433, 1572869, 3145739, 6291469,
	12582917, 25165843, 50331653, 100663319, 201326611,
	402653189, 805306457, 1610612741,
};


static const size_t hash_num_primes = sizeof (hash_primes) / sizeof (int);


// Allocate the table
static void hash_alloc_table (hash_t *self) {
	assert (self);

	size_t new_table_size;

	// Determine the table size based on the current prime index.
	// An attempt is made here to ensure sensible behavior if the
	// maximum prime is exceeded, but in practice other things are
	// likely to break long before that happens.

	if (self->prime_index < hash_num_primes)
		new_table_size = hash_primes[self->prime_index];
	else
		new_table_size = self->num_entries * 10;

	self->table_size = new_table_size;

	// Allocate the table and initialise to NULL for all entries
	self->table = calloc (self->table_size, sizeof (hash_entry_t));
	assert (self->table);
}


// Free an entry
static void hash_free_entry (hash_t *self, hash_entry_t *entry) {
	assert (self);
	assert (entry);

	// If there is a function registered for freeing keys, use it to free
	// the key
	if (self->key_free_func != NULL)
		self->key_free_func (&entry->key);

	// Likewise with the value
	if (self->value_free_func != NULL)
		self->value_free_func (&entry->value);

	// Free the data structure
	free (entry);
}


// Enlarge hash table size
static void hash_enlarge (hash_t *self) {
	assert (self);

	// Store a copy of the old table
	hash_entry_t *old_table = self->table;
	size_t old_table_size = self->table_size;
	// size_t old_prime_index = self->prime_index;

	// Allocate a new, larger table
	++self->prime_index;
	hash_alloc_table (self);

	hash_entry_t *rover;
	hash_entry_t *next;
	hash_entry_t *first_entry;
	size_t old_index;
	size_t index;

	// Link all entries from all chains into the new table
	for (old_index = 0; old_index < old_table_size; old_index++) {
		rover = old_table[old_index].next;

		while (rover != NULL) {
			next = rover->next;

			// Find the index into the new table
			index = self->hash_func (rover->key) % self->table_size;

			// Link this entry into the chain
			first_entry = self->table[index].next;
			rover->prev = &self->table[index];
			rover->next = first_entry;
			first_entry->prev = rover;
			self->table[index].next = rover;

			// Advance to next in the chain
			rover = next;
		}
	}

	// Free the old table
	free (old_table);
}


//----------------------------------------------------------------------------


hash_t *hash_new (hash_func_t hash_func,
                  equal_func_t equal_func,
                  free_func_t key_free_func,
                  free_func_t value_free_func) {
	assert (hash_func);
	assert (equal_func);

	hash_t *self = (hash_t *) malloc (sizeof (hash_t));
	assert (self);

	self->num_entries = 0;
	self->prime_index = 0;
	self->hash_func = hash_func;
	self->equal_func = equal_func;
	self->key_free_func = key_free_func;
	self->value_free_func = value_free_func;

	// Allocate the table
	hash_alloc_table (self);

	print_info ("hash table created.\n");
	return self;
}


void hash_free (hash_t **self_p) {
	assert (self_p);
    if (*self_p) {
        hash_t *self = *self_p;

        // Free all entries in all chains
        hash_entry_t *rover;
		hash_entry_t *next;
		for (size_t index = 0; index < self->table_size; ++index) {
			rover = self->table[index].next;
			while (rover != NULL) {
				next = rover->next;
				hash_free_entry (self, rover);
				rover = next;
			}
		}

		// Free the table
		free (self->table);

        free (self);
        *self_p = NULL;
    }
    print_info ("hash table freed.\n");
}


size_t hash_size (hash_t *self) {
	assert (self);
	return self->num_entries;
}


void *hash_insert (hash_t *self,
	                   	   void *key,
                       	   void *value,
                       	   bool guaranteed_new) {
	assert (self);
	assert (key);
	assert (value);

	// If table is more than 1/2 full, enlarge it
	if ((self->num_entries * 2) / self->table_size > 0)
		hash_enlarge (self);

	// Generate the hash of the key and hence the index into the table
	size_t index = self->hash_func (key) % self->table_size;

	// The entry is not guaranteed a new one, query first and replace
	// the existing one if it exists.
	if (!guaranteed_new) {
		hash_entry_t *rover = self->table[index].next;
		while (rover != NULL) {
			// find an entry with the same key
			if (self->equal_func (rover->key, key)) {
				print_info ("entry exists, update it.\n");
				// free key and value if needed
				if (self->value_free_func != NULL)
					self->value_free_func (&rover->value);
				if (self->key_free_func != NULL)
					self->key_free_func (&rover->key);
				// replace with new key and value
				rover->key = key;
				rover->value = value;
				return rover;
			}
			rover = rover->next;
		}
	}

	// Entry is guaranteed new or not found. Create a new entry.
	hash_entry_t *newentry = (hash_entry_t *) malloc (sizeof (hash_entry_t));
	assert (newentry);

	newentry->key = key;
	newentry->value = value;

	// Link into the list as the first item
	hash_entry_t *first_entry = self->table[index].next;
	newentry->prev = &self->table[index];
	newentry->next = first_entry;
	self->table[index].next = newentry;
	if (first_entry != NULL) // list is empty before
		first_entry->prev = newentry;

	// Adjust the number of entries
	++self->num_entries;
	return newentry;
}


void *hash_lookup (hash_t *self, const void *key) {
	assert (self);

	/* Generate the hash of the key and hence the index into the table */
	size_t index = self->hash_func (key) % self->table_size;

	// Walk the chain at this index until the corresponding entry is found
	hash_entry_t *rover = self->table[index].next;

	while (rover != NULL) {
		// Found the entry. Return the data.
		if (self->equal_func (key, rover->key))
			return rover->value;
		rover = rover->next;
	}

	// Not found
	return NULL;
}


void hash_remove (hash_t *self, void *key) {
	assert (self);

	size_t index = self->hash_func (key) % self->table_size;
	hash_entry_t *rover = self->table[index].next;
	hash_entry_t *next;

	while (rover != NULL) {
		next = rover->next;
		if (self->equal_func (key, rover->key)) {
			// Unlink from the list
			rover->prev->next = rover->next;
			if (rover->next != NULL)
				rover->next->prev = rover->prev;
			// Destroy the entry structure
			hash_free_entry (self, rover);
			// Track count of entries
			--self->num_entries;
			return;
		}
		rover = next;
	}

	// not found
	print_warning ("no entry found.\n");
}


void hash_remove_entry (hash_t *self, void *handle) {
	assert (self);
	assert (handle);

	hash_entry_t *entry = (hash_entry_t *) handle;

	// Unlink entry from list
	entry->prev->next = entry->next;
	if (entry->next != NULL)
		entry->next->prev = entry->prev;

	// Free entry
	hash_free_entry (self, entry);

	// Track count of entries
	--self->num_entries;
}


void hash_update_entry (hash_t *self, void *handle, void *key, void *value) {
	assert (self);
	assert (handle);
	assert (key);
	assert (value);

	hash_entry_t *entry = (hash_entry_t *) handle;
	assert (self->equal_func (entry->key, key));

	if (entry->key != key && self->key_free_func != NULL)
		self->key_free_func (&entry->key);

	if (entry->value != value && self->value_free_func != NULL)
		self->value_free_func (&entry->value);

	entry->key = key;
	entry->value = value;
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
	return self->cursor;
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


void *hash_entry_key (void *handle) {
	assert (handle);
	return ((hash_entry_t *)handle)->key;
}


void *hash_entry_value (void *handle) {
	assert (handle);
	return ((hash_entry_t *)handle)->value;
}


void hash_test (bool verbose) {
	print_info (" * hash: \n");
    hash_t *hash = hash_new ((hash_func_t) string_hash,
    						 (equal_func_t) string_equal,
    						 NULL,
    						 NULL);

	assert (hash);
	assert (hash_size (hash) == 0);
	assert (hash_first (hash) == NULL);

	// Insert
	void *handle;
	handle = hash_insert (hash, "PADINGTON", "goes to market", false);
	assert (handle);
	assert (streq (hash_entry_key (handle), "PADINGTON"));
	assert (streq (hash_entry_value (handle), "goes to market"));

	handle = hash_insert (hash, "MIFFY", "on a scoot", false);
	assert (handle);
	handle = hash_insert (hash, "MAISY", "goes shopping", false);
	assert (handle);
	handle = hash_insert (hash, "BUDDY", "plays a ball", false);
	assert (handle);
	assert (hash_size (hash) == 4);

	// Look for existing items
	char *item = (char *) hash_lookup (hash, "PADINGTON");
	assert (streq (item, "goes to market"));
	item = (char *) hash_lookup (hash, "MAISY");
	assert (streq (item, "goes shopping"));
	item = (char *) hash_lookup (hash, "BUDDY");
	assert (streq (item, "plays a ball"));
	item = (char *) hash_lookup (hash, "MIFFY");
	assert (streq (item, "on a scoot"));

	// Look for non-existent items
	item = (char *) hash_lookup (hash, "PIGGY");
	assert (item == NULL);

	// Try to insert duplicate items (update)
	hash_insert (hash, "MIFFY", "visit a friend", false);
	item = (char *) hash_lookup (hash, "MIFFY");
	assert (streq (item, "visit a friend"));

	// Delete a item
	hash_remove (hash, "MAISY");
	item = (char *) hash_lookup (hash, "MAISY");
	assert (item == NULL);
	assert (hash_size (hash) == 3);

	// Iteration
	handle = hash_first (hash);
	while (handle != NULL) {
		string_print (hash_entry_key (handle));
		string_print (hash_entry_value (handle));
		handle = hash_next (hash);
	}

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
			print_debug ("");
			printf ("%s\n", testset [testnbr].name);
			print_debug ("");
			handle = hash_insert (hash, testset [testnbr].name, "", false);
			assert (handle);
			testset [testnbr].exists = true;
		}
	}
	rng_free(&rng);

	// Test 10K lookups
	for (iteration = 0; iteration < 10000; iteration++)
		item = (char *) hash_lookup (hash, "DEADBEEFABADCAFE");

    hash_free (&hash);
    hash_free (&hash);
    assert (hash == NULL);
    print_info ("OK\n");
}
