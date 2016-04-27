/*  =========================================================================
    list4u - list container of usigned integers (size_t)

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


struct _list4u_t {
    size_t *data;
};


// Get alloced size
static size_t list4u_alloced (list4u_t *self) {
    return self->data[LIST4U_INDEX_ALLOCED];
}


// Set alloced size
static void list4u_set_alloced (list4u_t *self, size_t alloced) {
    self->data[LIST4U_INDEX_ALLOCED] = alloced;
}


// Set data size
static void list4u_set_size (list4u_t *self, size_t size) {
    self->data[LIST4U_INDEX_SIZE] = size;
}


// Set sorting state
static void list4u_set_sorting_state (list4u_t *self, size_t sorted_state) {
    self->data[LIST4U_INDEX_SORTED] = sorted_state;
}


// Enlarge alloced size
static void list4u_enlarge (list4u_t *self, size_t min_increment) {
    assert (self);

    // one third of size as redundant
    size_t new_alloced =
        list4u_alloced (self) + min_increment + list4u_size (self) / 3;

    size_t *new_data = (size_t *) realloc (self->data,
            sizeof (size_t) * (LIST4U_HEADER_SIZE + new_alloced));
    assert (new_data);

    self->data = new_data;
    list4u_set_alloced (self, new_alloced);
    print_info ("new alloced size: %zu\n", list4u_alloced (self));
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
static bool list4u_item_is_sorted_with_prev (list4u_t *self, size_t index) {
    assert (self);

    // first item
    if (index == 0)
        return list4u_is_sorted (self);

    if ((list4u_is_sorted_ascending (self) &&
         list4u_get (self, index) >= list4u_get (self, index-1)) ||
        (list4u_is_sorted_descending (self) &&
         list4u_get (self, index) <= list4u_get (self, index-1)))
        return true;
    else
        return false;
}


// If item at index is properyly sorted with its next item
static bool list4u_item_is_sorted_with_next (list4u_t *self, size_t index) {
    assert (self);
    size_t size = list4u_size (self);

    // last item
    if (index == size - 1)
        return list4u_is_sorted (self);

    if ((list4u_is_sorted_ascending (self) &&
         list4u_get (self, index) <= list4u_get (self, index + 1)) ||
        (list4u_is_sorted_descending (self) &&
         list4u_get (self, index) >= list4u_get (self, index + 1)))
        return true;
    else
        return false;
}


static void list4u_print (list4u_t *self) {
    assert (self);
    size_t size = list4u_size (self);
    char sorting_state[16];
    if (list4u_is_sorted_ascending (self))
        sprintf (sorting_state, "ascending");
    else if (list4u_is_sorted_descending (self))
        sprintf (sorting_state, "descending");
    else
        sprintf (sorting_state, "no");

    printf ("\nlist4u size: %zu, alloced: %zu, sorted: %s\n",
        size, list4u_alloced (self), sorting_state);

    printf ("--------------------------------------------------\n");
    for (size_t index = 0; index < list4u_size (self); index++) {
        printf ("%zu ", list4u_get (self, index));
    }
    printf ("\n\n");
}


// ---------------------------------------------------------------------------
list4u_t *list4u_new (size_t alloc_size) {
    list4u_t *self = (list4u_t *) malloc (sizeof (list4u_t));
    assert (self);

    self->data =
        (size_t *) malloc (sizeof (size_t) * (LIST4U_HEADER_SIZE + alloc_size));
    assert (self->data);

    list4u_set_alloced (self, alloc_size);
    list4u_set_size (self, 0);
    list4u_set_sorting_state (self, LIST4U_NOT_SORTED);

    print_info ("list4u created.\n");
    return self;
}


list4u_t *list4u_new_range (size_t start, size_t stop, int step) {
    list4u_t *self = NULL;

    if (step > 0) {
        assert (start <= stop);
        size_t num = (stop - start) / step + 1;
        self = list4u_new (num);
        for (size_t value = start; value <= stop; value += step)
            list4u_append (self, value);
        list4u_set_sorting_state (self, LIST4U_ASCENDING_SORTED);
    }
    else if (step < 0) {
        assert (start >= stop);
        size_t num = (start - stop) / (-step) + 1;
        self = list4u_new (num);
        for (size_t value = start; value >= stop; value += step)
            list4u_append (self, value);
        list4u_set_sorting_state (self, LIST4U_DESCENDING_SORTED);
    }
    else {
        self = list4u_new (1);
        list4u_append (self, start);
    }

    return self;
}


void list4u_free (list4u_t **self_p) {
    assert (self_p);
    if (*self_p) {
        list4u_t *self = *self_p;
        free (self->data);
        free (self);
        *self_p = NULL;
    }
    print_info ("list4u freed.\n");
}


size_t list4u_size (list4u_t *self) {
    assert (self);
    return self->data[LIST4U_INDEX_SIZE];
}


size_t list4u_get (list4u_t *self, size_t index) {
    assert (self);
    assert (index < list4u_size (self));
    return self->data[real_index (index)];
}


void list4u_set (list4u_t *self, size_t index, size_t value) {
    assert (self);
    assert (index < list4u_size (self)); // index must be in range

    self->data[real_index (index)] = value;

    // Adjust sorting state.
    // If the new item does not conforms to current sorting order,
    // reset the sorting state
    if (!list4u_item_is_sorted_with_prev (self, index) ||
        !list4u_item_is_sorted_with_next (self, index))
        list4u_set_sorting_state (self, LIST4U_NOT_SORTED);
}


void list4u_append (list4u_t *self, size_t value) {
    assert (self);

    size_t size = list4u_size (self);
    if (size == list4u_alloced (self))
        list4u_enlarge (self, 1);

    list4u_set_size (self, size + 1);
    list4u_set (self, size, value);

    // Adjust sorting state
    if (!list4u_item_is_sorted_with_prev (self, size))
        list4u_set_sorting_state (self, LIST4U_NOT_SORTED);
}


size_t list4u_pop (list4u_t *self) {
    assert (self);
    size_t size = list4u_size (self);
    size_t value = list4u_get (self, size - 1);
    list4u_set_size (self, size - 1);
    return value;
}


void list4u_extend (list4u_t *self, list4u_t *list) {
    assert (self);
    size_t size = list4u_size (self);
    size_t list_size = list4u_size (list);

    if (size + list_size > list4u_alloced (self))
        list4u_enlarge (self, size + list_size - list4u_alloced (self));

    for (size_t index = 0; index < list_size; index++)
        list4u_append (self, list4u_get (list, index));
}


void list4u_extend_array (list4u_t *self, size_t *array, size_t length) {
    assert (self);
    size_t size = list4u_size (self);

    if (size + length > list4u_alloced (self))
        list4u_enlarge (self, size + length - list4u_alloced (self));

    for (size_t index = 0; index < length; index++)
        list4u_append (self, array[index]);
}


void list4u_insert (list4u_t *self, size_t index, size_t value) {
    assert (self);

    size_t size = list4u_size (self);
    assert (index < size);

    if (size == list4u_alloced (self))
        list4u_enlarge (self, 1);

    list4u_set_size (self, size + 1);

    // for (size_t ind = size; ind > index; ind--)
    //     list4u_set (self, ind, list4u_get (self, ind - 1));

    // move valus: [index, ..., size-1] -> [index+1, ..., size]
    memmove (self->data + real_index (index + 1),
             self->data + real_index (index),
             sizeof (size_t) * (size - index));

    list4u_set (self, index, value);
}


void list4u_remove_slice (list4u_t *self, size_t index_begin, size_t index_end) {
    assert (self);

    size_t size = list4u_size (self);
    assert (index_begin <= index_end);
    assert (index_end < size);

    memmove (self->data + real_index (index_begin),
             self->data + real_index (index_end + 1),
             sizeof (size_t) * (size - 1 - index_end));

    list4u_set_size (self, size - (index_end - index_begin + 1));
}


void list4u_remove (list4u_t *self, size_t index) {
    assert (self);
    list4u_remove_slice (self, index, index);
}


size_t list4u_insert_sorted (list4u_t *self, size_t value) {
    assert (self);

    size_t size = list4u_size (self);
    if (list4u_size (self) == list4u_alloced (self))
        list4u_enlarge (self, 1);

    // If list is not sorted, sort it in ascending order
    if (!list4u_is_sorted (self))
        list4u_sort (self, true);

    // If list is sorted, binary search the position to insert
    bool ascending = list4u_is_sorted_ascending (self);
    size_t head = 0, tail = size, mid;

    if (ascending) {
        while (head < tail) {
            mid = (head + tail) / 2;
            if (list4u_get (self, mid) == value)
                break;
            else if (list4u_get (self, mid) > value)
                tail = mid;
            else
                head = mid + 1;
        }
    }
    else {
        while (head < tail) {
            mid = (head + tail) / 2;
            if (list4u_get (self, mid) == value)
                break;
            else if (list4u_get (self, mid) < value)
                tail = mid;
            else
                head = mid + 1;
        }
    }

    // position to insert
    size_t index = (head < tail) ? mid : head;

    // insert item
    list4u_insert (self, index, value);

    return index;
}


int list4u_remove_value (list4u_t *self, size_t value) {
    assert (self);

    int result = -1;
    size_t size = list4u_size (self);

    // If list is sorted, index the value and slide to head and tail
    // respectively to get the correct range
    if (list4u_is_sorted (self)) {
        size_t index = list4u_find (self, value);

        // not found
        if (index == SIZE_NONE)
            return -1;

        size_t index_begin = 0, index_end = size - 1;
        // to tail
        for (size_t ind = index + 1; ind < size; ind++) {
            if (list4u_get (self, ind) != value) {
                index_end = ind - 1;
                break;
            }
        }
        // to head
        for (size_t ind = index; ind-- > 0;) {
            if (list4u_get (self, ind) != value) {
                index_begin = ind + 1;
                break;
            }
        }

        list4u_remove_slice (self, index_begin, index_end);
        result = 0;
    }
    // If list is not sorted, search and remove one by one
    else {
        size_t ind = 0;
        while (ind < list4u_size (self)) {
            if (list4u_get (self, ind) == value) {
                list4u_remove (self, ind);
                result = 0;
            }
            else
                ind++;
        }
    }

    return result;
}


void list4u_purge (list4u_t *self) {
    assert (self);
    list4u_remove_slice (self, 0, list4u_size (self) - 1);
}


bool list4u_is_sorted (list4u_t *self) {
    return (self->data[LIST4U_INDEX_SORTED] != LIST4U_NOT_SORTED);
}


bool list4u_is_sorted_ascending (list4u_t *self) {
    return (self->data[LIST4U_INDEX_SORTED] == LIST4U_ASCENDING_SORTED);
}


bool list4u_is_sorted_descending (list4u_t *self) {
    return (self->data[LIST4U_INDEX_SORTED] == LIST4U_DESCENDING_SORTED);
}


void list4u_sort (list4u_t *self, bool ascending) {
    assert (self);

    // The list is already sorted properly
    if ((ascending && list4u_is_sorted_ascending (self)) ||
        (!ascending && list4u_is_sorted_descending (self)))
        return;

    s_quick_sort (self->data + real_index (0), list4u_size (self), ascending);

    if (ascending)
        list4u_set_sorting_state (self, LIST4U_ASCENDING_SORTED);
    else
        list4u_set_sorting_state (self, LIST4U_DESCENDING_SORTED);
}


void list4u_reverse (list4u_t *self) {
    assert (self);
    size_t size = list4u_size (self);
    bool is_sorted_before = list4u_is_sorted (self);
    size_t ascending_before = list4u_is_sorted_ascending (self);
    size_t tmp;

    for (size_t index = 0; index < size / 2; index++) {
        tmp = list4u_get (self, index);
        list4u_set (self, index, list4u_get (self, size - index - 1));
        list4u_set (self, size - index - 1, tmp);
    }

    // Reverse sorting state if list is sorted before
    if (is_sorted_before) {
        if (ascending_before)
            list4u_set_sorting_state (self, LIST4U_DESCENDING_SORTED);
        else
            list4u_set_sorting_state (self, LIST4U_ASCENDING_SORTED);
    }
}


void list4u_shuffle (list4u_t *self, rng_t *rng) {
    assert (self);

    // set not sorted anyway
    list4u_set_sorting_state (self, LIST4U_NOT_SORTED);

    size_t size = list4u_size (self);
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
        j = (size_t) rng_random_int (rng, i, size); // [i, size-1]
        tmp = list4u_get (self, j);
        list4u_set (self, j, list4u_get (self, i));
        list4u_set (self, i, tmp);
    }

    if (own_rng)
        rng_free (&rng);
}


size_t list4u_find (list4u_t *self, size_t value) {
    assert (self);

    size_t result = SIZE_NONE;
    size_t size = list4u_size (self);

    // For sorted list, do binary search
    if (list4u_is_sorted (self)) {
        bool ascending = list4u_is_sorted_ascending (self);
        result = s_binary_search (self->data + real_index (0),
                                size, value, ascending);
    }
    // For not-sorted list, search from head to tail
    else {
        for (size_t index = 0; index < size; index++) {
            if (list4u_get (self, index) == value) {
                result = index;
                break;
            }
        }
    }

    return result;
}


bool list4u_has_value (list4u_t *self, size_t value) {
    return list4u_find (self, value) != SIZE_NONE;
}


size_t list4u_count (list4u_t *self, size_t value) {
    assert (self);

    size_t size = list4u_size (self);
    size_t cnt;

    // list is sorted, index value and count to head and tail
    if (list4u_is_sorted (self)) {
        size_t index = list4u_find (self, value);
        if (index == SIZE_NONE)
            return 0;
        cnt = 1;
        // count to tail
        for (size_t ind = index+1; ind < size; ind++) {
            if (list4u_get (self, ind) == value)
                cnt++;
            else
                break;
        }
        // count to head
        for (size_t ind = index; ind-- > 0;) {
            if (list4u_get (self, ind) == value)
                cnt++;
            else
                break;
        }
    }
    // list is not sorted, count from head to tail
    else {
        cnt = 0;
        for (size_t ind = 0; ind < size; ind++) {
            if (list4u_get (self, ind) == value)
                cnt++;
        }
    }

    return cnt;
}


size_t *list4u_dup_array (list4u_t *self) {
    assert (self);
    size_t size = list4u_size (self);
    size_t *array = (size_t *) malloc (sizeof (size_t) * size);
    assert (array);
    memcpy (array, self->data + real_index (0), sizeof (size_t) * size);
    return array;
}


void list4u_test (bool verbose) {
    print_info (" * list4u: \n");
    list4u_t *list = list4u_new (0);

    assert (list4u_size (list) == 0);
    assert (list4u_is_sorted (list) == false);
    assert (list4u_is_sorted_ascending (list) == false);
    assert (list4u_is_sorted_descending (list) == false);

    for (size_t n = 1; n < 11; n++) {
        print_info ("append: %zu\n", n);
        list4u_append (list, n);
        // print_info ("get: %zu\n", list4u_get (list, list4u_size (list) - 1));
        assert (list4u_get (list, list4u_size (list) - 1) == n);
        assert (list4u_size (list) == n);
        assert (list4u_is_sorted (list) == false);
    }
    list4u_print (list);

    list4u_reverse (list);
    list4u_print (list);

    list4u_set (list, 2, 99);
    list4u_print (list);

    list4u_insert (list, 3, 77);
    list4u_insert (list, 1, 55);
    list4u_set (list, 2, 33);
    list4u_print (list);

    list4u_sort (list, true);
    list4u_print (list);

    list4u_insert_sorted (list, 8);
    list4u_insert_sorted (list, 9);
    list4u_insert_sorted (list, 11);
    list4u_print (list);

    list4u_remove_slice (list, 2, 5);
    list4u_print (list);

    list4u_remove (list, 0);
    list4u_print (list);

    list4u_remove (list, list4u_size (list)-1);
    list4u_print (list);

    list4u_reverse (list);
    list4u_print (list);

    list4u_set (list, 6, 9);
    list4u_print (list);

    assert (list4u_count (list, 9) == 2);
    assert (list4u_has_value (list, 9));
    assert (list4u_count (list, 0) == 0);
    assert (list4u_has_value (list, 0) == false);

    list4u_remove_value (list, 9);
    list4u_print (list);
    assert (list4u_has_value (list, 9) == false);

    assert (list4u_is_sorted (list));
    assert (list4u_is_sorted_ascending (list) == false);
    assert (list4u_is_sorted_descending (list));

    assert (list4u_pop (list) == 2);
    assert (list4u_pop (list) == 7);
    list4u_print (list);

    list4u_free (&list);


    list = list4u_new_range (3, 180, 8);
    list4u_print (list);

    rng_t *rng = rng_new ();
    list4u_shuffle (list, rng);
    list4u_print (list);
    list4u_shuffle (list, rng);
    list4u_print (list);
    list4u_shuffle (list, rng);
    list4u_print (list);


    rng_free (&rng);
    list4u_free (&list);


    print_info ("OK\n");
}
