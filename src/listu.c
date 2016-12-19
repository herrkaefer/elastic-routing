/*  =========================================================================
    listu - implementation

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/


#include "classes.h"


// Header defination
#define LIST4U_HEADER_SIZE   3
#define LIST4U_INDEX_ALLOCED 0
#define LIST4U_INDEX_SIZE    1
#define LIST4U_INDEX_SORTED  2
#define real_index(index)   (index+LIST4U_HEADER_SIZE)

// Sorting state defination
#define LIST4U_UNSORTED          1
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
    // one third of size as redundant
    size_t new_alloced =
        listu_alloced (self) + min_increment + listu_size (self) / 3;

    size_t *new_data = (size_t *) realloc (self->data,
            sizeof (size_t) * (LIST4U_HEADER_SIZE + new_alloced));
    assert (new_data);

    self->data = new_data;
    listu_set_alloced (self, new_alloced);
    // print_info ("new alloced size: %zu\n", listu_alloced (self));
}


// Check if item at index is properyly sorted with its previous item
static bool listu_item_is_sorted_with_prev (listu_t *self, size_t idx) {
    // first item
    if (idx == 0)
        return listu_is_sorted (self);

    if ((listu_is_sorted_ascending (self) &&
         listu_get (self, idx) >= listu_get (self, idx-1)) ||
        (listu_is_sorted_descending (self) &&
         listu_get (self, idx) <= listu_get (self, idx-1)))
        return true;
    else
        return false;
}


// Check if item at index is properyly sorted with its next item
static bool listu_item_is_sorted_with_next (listu_t *self, size_t idx) {
    size_t size = listu_size (self);

    // last item
    if (idx == size - 1)
        return listu_is_sorted (self);

    if ((listu_is_sorted_ascending (self) &&
         listu_get (self, idx) <= listu_get (self, idx + 1)) ||
        (listu_is_sorted_descending (self) &&
         listu_get (self, idx) >= listu_get (self, idx + 1)))
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
    listu_set_sorting_state (self, LIST4U_UNSORTED);

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


size_t listu_size (const listu_t *self) {
    assert (self);
    return self->data[LIST4U_INDEX_SIZE];
}


size_t listu_get (const listu_t *self, size_t index) {
    assert (self);
    assert (index < listu_size (self));
    return self->data[real_index (index)];
}


void listu_set (listu_t *self, size_t index, size_t value) {
    assert (self);
    assert (index < listu_size (self)); // index must be in range

    self->data[real_index (index)] = value;

    if (self->data[LIST4U_INDEX_SORTED] == LIST4U_UNSORTED)
        return;

    // Check the sorting state.
    // If the new item does not conform to current sorting order,
    // reset the sorting state
    if (!listu_item_is_sorted_with_prev (self, index) ||
        !listu_item_is_sorted_with_next (self, index))
        listu_set_sorting_state (self, LIST4U_UNSORTED);
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
        listu_set_sorting_state (self, LIST4U_UNSORTED);
}


size_t listu_last (const listu_t *self) {
    assert (self);
    size_t size = listu_size (self);
    return (size > 0) ? listu_get (self, size - 1) : SIZE_NONE;
}


size_t listu_pop_last (listu_t *self) {
    assert (self);
    size_t size = listu_size (self);
    size_t value = listu_get (self, size - 1);
    listu_set_size (self, size - 1);
    return value;
}


void listu_extend (listu_t *self, const listu_t *list) {
    assert (self);
    assert (list);
    size_t size = listu_size (self);
    size_t list_size = listu_size (list);
    if (list_size == 0)
        return;

    if (size + list_size > listu_alloced (self))
        listu_enlarge (self, size + list_size - listu_alloced (self));

    memcpy (self->data + real_index (size),
            list->data + real_index (0),
            sizeof (size_t) * list_size);
    listu_set_size (self, size + list_size);
    listu_set_sorting_state (self, LIST4U_UNSORTED);

    // for (size_t index = 0; index < list_size; index++)
    //     listu_append (self, listu_get (list, index));
}


void listu_extend_array (listu_t *self, const size_t *array, size_t length) {
    assert (self);
    assert (array);
    if (length == 0)
        return;

    size_t size = listu_size (self);
    if (size + length > listu_alloced (self))
        listu_enlarge (self, size + length - listu_alloced (self));

    memcpy (self->data + real_index (size), array, sizeof (size_t) * length);
    listu_set_size (self, size + length);
    listu_set_sorting_state (self, LIST4U_UNSORTED);

    // for (size_t index = 0; index < length; index++)
    //     listu_append (self, array[index]);
}


void listu_insert_at (listu_t *self, size_t index, size_t value) {
    assert (self);

    size_t size = listu_size (self);
    assert (index <= size);

    // Insert value after the last
    if (index == size)
        return listu_append (self, value);

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


void listu_swap (listu_t *self, size_t idx1, size_t idx2) {
    assert (self);
    assert (idx1 < listu_size (self));
    assert (idx2 < listu_size (self));

    if (idx1 == idx2)
        return;

    arrayu_swap (self->data + real_index (0), idx1, idx2);

    // Set unsorted anyway
    listu_set_sorting_state (self, LIST4U_UNSORTED);

    // size_t tmp = listu_get (self, idx1);
    // listu_set (self, idx1, listu_get (self, idx2));
    // listu_set (self, idx2, tmp);
}


void listu_purge (listu_t *self) {
    assert (self);
    listu_remove_slice (self, 0, listu_size (self) - 1);
}


const size_t *listu_array (const listu_t *self) {
    assert (self);
    return self->data + real_index (0);
}


bool listu_is_sorted (const listu_t *self) {
    assert (self);
    return (self->data[LIST4U_INDEX_SORTED] != LIST4U_UNSORTED);
}


bool listu_is_sorted_ascending (const listu_t *self) {
    assert (self);
    return (self->data[LIST4U_INDEX_SORTED] == LIST4U_ASCENDING_SORTED);
}


bool listu_is_sorted_descending (const listu_t *self) {
    assert (self);
    return (self->data[LIST4U_INDEX_SORTED] == LIST4U_DESCENDING_SORTED);
}


void listu_sort (listu_t *self, bool ascending) {
    assert (self);

    // The list is already sorted properly
    if ((ascending && listu_is_sorted_ascending (self)) ||
        (!ascending && listu_is_sorted_descending (self)))
        return;

    arrayu_quick_sort (self->data + real_index (0), listu_size (self), ascending);

    if (ascending)
        listu_set_sorting_state (self, LIST4U_ASCENDING_SORTED);
    else
        listu_set_sorting_state (self, LIST4U_DESCENDING_SORTED);
}


void listu_reverse (listu_t *self) {
    assert (self);
    bool is_sorted_before = listu_is_sorted (self);
    size_t ascending_before = listu_is_sorted_ascending (self);

    arrayu_reverse (self->data + real_index (0), listu_size (self));

    // Change sorting state if the list was sorted before
    if (is_sorted_before) {
        if (ascending_before)
            listu_set_sorting_state (self, LIST4U_DESCENDING_SORTED);
        else
            listu_set_sorting_state (self, LIST4U_ASCENDING_SORTED);
    }
}


void listu_reverse_slice (listu_t *self, size_t idx_begin, size_t idx_end) {
    assert (self);
    assert (idx_begin <= idx_end);
    assert (idx_end < listu_size (self));

    if (idx_begin == idx_end)
        return;

    arrayu_reverse (self->data + real_index (idx_begin),
                    idx_end - idx_begin + 1);

    // Set unsorted anyway
    listu_set_sorting_state (self, LIST4U_UNSORTED);
}


void listu_rotate (listu_t *self, int num) {
    assert (self);
    arrayu_rotate (self->data + real_index (0), listu_size (self), num);
    listu_set_sorting_state (self, LIST4U_UNSORTED);
}


void listu_shuffle (listu_t *self, rng_t *rng) {
    assert (self);
    size_t size = listu_size (self);
    if (size <= 1)
        return;

    arrayu_shuffle (self->data + real_index (0), size, rng);
    listu_set_sorting_state (self, LIST4U_UNSORTED);
}


void listu_shuffle_slice (listu_t *self,
                          size_t idx_begin,
                          size_t idx_end,
                          rng_t *rng) {
    assert (self);
    assert (idx_begin <= idx_end);
    assert (idx_end < listu_size (self));

    if (idx_begin == idx_end)
        return;

    arrayu_shuffle (self->data + real_index (idx_begin),
                    idx_end - idx_begin + 1,
                    rng);

    // Set unsorted anyway
    listu_set_sorting_state (self, LIST4U_UNSORTED);
}


void listu_swap_slices (listu_t *self, size_t i, size_t j, size_t u, size_t v) {
    assert (self);
    assert (i <= j);
    assert (j < u);
    assert (u <= v);
    assert (v < listu_size (self));

    arrayu_swap_slices (self->data + real_index (0), i, j, u, v);
    listu_set_sorting_state (self, LIST4U_UNSORTED);
}


size_t listu_find (const listu_t *self, size_t value) {
    assert (self);
    size_t size = listu_size (self);

    // For sorted list, do binary search
    if (listu_is_sorted (self))
        return arrayu_binary_search (self->data + real_index (0),
                                     size,
                                     value,
                                     listu_is_sorted_ascending (self));
    // else search from head to tail
    else
        return arrayu_find (self->data + real_index (0), size, value);
}


bool listu_includes (const listu_t *self, size_t value) {
    return listu_find (self, value) != SIZE_NONE;
}


size_t listu_count (const listu_t *self, size_t value) {
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
        for (size_t ind = index + 1; ind < size; ind++) {
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
    else
        cnt = arrayu_count (self->data + real_index (0), size, value);

    return cnt;
}


size_t *listu_dump_array (const listu_t *self) {
    assert (self);
    size_t size = listu_size (self);
    size_t *array = (size_t *) malloc (sizeof (size_t) * size);
    assert (array);
    memcpy (array, self->data + real_index (0), sizeof (size_t) * size);
    return array;
}


listu_t *listu_dup (const listu_t *self) {
    if (!self)
        return NULL;

    listu_t *copy = listu_new (self->data[LIST4U_INDEX_ALLOCED]);
    assert (copy);

    // Only size bytes are necessary to be copied
    memcpy (copy->data,
            self->data,
            sizeof (size_t) *
                (LIST4U_HEADER_SIZE + self->data[LIST4U_INDEX_SIZE]));
    return copy;
}


bool listu_equal (const listu_t *self, const listu_t *list) {
    if (self == NULL && list == NULL)
        return true;
    else if (self == NULL || list == NULL)
        return false;

    if (listu_size (self) != listu_size (list))
        return false;

    for (size_t idx = 0; idx < listu_size (self); idx++) {
        if (listu_get (self, idx) != listu_get (list, idx))
            return false;
    }
    return true;
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

    print_debug ("sort again\n");
    listu_sort (list, true);
    listu_print (list);

    print_debug ("shuffle slice\n");
    listu_shuffle_slice (list, 1, listu_size (list)-2, rng);
    listu_print (list);
    listu_shuffle_slice (list, 2, listu_size (list)-3, rng);
    listu_print (list);

    print_debug ("sort again\n");
    listu_sort (list, true);
    listu_print (list);

    assert (listu_find (list, 3) == 0);
    assert (listu_find (list, 35) == 4);
    assert (listu_find (list, 100) == SIZE_NONE);

    assert (listu_includes (list, 131));
    assert (listu_includes (list, 130) == false);

    listu_swap (list, 0, 1);
    listu_print (list);

    listu_swap (list, 0, listu_size (list) - 1);
    listu_print (list);

    listu_swap (list, 2, 2);
    listu_print (list);

    listu_rotate (list, listu_size (list) + 4);
    listu_print (list);

    listu_rotate (list, -4);
    listu_print (list);

    listu_t *copy = listu_dup (list);
    assert (copy);
    assert (listu_equal (list, copy));

    size_t size = listu_size (list);
    for (size_t cnt = 0; cnt < 1000; cnt++) {
        int num = rng_random_int (rng, -100 * (int) size, 100 * (int) size);
        listu_rotate (copy, num);
        listu_rotate (copy, -num);
        assert (listu_equal (copy, list));
    }

    listu_t *list2 = listu_new_range (10, 20, 5);
    assert (list2);
    listu_print (list2);

    listu_extend (list, list2);
    listu_print (list);

    const size_t *arr = listu_array (list2);
    listu_extend_array (list, arr, listu_size (list2));
    listu_print (list);


    rng_free (&rng);
    listu_free (&list);
    listu_free (&copy);
    listu_free (&list2);

    print_info ("OK\n");
}
