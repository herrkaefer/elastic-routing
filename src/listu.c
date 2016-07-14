/*  =========================================================================
    listu - list container of usigned integers

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/


#include "classes.h"


// header defination
#define LIST4U_HEADER_SIZE   3
#define LIST4U_INDEX_ALLOCED 0
#define LIST4U_INDEX_SIZE    1
#define LIST4U_INDEX_SORTED  2
#define real_index(index)   (index+LIST4U_HEADER_SIZE)

// sorting state
#define LIST4U_NOT_SORTED        1
#define LIST4U_ASCENDING_SORTED  2
#define LIST4U_DESCENDING_SORTED 4


struct _listu_t {
    size_t *data;
};


// Get alloced size
static size_t listu_alloced (listu_t *self) {
    return self->data[LIST4U_INDEX_ALLOCED];
}


// Set alloced size
static void listu_set_alloced (listu_t *self, size_t alloced) {
    self->data[LIST4U_INDEX_ALLOCED] = alloced;
}


// Set data size
static void listu_set_size (listu_t *self, size_t size) {
    self->data[LIST4U_INDEX_SIZE] = size;
}


// Set sorting state
static void listu_set_sorting_state (listu_t *self, size_t sorted_state) {
    self->data[LIST4U_INDEX_SORTED] = sorted_state;
}


// Enlarge alloced size
static void listu_enlarge (listu_t *self, size_t min_increment) {
    assert (self);

    // one third of size as redundant
    size_t new_alloced =
        listu_alloced (self) + min_increment + listu_size (self) / 3;

    size_t *new_data = (size_t *) realloc (self->data,
            sizeof (size_t) * (LIST4U_HEADER_SIZE + new_alloced));
    assert (new_data);

    self->data = new_data;
    listu_set_alloced (self, new_alloced);
    print_info ("new alloced size: %zu\n", listu_alloced (self));
}


// Quick sort of array
static void s_quick_sort (size_t *array, size_t length, bool ascending) {
    if (length <= 1)
        return;

    size_t t = array[0];
    size_t i = 0, j = length - 1;

    while (1) {
        if (ascending)
            while ((j > i) && (array[j] >= t)) j--;
        else
            while ((j > i) && (array[j] <= t)) j--;

        if (j == i) break;

        array[i] = array[j];
        i++;

        if (ascending)
            while ((i < j) && (array[i] <= t)) i++;
        else
            while ((i < j) && (array[i] >= t)) i++;

        if (i == j) break;

        array[j] = array[i];
        j--;
    }

    array[i] = t;

    s_quick_sort (array, i, ascending);
    s_quick_sort (&array[i+1], length-i-1, ascending);
}


// Binary search of value in sorted array.
// Return SIZE_NONE if not found.
static size_t s_binary_search (const size_t *array,
                               size_t length,
                               size_t value,
                               bool ascending) {
    assert (array);

    size_t head = 0, tail = length, mid;

    if (ascending) {
        while (head < tail) {
            mid = (head + tail) / 2;
            if (array[mid] == value)
                return mid;
            else if (array[mid] > value)
                tail = mid;
            else
                head = mid + 1;
        }
    }
    else {
        while (head < tail) {
            mid = (head + tail) / 2;
            if (array[mid] == value)
                return mid;
            else if (array[mid] < value)
                tail = mid;
            else
                head = mid + 1;
        }
    }

    return SIZE_NONE;
}


// If item at index is properyly sorted with its previous item
static bool listu_item_is_sorted_with_prev (listu_t *self, size_t index) {
    assert (self);

    // first item
    if (index == 0)
        return listu_is_sorted (self);

    if ((listu_is_sorted_ascending (self) &&
         listu_get (self, index) >= listu_get (self, index-1)) ||
        (listu_is_sorted_descending (self) &&
         listu_get (self, index) <= listu_get (self, index-1)))
        return true;
    else
        return false;
}


// If item at index is properyly sorted with its next item
static bool listu_item_is_sorted_with_next (listu_t *self, size_t index) {
    assert (self);
    size_t size = listu_size (self);

    // last item
    if (index == size - 1)
        return listu_is_sorted (self);

    if ((listu_is_sorted_ascending (self) &&
         listu_get (self, index) <= listu_get (self, index + 1)) ||
        (listu_is_sorted_descending (self) &&
         listu_get (self, index) >= listu_get (self, index + 1)))
        return true;
    else
        return false;
}


// ---------------------------------------------------------------------------
listu_t *listu_new (size_t alloc_size) {
    listu_t *self = (listu_t *) malloc (sizeof (listu_t));
    assert (self);

    self->data =
        (size_t *) malloc (sizeof (size_t) * (LIST4U_HEADER_SIZE + alloc_size));
    assert (self->data);

    listu_set_alloced (self, alloc_size);
    listu_set_size (self, 0);
    listu_set_sorting_state (self, LIST4U_NOT_SORTED);

    // print_info ("listu created.\n");
    return self;
}


listu_t *listu_new_range (size_t start, size_t stop, int step) {
    listu_t *self = NULL;

    if (step > 0) {
        assert (start <= stop);
        size_t num = (stop - start) / step + 1;
        self = listu_new (num);
        for (size_t value = start; value <= stop; value += step)
            listu_append (self, value);
        listu_set_sorting_state (self, LIST4U_ASCENDING_SORTED);
    }
    else if (step < 0) {
        assert (start >= stop);
        size_t num = (start - stop) / (-step) + 1;
        self = listu_new (num);
        for (size_t value = start; value >= stop; value += step)
            listu_append (self, value);
        listu_set_sorting_state (self, LIST4U_DESCENDING_SORTED);
    }
    else {
        self = listu_new (1);
        listu_append (self, start);
    }

    return self;
}


listu_t *listu_new_from_array (const size_t *array, size_t len) {
    assert (array);
    listu_t *self = listu_new (len);
    assert (self);
    memcpy (self->data + real_index (0), array, sizeof (size_t) * len);
    listu_set_size (self, len);
    return self;
}


void listu_free (listu_t **self_p) {
    assert (self_p);
    if (*self_p) {
        listu_t *self = *self_p;
        free (self->data);
        free (self);
        *self_p = NULL;
    }
    // print_info ("listu freed.\n");
}


size_t listu_size (listu_t *self) {
    assert (self);
    return self->data[LIST4U_INDEX_SIZE];
}


size_t listu_get (listu_t *self, size_t index) {
    assert (self);
    assert (index < listu_size (self));
    return self->data[real_index (index)];
}


void listu_set (listu_t *self, size_t index, size_t value) {
    assert (self);
    assert (index < listu_size (self)); // index must be in range

    self->data[real_index (index)] = value;

    // Adjust sorting state.
    // If the new item does not conforms to current sorting order,
    // reset the sorting state
    if (!listu_item_is_sorted_with_prev (self, index) ||
        !listu_item_is_sorted_with_next (self, index))
        listu_set_sorting_state (self, LIST4U_NOT_SORTED);
}


void listu_prepend (listu_t *self, size_t value) {
    assert (self);
    listu_insert_at (self, 0, value);
}


void listu_append (listu_t *self, size_t value) {
    assert (self);

    size_t size = listu_size (self);
    if (size == listu_alloced (self))
        listu_enlarge (self, 1);

    listu_set_size (self, size + 1);
    listu_set (self, size, value);

    // Adjust sorting state
    if (!listu_item_is_sorted_with_prev (self, size))
        listu_set_sorting_state (self, LIST4U_NOT_SORTED);
}


size_t listu_pop_last (listu_t *self) {
    assert (self);
    size_t size = listu_size (self);
    size_t value = listu_get (self, size - 1);
    listu_set_size (self, size - 1);
    return value;
}


void listu_extend (listu_t *self, listu_t *list) {
    assert (self);
    size_t size = listu_size (self);
    size_t list_size = listu_size (list);

    if (size + list_size > listu_alloced (self))
        listu_enlarge (self, size + list_size - listu_alloced (self));

    for (size_t index = 0; index < list_size; index++)
        listu_append (self, listu_get (list, index));
}


void listu_extend_array (listu_t *self, size_t *array, size_t length) {
    assert (self);
    size_t size = listu_size (self);

    if (size + length > listu_alloced (self))
        listu_enlarge (self, size + length - listu_alloced (self));

    for (size_t index = 0; index < length; index++)
        listu_append (self, array[index]);
}


void listu_insert_at (listu_t *self, size_t index, size_t value) {
    assert (self);

    size_t size = listu_size (self);
    assert (index < size);

    if (size == listu_alloced (self))
        listu_enlarge (self, 1);

    listu_set_size (self, size + 1);

    // Move values at [index, ..., size-1] to [index+1, ..., size]
    memmove (self->data + real_index (index + 1),
             self->data + real_index (index),
             sizeof (size_t) * (size - index));

    listu_set (self, index, value);
}


void listu_remove_slice (listu_t *self, size_t index_begin, size_t index_end) {
    assert (self);

    size_t size = listu_size (self);
    assert (index_begin <= index_end);
    assert (index_end < size);

    memmove (self->data + real_index (index_begin),
             self->data + real_index (index_end + 1),
             sizeof (size_t) * (size - 1 - index_end));

    listu_set_size (self, size - (index_end - index_begin + 1));
}


void listu_remove_at (listu_t *self, size_t index) {
    assert (self);
    listu_remove_slice (self, index, index);
}


void listu_remove_first (listu_t *self) {
    assert (self);
    listu_remove_slice (self, 0, 0);
}


void listu_remove_last (listu_t *self) {
    assert (self);
    size_t size = listu_size (self);
    listu_remove_slice (self, size-1, size-1);
}


size_t listu_insert_sorted (listu_t *self, size_t value) {
    assert (self);

    size_t size = listu_size (self);
    if (listu_size (self) == listu_alloced (self))
        listu_enlarge (self, 1);

    // If list is not sorted, sort it in ascending order
    if (!listu_is_sorted (self))
        listu_sort (self, true);

    // If list is sorted, binary search the position to insert
    bool ascending = listu_is_sorted_ascending (self);
    size_t head = 0, tail = size, mid;

    if (ascending) {
        while (head < tail) {
            mid = (head + tail) / 2;
            if (listu_get (self, mid) == value)
                break;
            else if (listu_get (self, mid) > value)
                tail = mid;
            else
                head = mid + 1;
        }
    }
    else {
        while (head < tail) {
            mid = (head + tail) / 2;
            if (listu_get (self, mid) == value)
                break;
            else if (listu_get (self, mid) < value)
                tail = mid;
            else
                head = mid + 1;
        }
    }

    // position to insert
    size_t index = (head < tail) ? mid : head;

    // insert item
    listu_insert_at (self, index, value);

    return index;
}


int listu_remove (listu_t *self, size_t value) {
    assert (self);

    int result = -1;
    size_t size = listu_size (self);

    // If list is sorted, index the value and slide to head and tail
    // respectively to get the correct range
    if (listu_is_sorted (self)) {
        size_t index = listu_find (self, value);

        // not found
        if (index == SIZE_NONE)
            return -1;

        size_t index_begin = 0, index_end = size - 1;
        // to tail
        for (size_t ind = index + 1; ind < size; ind++) {
            if (listu_get (self, ind) != value) {
                index_end = ind - 1;
                break;
            }
        }
        // to head
        for (size_t ind = index; ind-- > 0;) {
            if (listu_get (self, ind) != value) {
                index_begin = ind + 1;
                break;
            }
        }

        listu_remove_slice (self, index_begin, index_end);
        result = 0;
    }
    // If list is not sorted, search and remove one by one
    else {
        size_t ind = 0;
        while (ind < listu_size (self)) {
            if (listu_get (self, ind) == value) {
                listu_remove_at (self, ind);
                result = 0;
            }
            else
                ind++;
        }
    }

    return result;
}


void listu_swap (listu_t *self, size_t index1, size_t index2) {
    assert (self);
    size_t size = listu_size (self);
    assert (index1 < size);
    assert (index2 < size);

    if (index1 == index2)
        return;

    size_t tmp = listu_get (self, index1);
    listu_set (self, index1, listu_get (self, index2));
    listu_set (self, index2, tmp);
}


void listu_purge (listu_t *self) {
    assert (self);
    listu_remove_slice (self, 0, listu_size (self) - 1);
}


const size_t *listu_array (listu_t *self) {
    assert (self);
    return self->data + real_index (0);
}


bool listu_is_sorted (listu_t *self) {
    return (self->data[LIST4U_INDEX_SORTED] != LIST4U_NOT_SORTED);
}


bool listu_is_sorted_ascending (listu_t *self) {
    return (self->data[LIST4U_INDEX_SORTED] == LIST4U_ASCENDING_SORTED);
}


bool listu_is_sorted_descending (listu_t *self) {
    return (self->data[LIST4U_INDEX_SORTED] == LIST4U_DESCENDING_SORTED);
}


void listu_sort (listu_t *self, bool ascending) {
    assert (self);

    // The list is already sorted properly
    if ((ascending && listu_is_sorted_ascending (self)) ||
        (!ascending && listu_is_sorted_descending (self)))
        return;

    s_quick_sort (self->data + real_index (0), listu_size (self), ascending);

    if (ascending)
        listu_set_sorting_state (self, LIST4U_ASCENDING_SORTED);
    else
        listu_set_sorting_state (self, LIST4U_DESCENDING_SORTED);
}


void listu_reverse (listu_t *self) {
    assert (self);
    size_t size = listu_size (self);
    bool is_sorted_before = listu_is_sorted (self);
    size_t ascending_before = listu_is_sorted_ascending (self);
    size_t tmp;

    for (size_t index = 0; index < size / 2; index++) {
        tmp = listu_get (self, index);
        listu_set (self, index, listu_get (self, size - index - 1));
        listu_set (self, size - index - 1, tmp);
    }

    // Reverse sorting state if list is sorted before
    if (is_sorted_before) {
        if (ascending_before)
            listu_set_sorting_state (self, LIST4U_DESCENDING_SORTED);
        else
            listu_set_sorting_state (self, LIST4U_ASCENDING_SORTED);
    }
}


void listu_shuffle (listu_t *self, rng_t *rng) {
    assert (self);

    size_t size = listu_size (self);
    if (size <= 1)
        return;

    bool own_rng = false;
    if (rng == NULL) {
        rng = rng_new ();
        own_rng = true;
    }

    size_t i, j;
    size_t tmp;
    for (i = 0; i < size - 1; i++) {
        j = (size_t) rng_random_int (rng, i, size); // j in [i, size-1]
        // Swap values at i and j
        tmp = listu_get (self, j);
        listu_set (self, j, listu_get (self, i));
        listu_set (self, i, tmp);
    }

    if (own_rng)
        rng_free (&rng);

    // set not sorted anyway
    listu_set_sorting_state (self, LIST4U_NOT_SORTED);
}


void listu_shuffle_slice (listu_t *self,
                           size_t index_begin,
                           size_t index_end,
                           rng_t *rng) {
    assert (self);
    assert (index_begin <= index_end);
    assert (index_end < listu_size (self));

    if (index_begin == index_end)
        return;

    bool own_rng = false;
    if (rng == NULL) {
        rng = rng_new ();
        own_rng = true;
    }

    size_t i, j;
    size_t tmp;
    for (i = index_begin; i < index_end; i++) {
        j = (size_t) rng_random_int (rng, i, index_end+1); // j in [i, index_end]
        // Swap values at i and j
        tmp = listu_get (self, j);
        listu_set (self, j, listu_get (self, i));
        listu_set (self, i, tmp);
    }

    if (own_rng)
        rng_free (&rng);

    // set not sorted anyway
    listu_set_sorting_state (self, LIST4U_NOT_SORTED);
}


size_t listu_find (listu_t *self, size_t value) {
    assert (self);

    size_t result = SIZE_NONE;
    size_t size = listu_size (self);

    // For sorted list, do binary search
    if (listu_is_sorted (self)) {
        bool ascending = listu_is_sorted_ascending (self);
        result = s_binary_search (self->data + real_index (0),
                                size, value, ascending);
    }
    // For not-sorted list, search from head to tail
    else {
        for (size_t index = 0; index < size; index++) {
            if (listu_get (self, index) == value) {
                result = index;
                break;
            }
        }
    }

    return result;
}


bool listu_includes (listu_t *self, size_t value) {
    return listu_find (self, value) != SIZE_NONE;
}


size_t listu_count (listu_t *self, size_t value) {
    assert (self);

    size_t size = listu_size (self);
    size_t cnt;

    // list is sorted, index value and count to head and tail
    if (listu_is_sorted (self)) {
        size_t index = listu_find (self, value);
        if (index == SIZE_NONE)
            return 0;
        cnt = 1;
        // count to tail
        for (size_t ind = index+1; ind < size; ind++) {
            if (listu_get (self, ind) == value)
                cnt++;
            else
                break;
        }
        // count to head
        for (size_t ind = index; ind-- > 0;) {
            if (listu_get (self, ind) == value)
                cnt++;
            else
                break;
        }
    }
    // list is not sorted, count from head to tail
    else {
        cnt = 0;
        for (size_t ind = 0; ind < size; ind++) {
            if (listu_get (self, ind) == value)
                cnt++;
        }
    }

    return cnt;
}


size_t *listu_dump_array (listu_t *self) {
    assert (self);
    size_t size = listu_size (self);
    size_t *array = (size_t *) malloc (sizeof (size_t) * size);
    assert (array);
    memcpy (array, self->data + real_index (0), sizeof (size_t) * size);
    return array;
}


listu_t *listu_dup (listu_t *self) {
    if (!self)
        return NULL;

    listu_t *copy = listu_new (listu_alloced (self));
    assert (copy);

    // Only size bytes are necessary to be copied
    memcpy (copy->data,
            self->data,
            sizeof (size_t) * (LIST4U_HEADER_SIZE + listu_size (self)));
    return copy;
}


void listu_print (const listu_t *self) {
    assert (self);
    size_t size = self->data[LIST4U_INDEX_SIZE];
    char sorting_state[16];
    if (self->data[LIST4U_INDEX_SORTED] == LIST4U_ASCENDING_SORTED)
        sprintf (sorting_state, "ascending");
    else if (self->data[LIST4U_INDEX_SORTED] == LIST4U_DESCENDING_SORTED)
        sprintf (sorting_state, "descending");
    else
        sprintf (sorting_state, "no");

    printf ("\nlistu size: %zu, sorted: %s\n", size, sorting_state);

    printf ("--------------------------------------------------\n");
    for (size_t index = 0; index < size; index++) {
        printf ("%zu ", self->data[real_index (index)]);
    }
    printf ("\n\n");
}


void listu_test (bool verbose) {
    print_info (" * listu: \n");
    listu_t *list = listu_new (0);

    assert (listu_size (list) == 0);
    assert (listu_is_sorted (list) == false);
    assert (listu_is_sorted_ascending (list) == false);
    assert (listu_is_sorted_descending (list) == false);

    for (size_t n = 1; n < 11; n++) {
        print_info ("append: %zu\n", n);
        listu_append (list, n);
        // print_info ("get: %zu\n", listu_get (list, listu_size (list) - 1));
        assert (listu_get (list, listu_size (list) - 1) == n);
        assert (listu_size (list) == n);
        assert (listu_is_sorted (list) == false);
    }
    listu_print (list);

    listu_reverse (list);
    listu_print (list);

    listu_set (list, 2, 99);
    listu_print (list);

    listu_insert_at (list, 3, 77);
    listu_insert_at (list, 1, 55);
    listu_set (list, 2, 33);
    listu_print (list);

    listu_sort (list, true);
    listu_print (list);

    listu_insert_sorted (list, 8);
    listu_insert_sorted (list, 9);
    listu_insert_sorted (list, 11);
    listu_print (list);

    listu_remove_slice (list, 2, 5);
    listu_print (list);

    listu_remove_at (list, 0);
    listu_print (list);

    listu_remove_at (list, listu_size (list)-1);
    listu_print (list);

    listu_reverse (list);
    listu_print (list);

    listu_set (list, 6, 9);
    listu_print (list);

    assert (listu_count (list, 9) == 2);
    assert (listu_includes (list, 9));
    assert (listu_count (list, 0) == 0);
    assert (listu_includes (list, 0) == false);

    listu_remove (list, 9);
    listu_print (list);
    assert (listu_includes (list, 9) == false);

    assert (listu_is_sorted (list));
    assert (listu_is_sorted_ascending (list) == false);
    assert (listu_is_sorted_descending (list));

    assert (listu_pop_last (list) == 2);
    assert (listu_pop_last (list) == 7);
    listu_print (list);

    listu_free (&list);


    list = listu_new_range (3, 180, 8);
    listu_print (list);

    print_debug ("shuffle\n");
    rng_t *rng = rng_new ();
    listu_shuffle (list, rng);
    listu_print (list);
    listu_shuffle (list, rng);
    listu_print (list);
    listu_shuffle (list, rng);
    listu_print (list);
    print_debug ("shuffle slice\n");
    listu_shuffle_slice (list, 1, listu_size (list)-2, rng);
    listu_print (list);
    listu_shuffle_slice (list, 2, listu_size (list)-3, rng);
    listu_print (list);


    rng_free (&rng);
    listu_free (&list);


    print_info ("OK\n");
}
