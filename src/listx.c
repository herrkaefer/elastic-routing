/*  =========================================================================
    listx - implementation

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#include "classes.h"


// sorting state
#define LIST4X_NOT_SORTED        1
#define LIST4X_ASCENDING_SORTED  2
#define LIST4X_DESCENDING_SORTED 4


typedef struct _s_node_t {
    struct _s_node_t *prev;
    struct _s_node_t *next;
    void *item;
    bool attached;
} s_node_t;


struct _listx_t {
    s_node_t *head;
    size_t size;
    int sorting_state;
    destructor_t destructor;
    duplicator_t duplicator;
    comparator_t comparator;
    printer_t printer;
};


// A simple wrapper of node index and handle
typedef struct {
    size_t index;
    s_node_t *node;
} s_indicator_t;


// Create a new node
static s_node_t *s_node_new (void *item) {
    s_node_t *self = (s_node_t *) malloc (sizeof (s_node_t));
    assert (self);
    self->prev = self;
    self->next = self; // linking with itself
    self->item = item;
    self->attached = false; // not attached to a list after creation
    return self;
}


// Link or unlink node between prev node and next node
static void s_node_relink (s_node_t *node, s_node_t *prev, s_node_t *next) {
    s_node_t *temp = node->next;
    node->next = prev->next;
    prev->next = temp;
    temp = node->prev;
    node->prev = next->prev;
    next->prev = temp;
}


// Get the node num_steps steps away from node in forward or backward direction
static s_node_t *s_node_walk (s_node_t *node, size_t num_steps, bool forward) {
    if (forward) {
        for (size_t cnt = 0; cnt < num_steps; cnt++)
            node = node->next;
    }
    else {
        for (size_t cnt = 0; cnt < num_steps; cnt++)
            node = node->prev;
    }
    return node;
}


// Create a new node from item.
// Duplicate the item if duplicator is set.
static s_node_t *listx_new_node (const listx_t *self, void *item) {
    if (self->duplicator) {
        item = self->duplicator (item);
        assert (item);
    }
    s_node_t *node = s_node_new (item);
    return node;
}


// Get node by index
static s_node_t *listx_node (const listx_t *self, size_t index) {
    return (index < self->size / 2) ?
           s_node_walk (self->head->next, index, true) :
           s_node_walk (self->head->prev, self->size-index-1, false);
}


static void listx_set_sorted (listx_t *self, bool ascending) {
    self->sorting_state = ascending ?
                          LIST4X_ASCENDING_SORTED :
                          LIST4X_DESCENDING_SORTED;
}


static void listx_set_not_sorted (listx_t *self) {
    self->sorting_state = LIST4X_NOT_SORTED;
}


// Check if node is sorted properly with its previous node
static bool listx_node_is_sorted_with_prev (const listx_t *self,
                                            const s_node_t *node) {
    if (node->prev == self->head) // first node
        return listx_is_sorted (self);
    else
        return (listx_is_sorted_ascending (self) &&
                self->comparator (node->item, node->prev->item) >= 0) ||
               (listx_is_sorted_descending (self) &&
                self->comparator (node->item, node->prev->item) <= 0);
}


// Check if node is sorted properly with its next node
static bool listx_node_is_sorted_with_next (const listx_t *self,
                                            const s_node_t *node) {
    if (node->next == self->head) // last node
        return listx_is_sorted (self);
    else
        return (listx_is_sorted_ascending (self) &&
                self->comparator (node->item, node->next->item) <= 0) ||
               (listx_is_sorted_descending (self) &&
                self->comparator (node->item, node->next->item) >= 0);
}


// Quick sort of list
// Note that this implementation will detach items from their node handles
// static void listx_quick_sort1 (listx_t *self,
//                                s_node_t *first_node,
//                                s_node_t *last_node,
//                                size_t length,
//                                bool ascending) {
//     if (length <= 1)
//         return;

//     void *test = first_node->item;
//     size_t i = 0, j = length - 1;
//     s_node_t *node_i = first_node;
//     s_node_t *node_j = last_node;

//     while (1) {
//         if (ascending)
//             while ((j > i) &&
//                    (self->comparator (node_j->item, test) >= 0)) {
//                 j--; node_j = node_j->prev;
//             }
//         else
//             while ((j > i) &&
//                    (self->comparator (node_j->item, test) <= 0)) {
//                 j--; node_j = node_j->prev;
//             }

//         if (j == i) break;

//         node_i->item = node_j->item;
//         i++; node_i = node_i->next;

//         if (ascending)
//             while ((i < j) &&
//                    (self->comparator (node_i->item, test) <= 0)) {
//                 i++; node_i = node_i->next;
//             }
//         else
//             while ((i < j) &&
//                    (self->comparator (node_i->item, test) >= 0)) {
//                 i++; node_i = node_i->next;
//             }

//         if (i == j) break;

//         node_j->item = node_i->item;
//         j--; node_j = node_j->prev;
//     }

//     node_i->item = test;

//     listx_quick_sort1 (self, first_node, node_i->prev, i, ascending);
//     listx_quick_sort1 (self, node_i->next, last_node, length-i-1, ascending);
// }


// Quick sort of list
// This implementation will preserve bindings between items and their handles.
static void listx_quick_sort (listx_t *self,
                              s_node_t *first_node,
                              s_node_t *last_node,
                              bool ascending) {

    if (first_node == last_node || first_node == last_node->next)
        return;

    s_node_t *test = first_node;
    s_node_t *node_h = first_node->prev;
    s_node_t *node_t = last_node;
    s_node_t *tmp;

    first_node = first_node->prev;
    last_node = last_node->next;

    // Detach test node
    s_node_relink (test, test->prev, test->next);

    while (1) {
        if (ascending)
            while ((node_t != node_h) &&
                   (self->comparator (node_t->item, test->item) >= 0))
                node_t = node_t->prev;
        else
            while ((node_t != node_h) &&
                   (self->comparator (node_t->item, test->item) <= 0))
                node_t = node_t->prev;

        if (node_t == node_h) {
            s_node_relink (test, node_h, node_h->next);
            break;
        }

        // Move node_t to the position after node_h
        tmp = node_t->next;
        s_node_relink (node_t, node_t->prev, node_t->next);
        s_node_relink (node_t, node_h, node_h->next);
        node_h = node_t->next;
        node_t = tmp;

        if (ascending)
            while ((node_h != node_t) &&
                   (self->comparator (node_h->item, test->item) <= 0))
                node_h = node_h->next;
        else
            while ((node_h != node_t) &&
                   (self->comparator (node_h->item, test->item) >= 0))
                node_h = node_h->next;

        if (node_h == node_t) {
            s_node_relink (test, node_t->prev, node_t);
            break;
        }

        // Move node_h to the position before node_t
        tmp = node_h->prev;
        s_node_relink (node_h, node_h->prev, node_h->next);
        s_node_relink (node_h, node_t->prev, node_t);
        node_t = node_h->prev;
        node_h = tmp;
    }

    listx_quick_sort (self, first_node->next, test->prev, ascending);
    listx_quick_sort (self, test->next, last_node->prev, ascending);
}


// Binary search of item in a sorted list which is equal to the given item.
// Return struct s_indicator_t which includes the index of item and
// coorsponding node pointer. If item is not found, the index is SIZE_NONE
// (by which you can know that item is not found), and the node pointer is the
// node just sorted after the given item.
static s_indicator_t listx_binary_search (const listx_t *self, const void *item) {
    size_t head = 0,
           tail = listx_size (self),
           mid;
    s_node_t *node_head = self->head->next,
             *node_tail = self->head,
             *node_mid;

    int compare_result;

    if (listx_is_sorted_ascending (self)) {
        while (head < tail) {
            mid = (head + tail) / 2;
            node_mid = s_node_walk (node_head, mid - head, true);
            compare_result = self->comparator (node_mid->item, item);
            if (compare_result == 0)
                break;
            else if (compare_result > 0) {
                tail = mid;
                node_tail = node_mid;
            }
            else {
                head = mid + 1;
                node_head = node_mid->next;
            }
        }
    }
    else if (listx_is_sorted_descending (self)) {
        while (head < tail) {
            mid = (head + tail) / 2;
            node_mid = s_node_walk (node_head, mid - head, true);
            compare_result = self->comparator (node_mid->item, item);
            if (compare_result == 0)
                break;
            else if (compare_result < 0) {
                tail = mid;
                node_tail = node_mid;
            }
            else {
                head = mid + 1;
                node_head = node_mid->next;
            }
        }
    }

    s_indicator_t indicator = (head < tail) ?
                              (s_indicator_t) {mid, node_mid} :
                              (s_indicator_t) {SIZE_NONE, node_head};
    return indicator;
}


// Find item in list.
// Return struct s_indicator_t which includes the index of item and
// coorsponding node pointer. If item is not found, return {SIZE_NONE, NULL}.
static s_indicator_t listx_find_node (const listx_t *self, const void *item) {
    s_indicator_t indicator = {SIZE_NONE, NULL};
    if (self->size == 0)
        return indicator;

    // For sorted list, do binary search
    if (listx_is_sorted (self)) {
        indicator = listx_binary_search (self, item);
        if (indicator.index == SIZE_NONE) {
            indicator.node = NULL;
            return indicator;
        }
    }

    // For not-sorted list, search from head to tail
    else {
        indicator.index = 0;
        indicator.node = self->head->next;
        while (indicator.node != self->head) {
            if (self->comparator ((indicator.node)->item, item) == 0)
                return indicator;
            indicator.index++;
            indicator.node = (indicator.node)->next;
        }
        // Not found
        indicator.index = SIZE_NONE;
        indicator.node = NULL;
    }

    return indicator;
}


// ---------------------------------------------------------------------------


listx_t *listx_new () {
    listx_t *self = (listx_t *) malloc (sizeof (listx_t));
    assert (self);
    self->head = s_node_new (NULL);
    self->size = 0;
    self->sorting_state = LIST4X_NOT_SORTED;
    self->destructor = NULL;
    self->duplicator = NULL;
    self->comparator = NULL;
    self->printer = NULL;
    return self;
}


void listx_free (listx_t **self_p) {
    assert (self_p);
    if (*self_p) {
        listx_t *self = *self_p;
        listx_purge (self);
        free (self->head);
        free (self);
        *self_p = NULL;
    }
}


void listx_set_destructor (listx_t *self, destructor_t destructor) {
    assert (self);
    self->destructor = destructor;
}


void listx_set_duplicator (listx_t *self,
                                duplicator_t duplicator) {
    assert (self);
    self->duplicator = duplicator;
}


void listx_set_comparator (listx_t *self, comparator_t comparator) {
    assert (self);
    self->comparator = comparator;
}


void listx_set_printer (listx_t *self, printer_t printer) {
    assert (self);
    self->printer = printer;
}


size_t listx_size (const listx_t *self) {
    assert (self);
    return self->size;
}


bool listx_is_sorted (const listx_t *self) {
    assert (self);
    return self->sorting_state != LIST4X_NOT_SORTED;
}


bool listx_is_sorted_ascending (const listx_t *self) {
    assert (self);
    return self->sorting_state == LIST4X_ASCENDING_SORTED;
}


bool listx_is_sorted_descending (const listx_t *self) {
    assert (self);
    return self->sorting_state == LIST4X_DESCENDING_SORTED;
}


void *listx_item (const listx_t *self, const void *handle) {
    assert (self);
    assert (handle);
    assert (((s_node_t *) handle)->attached);
    return ((s_node_t *) handle)->item;
}


void *listx_item_at (const listx_t *self, size_t index) {
    assert (self);
    assert (index < self->size);
    s_node_t *node = listx_node (self, index);
    return node->item;
}


void *listx_first (const listx_t *self) {
    assert (self);
    return (self->head->next == self->head) ?
           NULL :
           self->head->next->item;
}


void *listx_last (const listx_t *self) {
    assert (self);
    return (self->head->prev == self->head) ?
           NULL :
           self->head->prev->item;
}


void listx_set_item (listx_t *self, void *handle, void *item) {
    assert (self);
    assert (handle);
    assert (item);

    s_node_t *node = (s_node_t *) handle;
    assert (node->attached);

    // Replace node->item with new item
    if (node->item && self->destructor)
        self->destructor (&node->item);

    if (self->duplicator) {
        item = self->duplicator (item);
        assert (item);
    }

    node->item = item;

    // Adjust sorting state
    if (!listx_node_is_sorted_with_prev (self, node) ||
        !listx_node_is_sorted_with_next (self, node))
        listx_set_not_sorted (self);
}


void listx_set_item_at (listx_t *self, size_t index, void *item) {
    assert (self);
    assert (index < self->size);
    assert (item);
    s_node_t *node = listx_node (self, index);
    listx_set_item (self, node, item);
}


void *listx_prepend (listx_t *self, void *item) {
    assert (self);
    assert (item);

    s_node_t *node = listx_new_node (self, item);
    s_node_relink (node, self->head, self->head->next);
    node->attached = true;
    self->size++;

    // Adjust sorting state
    if (!listx_node_is_sorted_with_next (self, node))
        listx_set_not_sorted (self);

    return node;
}


void *listx_append (listx_t *self, void *item) {
    assert (self);
    assert (item);

    s_node_t *node = listx_new_node (self, item);
    s_node_relink (node, self->head->prev, self->head);
    node->attached = true;
    self->size++;

    // Adjust sorting state
    if (!listx_node_is_sorted_with_prev (self, node))
        listx_set_not_sorted (self);

    return node;
}


void *listx_pop (listx_t *self, void *handle) {
    assert (self);
    assert (handle);
    s_node_t *node = (s_node_t *) handle;
    assert (node->attached);
    s_node_relink (node, node->prev, node->next);
    void *item = node->item;
    free (node);
    self->size--;
    return item;
}


void *listx_pop_first (listx_t *self) {
    assert (self);
    if (self->size == 0)
        return NULL;
    return listx_pop (self, self->head->next);
}


void *listx_pop_last (listx_t *self) {
    assert (self);
    if (self->size == 0)
        return NULL;
    return listx_pop (self, self->head->prev);
}


void *listx_pop_at (listx_t *self, size_t index) {
    assert (self);
    assert (index < listx_size (self));
    s_node_t *node = listx_node (self, index);
    return listx_pop (self, node);
}


void listx_extend (listx_t *self, const listx_t *list) {
    assert (self);
    assert (list);
    for (s_node_t *node = list->head->next;
         node != list->head;
         node = node->next)
        listx_append (self, node->item);
}


void listx_extend_array (listx_t *self, void **array, size_t length) {
    assert (self);
    assert (array);

    s_node_t *node;
    for (size_t index = 0; index < length; index++) {
        node = listx_new_node (self, array[index]);
        listx_append (self, node);
    }
}


void *listx_insert_at (listx_t *self, size_t index, void *item) {
    assert (self);
    assert (index < listx_size (self));
    assert (item);

    s_node_t *node = listx_node (self, index);
    s_node_t *new_node = listx_new_node (self, item);
    s_node_relink (new_node, node->prev, node);
    new_node->attached = true;
    self->size++;

    // Adjust sorting state
    if (!listx_node_is_sorted_with_prev (self, new_node) ||
        !listx_node_is_sorted_with_next (self, new_node))
        listx_set_not_sorted (self);

    return new_node;
}


void *listx_insert_sorted (listx_t *self, void *item) {
    assert (self);

    // If list is not sorted, sort it in ascending order
    if (!listx_is_sorted (self)) {
        print_warning ("List is not sorted yet. Sort it in ascending order.\n");
        listx_sort (self, true);
    }

    // Create new node from item
    s_node_t *node = listx_new_node (self, item);

    // Insert the new node to the right position
    // listx_reorder() will deal with list size correctly
    listx_reorder (self, node);

    return node;
}


void listx_remove (listx_t *self, void *handle) {
    assert (self);
    assert (handle);
    void *item = listx_pop (self, handle);
    if (self->destructor)
        self->destructor (&item);
}


void listx_remove_item (listx_t *self, void *item) {
    assert (self);
    assert (item);

    // If list is sorted, find item then count to head and tail
    if (listx_is_sorted (self)) {
        s_node_t *node_found = (s_node_t *) listx_find (self, item);
        if (node_found == NULL)
            return;

        // Remove to tail
        s_node_t *node = node_found->next;
        s_node_t *next;
        while (node != self->head) {
            next = node->next;
            if (self->comparator (node->item, item) == 0)
                listx_remove (self, node);
            else
                break;
            node = next;
        }

        // Remove to head
        node = node_found->prev;
        while (node != self->head) {
            next = node->prev;
            if (self->comparator (node->item, item) == 0)
                listx_remove (self, node);
            else
                break;
            node = next;
        }

        // Remove the found node
        listx_remove (self, node_found);
    }

    // If list is not sorted, remove from head to tail
    else {
        s_node_t *node = self->head->next;
        s_node_t *next;
        while (node != self->head) {
            next = node->next;
            if (self->comparator (node->item, item) == 0)
                listx_remove (self, node);
            node = next;
        }
    }
}


void listx_remove_first (listx_t *self) {
    assert (self);
    if (self->size == 0)
        return;
    listx_remove (self, self->head->next);
}


void listx_remove_last (listx_t *self) {
    assert (self);
    if (self->size == 0)
        return;
    listx_remove (self, self->head->prev);
}


void listx_remove_at (listx_t *self, size_t index) {
    assert (self);
    assert (index < listx_size (self));
    s_node_t *node = listx_node (self, index);
    listx_remove (self, node);
}


void listx_remove_slice (listx_t *self, size_t from_index, size_t to_index) {
    assert (self);
    assert (from_index <= to_index);
    assert (to_index < listx_size (self));
    s_node_t *node_preceding = listx_node (self, from_index)->prev;
    for (size_t cnt = 0; cnt < to_index - from_index + 1; cnt++)
        listx_remove (self, node_preceding->next);
}


void listx_purge (listx_t *self) {
    assert (self);
    if (self->size == 0)
        return;
    listx_remove_slice (self, 0, self->size - 1);
}


void listx_sort (listx_t *self, bool ascending) {
    assert (self);

    // If the list is sorted in opposite order, reverse it
    if ((ascending && listx_is_sorted_descending (self)) ||
        (!ascending && listx_is_sorted_ascending (self)))
        listx_reverse (self);

    // Otherwise, do a quick sort
    else {
        listx_quick_sort (self,
                          self->head->next,
                          self->head->prev,
                          ascending);
        listx_set_sorted (self, ascending);
    }
}


void listx_reorder (listx_t *self, void *handle) {
    assert (self);
    assert (handle);
    // list should be sorted
    assert (listx_is_sorted (self));

    s_node_t *node = (s_node_t *) handle;

    // If node is in list, detach it first
    if (node->attached) {
        s_node_relink (node, node->prev, node->next);
        // bug fixed: adjust list size, or binary search can not do right.
        self->size--;
    }

    // Binary search the position to insert
    s_indicator_t indicator = listx_binary_search (self, node->item);
    s_node_t *node_after = indicator.node;

    // Insert the node back
    s_node_relink (node, node_after->prev, node_after);
    node->attached = true; // in case that the node is not in list before
    self->size++;

    // @todo remove the following assertions after test
    // assert (listx_node_is_sorted_with_prev (self, node));
    // assert (listx_node_is_sorted_with_next (self, node));
}


void listx_reverse (listx_t *self) {
    assert (self);

    bool is_sorted_before = listx_is_sorted (self);
    bool ascending_before = listx_is_sorted_ascending (self);

    s_node_t *node = self->head,
             *next = node->next;

    while (next != self->head) {
        next = node->next;
        node->next = node->prev;
        node->prev = next;
        node = next;
    }

    // s_node_t *node_head = self->head->next,
    //          *node_tail = self->head->prev;
    // void *tmp;

    // while (node_head != node_tail && node_head->prev != node_tail) {
    //     // swap items
    //     tmp = node_head->item;
    //     node_head->item = node_tail->item;
    //     node_tail->item = tmp;

    //     node_head = node_head->next;
    //     node_tail = node_tail->prev;
    // }

    if (is_sorted_before)
        listx_set_sorted (self, !ascending_before);
}


void listx_shuffle (listx_t *self, rng_t *rng) {
    assert (self);

    // Set as not sorted regardless of actual state
    listx_set_not_sorted (self);

    size_t size = listx_size (self);
    if (size <= 1)
        return;

    bool own_rng = false;
    if (rng == NULL) {
        rng = rng_new ();
        own_rng = true;
    }

    size_t index = 0, index_swap;
    s_node_t *node = self->head->next, *node_swap, *prev;

    // from the first node to the node before last, or [0, size-2]
    while (node->next != self->head) {
        // prev is untouched during swap
        prev = node->prev;

        // random index in [index, size-1]
        index_swap = (size_t) rng_random_int (rng, index, size);

        // Swap item at index and index_swap
        if (index_swap > index) {
            node_swap = s_node_walk (node, index_swap - index, true);

            // Move node after node_swap
            s_node_relink (node, node->prev, node->next);
            s_node_relink (node, node_swap, node_swap->next);

            // Move node_swap after prev
            if (index_swap > index + 1) {
                s_node_relink (node_swap, node_swap->prev, node_swap->next);
                s_node_relink (node_swap, prev, prev->next);
            }
        }

        index++;
        node = prev->next->next;
    }

    if (own_rng)
        rng_free (&rng);

    // size_t i, j;
    // s_node_t *node_i, *node_j;
    // void *tmp;

    // for (i = 0, node_i = self->head->next;
    //      i < size - 1;
    //      i++, node_i = node_i->next) {
    //     j = (size_t) rng_random_int (rng, i, size); // [i, size-1]
    //     node_j = s_node_walk (node_i, j-i, true);
    //     tmp = node_j->item;
    //     node_j->item = node_i->item;
    //     node_i->item = tmp;
    // }
}


size_t listx_index (const listx_t *self, const void *item) {
    assert (self);
    assert (item);
    s_indicator_t indicator = listx_find_node (self, item);
    return indicator.index;
}


void *listx_find (const listx_t *self, const void *item) {
    assert (self);
    assert (item);
    s_indicator_t indicator = listx_find_node (self, item);
    return indicator.node;
}


bool listx_includes (const listx_t *self, const void *item) {
    assert (self);
    assert (item);
    return listx_find (self, item) != NULL;
}


bool listx_includes_identical (const listx_t *self, const void *item) {
    assert (self);
    assert (item);
    s_node_t *node = self->head->next;
    while (node != self->head) {
        if (node->item == item)
            return true;
        node = node->next;
    }
    return false;
}


size_t listx_count (const listx_t *self, const void *item) {
    assert (self);
    assert (item);
    size_t cnt;

    // If list is sorted, find value then count to head and tail
    if (listx_is_sorted (self)) {
        s_node_t *node_found = (s_node_t *) listx_find (self, item);
        if (node_found == NULL)
            return 0;
        cnt = 1;
        // count to tail
        s_node_t *node = node_found->next;
        while (node != self->head) {
            if (self->comparator (node->item, item) == 0)
                cnt++;
            else
                break;
            node = node->next;
        }
        // count to head
        node = node_found->prev;
        while (node != self->head) {
            if (self->comparator (node->item, item) == 0)
                cnt++;
            else
                break;
            node = node->prev;
        }
    }

    // If list is not sorted, count from head to tail
    else {
        cnt = 0;
        for (s_node_t *node = self->head->next;
             node != self->head;
             node = node->next) {
            if (self->comparator (node->item, item) == 0)
                cnt++;
        }
    }

    return cnt;
}


// void *listx_next (listx_t *self, void *handle) {
//     assert (self);
//     assert (handle);
//     assert (((s_node_t *) handle)->attached);
//     s_node_t *node = ((s_node_t *) handle)->next;
//     return node == self->head ? NULL : node->item;
// }


// void *listx_prev (listx_t *self, void *handle) {
//     assert (self);
//     assert (handle);
//     assert (((s_node_t *) handle)->attached);
//     s_node_t *node = ((s_node_t *) handle)->prev;
//     return node == self->head ? NULL : node->item;
// }


// void *listx_prev_handle (listx_t *self, void *handle) {
//     assert (self);
//     assert (handle);
//     assert (((s_node_t *) handle)->attached);
//     return ((s_node_t *) handle)->prev == self->head ?
//            NULL :
//            ((s_node_t *) handle)->prev;
// }


// void *listx_next_handle (listx_t *self, void *handle) {
//     assert (self);
//     assert (handle);
//     assert (((s_node_t *) handle)->attached);
//     return ((s_node_t *) handle)->next == self->head ?
//            NULL :
//            ((s_node_t *) handle)->next;
// }


listx_iterator_t listx_iter_init (const listx_t *self, bool forward) {
    assert (self);
    return (listx_iterator_t) {self->head, forward};
}


listx_iterator_t listx_iter_init_from (const listx_t *self,
                                       void *handle,
                                       bool forward) {
    assert (self);
    assert (handle);
    s_node_t *node = (s_node_t *) handle;
    assert (node->attached);
    return (listx_iterator_t) {node, forward};
}


void *listx_iter (const listx_t *self, listx_iterator_t *iterator) {
    assert (self);
    assert (iterator);
    iterator->handle = iterator->forward ?
                       ((s_node_t *)(iterator->handle))->next :
                       ((s_node_t *)(iterator->handle))->prev;

    return (iterator->handle == self->head) ?
           NULL :
           ((s_node_t *)(iterator->handle))->item;
}


void *listx_iter_pop (listx_t *self, listx_iterator_t *iterator) {
    assert (self);
    assert (iterator);

    s_node_t *node = (s_node_t *)(iterator->handle);
    assert (node != self->head);

    // Move handle back to last position
    iterator->handle = iterator->forward ?
                       node->prev :
                       node->next;

    return listx_pop (self, node);
}


void listx_iter_remove (listx_t *self, listx_iterator_t *iterator) {
    assert (self);
    assert (iterator);
    assert ((s_node_t *)(iterator->handle) != self->head);
    void *item = listx_iter_pop (self, iterator);
    if (self->destructor)
        self->destructor (&item);
}


listx_t *listx_map (const listx_t *self, mapping_t mapping) {
    assert (self);
    assert (mapping);

    listx_t *output = listx_new ();
    assert (output);

    for (s_node_t *node = self->head->next;
         node != self->head;
         node = node->next)
        listx_append (output, mapping (node->item));

    return output;
}


void *listx_reduce (const listx_t *self, reducer_t reducer, void *initial) {
    assert (self);
    assert (reducer);

    void *output = initial;
    void *tmp = NULL;

    for (s_node_t *node = self->head->next;
         node != self->head;
         node = node->next) {
        tmp = output;
        output = reducer (output, node->item);
        // Free intermediate combining results except for the initial
        if (self->destructor && tmp != initial)
            self->destructor (&tmp);
    }

    return output;
}


listx_t *listx_filter (const listx_t *self, filter_t filter) {
    assert (self);
    assert (filter);

    listx_t *output = listx_new ();
    assert (output);

    output->sorting_state = self->sorting_state;
    output->destructor = self->destructor;
    output->duplicator = self->duplicator;
    output->comparator = self->comparator;
    output->printer = self->printer;

    for (s_node_t *node = self->head->next;
         node != self->head;
         node = node->next)
        if (filter (node->item))
            listx_append (output, node->item);

    return output;
}


bool listx_all (const listx_t *self, filter_t filter) {
    assert (self);
    assert (filter);
    for (s_node_t *node = self->head->next;
         node != self->head;
         node = node->next)
        if (!filter (node->item))
            return false;
    return true;
}


bool listx_any (const listx_t *self, filter_t filter) {
    assert (self);
    assert (filter);
    for (s_node_t *node = self->head->next;
         node != self->head;
         node = node->next)
        if (filter (node->item))
            return true;
    return false;
}


listx_t *listx_dup (const listx_t *self) {
    if (!self)
        return NULL;

    listx_t *copy = listx_new ();

    copy->sorting_state = self->sorting_state;
    copy->destructor = self->destructor;
    copy->duplicator = self->duplicator;
    copy->comparator = self->comparator;
    copy->printer = self->printer;

    //  Copy nodes
    for (s_node_t *node = self->head->next;
         node != self->head;
         node = node->next)
        listx_append (copy, node->item);

    return copy;
}


bool listx_equal (const listx_t *self, const listx_t *list) {
    if (self == NULL && list == NULL)
        return true;
    else if (self == NULL || list == NULL)
        return false;

    assert (self->comparator);

    if (self->size != list->size)
        return false;

    bool result = true;
    s_node_t *node1 = self->head->next;
    s_node_t *node2 = list->head->next;
    for (size_t index = 0; index < self->size; index++) {
        if (self->comparator (node1->item, node2->item) != 0) {
            result = false;
            break;
        }
        node1 = node1->next;
        node2 = node2->next;
    }
    return result;
}


void listx_print (const listx_t *self) {
    assert (self);
    printf ("\nlistx size: %zu, sorted: %s\n", self->size,
            (self->sorting_state == LIST4X_ASCENDING_SORTED) ?
            "ascending" : (self->sorting_state == LIST4X_DESCENDING_SORTED ?
                          "descending" :
                          "no"));
    printf ("destructor: %s, duplicator: %s, comparator: %s, printer: %s\n",
            self->destructor ? "set" : "none",
            self->duplicator ? "set" : "none",
            self->comparator ? "set" : "none",
            self->printer    ? "set" : "none");
    printf ("-----------------------------------------------------------------\n");
    if (self->printer) {
        size_t index = 0;
        s_node_t *node = self->head->next;
        while (node != self->head) {
            printf ("index %zu: ", index);
            self->printer (node->item);
            index++;
            node = node->next;
        }
        printf ("\n");
    }
}


void listx_assert_sort (const listx_t *self, const char *order) {
    assert (self);
    if (streq (order, "ascending")) {
        assert (listx_is_sorted (self));
        assert (listx_is_sorted_ascending (self));
        assert (!listx_is_sorted_descending (self));
    }
    else if (streq (order, "descending")) {
        assert (listx_is_sorted (self));
        assert (!listx_is_sorted_ascending (self));
        assert (listx_is_sorted_descending (self));
    }
    else if (streq (order, "no")) {
        assert (!listx_is_sorted (self));
        assert (!listx_is_sorted_ascending (self));
        assert (!listx_is_sorted_descending (self));
    }
    else
        assert (false);

    if (listx_is_sorted (self)) {
        assert (self->comparator);
        s_node_t *node = self->head->next;
        while (node != self->head) {
            assert (listx_node_is_sorted_with_prev (self, node));
            assert (listx_node_is_sorted_with_next (self, node));
            node = node->next;
        }
    }
}


void listx_test (bool verbose) {
    print_info (" * listx: \n");

    listx_t *list = listx_new ();
    assert (list);
    assert (listx_size (list) == 0);
    assert (listx_is_sorted (list) == false);
    assert (listx_is_sorted_ascending (list) == false);
    assert (listx_is_sorted_descending (list) == false);

    // Operations on empty list
    assert (listx_first (list) == NULL);
    assert (listx_last (list) == NULL);
    assert (listx_pop_first (list) == NULL);
    assert (listx_pop_last (list) == NULL);

    printf ("\nIterate empty list: \n");
    listx_iterator_t iter = listx_iter_init (list, true);
    char *str;
    while ((str = (char *) listx_iter (list, &iter)) != NULL)
        printf ("%s, ", str);
    printf ("\n");

    listx_set_destructor (list, (destructor_t) string_free);
    listx_set_duplicator (list, (duplicator_t) string_duplicate);
    listx_set_comparator (list, (comparator_t) string_compare);
    listx_set_printer (list, (printer_t) string_print);

    assert (listx_find (list, "hello") == NULL);
    assert (listx_index (list, "hello") == SIZE_NONE);

    listx_sort (list, true);
    assert (listx_size (list) == 0);
    assert (listx_is_sorted (list));
    assert (listx_is_sorted_ascending (list));
    assert (listx_is_sorted_descending (list) == false);
    listx_sort (list, false);
    assert (listx_size (list) == 0);
    assert (listx_is_sorted (list));
    assert (listx_is_sorted_descending (list));
    assert (listx_is_sorted_ascending (list) == false);
    listx_reverse (list);
    assert (listx_size (list) == 0);
    assert (listx_is_sorted (list));
    assert (listx_is_sorted_ascending (list));
    assert (listx_is_sorted_descending (list) == false);
    rng_t *rng = rng_new ();
    listx_shuffle (list, rng);
    rng_free (&rng);
    assert (listx_size (list) == 0);
    assert (listx_is_sorted (list) == false);
    assert (listx_is_sorted_ascending (list) == false);
    assert (listx_is_sorted_descending (list) == false);
    listx_purge (list);


    //
    listx_append (list, "world");
    assert (listx_size (list) == 1);
    assert (streq ((char *) listx_first (list), "world"));
    assert (streq ((char *) listx_last (list), "world"));
    assert (streq ((char *) listx_item_at (list, 0), "world"));
    listx_print (list);

    listx_append (list, "hello");
    listx_print (list);
    assert (listx_size (list) == 2);
    assert (streq ((char *) listx_first (list), "world"));
    assert (streq ((char *) listx_last (list), "hello"));
    assert (streq ((char *) listx_item_at (list, 0), "world"));
    assert (streq ((char *) listx_item_at (list, 1), "hello"));
    assert (listx_index (list, "hello") == 1);
    assert (listx_index (list, "world") == 0);
    void *h1 = listx_find (list, "world");
    void *h2 = listx_find (list, "hello");
    assert (streq ((char *) listx_item (list, h1), "world"));
    assert (streq ((char *) listx_item (list, h2), "hello"));

    listx_sort (list, true);
    listx_print (list);

    listx_append (list, "mini");
    listx_sort (list, false);
    listx_print (list);
    listx_sort (list, true);
    listx_print (list);
    print_debug ("");

    assert (listx_size (list) == 3);
    assert (listx_index (list, "hello") == 0);
    assert (listx_index (list, "world") == 2);
    assert (streq (listx_item_at (list, 0), "hello"));
    assert (streq (listx_item_at (list, 1), "mini"));
    listx_remove_at (list, 0);
    assert (listx_size (list) == 2);
    listx_print (list);

    char *string = (char *) listx_pop_last (list);
    assert (streq (string, "world"));
    free (string);
    assert (listx_size (list) == 1);

    listx_purge (list);
    assert (listx_size (list) == 0);

    // Now populate the list with items
    listx_prepend (list, "five");
    listx_append (list, "six");
    listx_prepend (list, "four");
    listx_append (list, "seven");
    listx_prepend (list, "three");
    listx_append (list, "eight");
    listx_prepend (list, "two");
    listx_append (list, "nine");
    listx_prepend (list, "one");
    listx_append (list, "ten");
    listx_print (list);

    // Reverse
    for (size_t i = 0; i < 101; i++)
        listx_reverse (list);
    print_debug ("after reverse: \n");
    listx_print (list);

    // Shuffle
    rng = rng_new ();
    for (size_t i = 0; i < 100; i++)
        listx_shuffle (list, rng);
    print_debug ("after shuffle: \n");
    listx_print (list);
    rng_free (&rng);


    // Sort by alphabetical order
    listx_sort (list, true);
    print_debug ("after sort: \n");
    listx_print (list);
    assert (streq ((char *) listx_first (list), "eight"));
    assert (streq ((char *) listx_last (list), "two"));
    listx_reverse (list);
    listx_print (list);

    listx_insert_sorted (list, "eleven");
    listx_insert_sorted (list, "eleven");
    void *h3 = listx_insert_sorted (list, "eleven");
    listx_remove (list, h3);
    listx_insert_sorted (list, "twelve");
    print_debug ("");
    listx_print (list);

    listx_remove_item (list, "eleven");
    listx_remove_item (list, "six");
    print_debug ("");
    listx_print (list);

    assert (listx_includes (list, "nine"));
    assert (listx_includes (list, "ninty") == false);
    assert (streq ((char *) listx_item_at (list, listx_index (list, "three")), "three"));
    assert (streq ((char *) listx_item (list, listx_find (list, "one")), "one"));

    // Copy a list
    listx_t *copy = listx_dup (list);
    assert (copy);
    assert (listx_equal (list, copy));
    assert (listx_size (copy) == 10);
    listx_sort (copy, false);
    listx_print (copy);
    assert (streq ((char *) listx_first (copy), "two"));
    assert (streq ((char *) listx_last (copy), "eight"));
    listx_free (&copy);

    // Insert to sorted list
    listx_sort (list, false);
    void *handle = listx_insert_sorted (list, "fk");
    assert (streq (listx_item (list, handle), "fk"));
    listx_print (list);

    // iteration
    iter = listx_iter_init (list, true);
    while ((str = (char *) listx_iter (list, &iter)) != NULL)
        printf ("%s, ", str);
    printf ("\n");

    iter = listx_iter_init (list, false);
    while ((str = (char *) listx_iter (list, &iter)) != NULL) {
        if (streq (str, "eleven"))
            listx_iter_remove (list, &iter);
        // printf ("%s, ", str);
    }
    // printf ("\n")

    iter = listx_iter_init (list, true);
    while ((str = (char *) listx_iter (list, &iter)) != NULL)
        printf ("%s, ", str);
    printf ("\n");

    listx_sort (list, false);
    listx_print (list);
    handle = listx_find (list, "ten");
    print_debug ("");
    iter = listx_iter_init_from (list, handle, false);
    while ((str = (char *) listx_iter (list, &iter)) != NULL)
        printf ("%s, ", str);
    printf ("\n");

    assert (streq (listx_item (list, handle), "ten"));
    char *item = listx_item (list, handle);
    item[0] = 'a';
    listx_print (list);
    listx_reorder (list, handle);
    assert (streq (listx_item (list, handle), "aen"));
    listx_print (list);

    listx_purge (list);
    listx_sort (list, false);
    listx_insert_sorted (list, "mall");
    listx_insert_sorted (list, "hotel");
    listx_insert_sorted (list, "zoo");
    listx_print (list);


    listx_free (&list);
    print_info ("OK\n");
}
