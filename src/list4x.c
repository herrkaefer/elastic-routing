/*  =========================================================================
    list4x - a generic list container

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/
/* @todo

- [x]add list4x_iterator_t for iteration. 使用内置cursor变量的缺点是不支持对同一个
  list的循环嵌套。

- [x]优化list4x_count()

- [x]写handle不变的sort, reverse, shuffle, ...

- [x]不用reorder，改用list4x_pop(), change item, then insert_sorted()

- [x]list4x_insert_sorted () and list4x_reorder () 需要进一步test，主要是
  binary search寻找位置尚不能保证一定准确。

- [x]s_node_t: Add a indicator: whether the node is in list or not.

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


struct _list4x_t {
    s_node_t *head;
    size_t size;
    int sorting_state;
    free_func_t      destructor;
    duplicate_func_t duplicator;
    compare_func_t   comparator;
    print_func_t     printer;
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


// Create a new node from item for list.
// Duplicate the item if deplicate function is set.
static s_node_t *list4x_new_node (list4x_t *self, void *item) {
    if (self->duplicator) {
        item = self->duplicator (item);
        assert (item);
    }
    s_node_t *node = s_node_new (item);
    return node;
}


// Get node by index
static s_node_t *list4x_node (list4x_t *self, size_t index) {
    return (index < self->size / 2) ?
           s_node_walk (self->head->next, index, true) :
           s_node_walk (self->head->prev, self->size-index-1, false);
}


static void list4x_set_sorted (list4x_t *self, bool ascending) {
    self->sorting_state = ascending ?
                          LIST4X_ASCENDING_SORTED :
                          LIST4X_DESCENDING_SORTED;
}


static void list4x_set_not_sorted (list4x_t *self) {
    self->sorting_state = LIST4X_NOT_SORTED;
}


// Check if node is sorted properly with its previous node
static bool list4x_node_is_sorted_with_prev (list4x_t *self,
                                             s_node_t *node) {
    if (node->prev == self->head) // first node
        return list4x_is_sorted (self);
    else
        return (list4x_is_sorted_ascending (self) &&
                self->comparator (node->item, node->prev->item) >= 0) ||
               (list4x_is_sorted_descending (self) &&
                self->comparator (node->item, node->prev->item) <= 0);
}


// Check if node is sorted properly with its next node
static bool list4x_node_is_sorted_with_next (list4x_t *self,
                                             s_node_t *node) {
    if (node->next == self->head) // last node
        return list4x_is_sorted (self);
    else
        return (list4x_is_sorted_ascending (self) &&
                self->comparator (node->item, node->next->item) <= 0) ||
               (list4x_is_sorted_descending (self) &&
                self->comparator (node->item, node->next->item) >= 0);
}


// Quick sort of list
// Note that this implementation will detach items from their node handles
// static void list4x_quick_sort1 (list4x_t *self,
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

//     list4x_quick_sort1 (self, first_node, node_i->prev, i, ascending);
//     list4x_quick_sort1 (self, node_i->next, last_node, length-i-1, ascending);
// }


// Quick sort of list
// Note that this implementation will keep bindings between items and their
// handles.
static void list4x_quick_sort (list4x_t *self,
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

    list4x_quick_sort (self, first_node->next, test->prev, ascending);
    list4x_quick_sort (self, test->next, last_node->prev, ascending);
}


// Binary search of item in a sorted list which is equal to the given item.
// Return struct s_indicator_t which includes the index of item and
// coorsponding node pointer. If item is not found, the index is SIZE_NONE
// (by which you can know that item is not found), and the node pointer is the
// node just sorted after the given item.
static s_indicator_t list4x_binary_search (list4x_t *self, void *item) {
    size_t head = 0,
           tail = list4x_size (self),
           mid;
    s_node_t *node_head = self->head->next,
             *node_tail = self->head,
             *node_mid;

    int compare_result;

    if (list4x_is_sorted_ascending (self)) {
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
    else if (list4x_is_sorted_descending (self)) {
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
static s_indicator_t list4x_find_node (list4x_t *self, void *item) {
    s_indicator_t indicator = {SIZE_NONE, NULL};
    if (self->size == 0)
        return indicator;

    // For sorted list, do binary search
    if (list4x_is_sorted (self)) {
        indicator = list4x_binary_search (self, item);
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


list4x_t *list4x_new () {
    list4x_t *self = (list4x_t *) malloc (sizeof (list4x_t));
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


void list4x_free (list4x_t **self_p) {
    assert (self_p);
    if (*self_p) {
        list4x_t *self = *self_p;
        list4x_purge (self);
        free (self->head);
        free (self);
        *self_p = NULL;
    }
}


void list4x_set_destructor (list4x_t *self, free_func_t destructor) {
    assert (self);
    self->destructor = destructor;
}


void list4x_set_duplicator (list4x_t *self,
                                duplicate_func_t duplicator) {
    assert (self);
    self->duplicator = duplicator;
}


void list4x_set_comparator (list4x_t *self, compare_func_t comparator) {
    assert (self);
    self->comparator = comparator;
}


void list4x_set_printer (list4x_t *self, print_func_t printer) {
    assert (self);
    self->printer = printer;
}


size_t list4x_size (list4x_t *self) {
    assert (self);
    return self->size;
}


bool list4x_is_sorted (list4x_t *self) {
    assert (self);
    return self->sorting_state != LIST4X_NOT_SORTED;
}


bool list4x_is_sorted_ascending (list4x_t *self) {
    assert (self);
    return self->sorting_state == LIST4X_ASCENDING_SORTED;
}


bool list4x_is_sorted_descending (list4x_t *self) {
    assert (self);
    return self->sorting_state == LIST4X_DESCENDING_SORTED;
}


void *list4x_item (list4x_t *self, void *handle) {
    assert (self);
    assert (handle);
    assert (((s_node_t *) handle)->attached);
    return ((s_node_t *) handle)->item;
}


void *list4x_item_at (list4x_t *self, size_t index) {
    assert (self);
    assert (index < self->size);
    s_node_t *node = list4x_node (self, index);
    return node->item;
}


void *list4x_first (list4x_t *self) {
    assert (self);
    return (self->head->next == self->head) ?
           NULL :
           self->head->next->item;
}


void *list4x_last (list4x_t *self) {
    assert (self);
    return (self->head->prev == self->head) ?
           NULL :
           self->head->prev->item;
}


void list4x_set_item (list4x_t *self, void *handle, void *item) {
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
    if (!list4x_node_is_sorted_with_prev (self, node) ||
        !list4x_node_is_sorted_with_next (self, node))
        list4x_set_not_sorted (self);
}


void list4x_set_item_at (list4x_t *self, size_t index, void *item) {
    assert (self);
    assert (index < self->size);
    assert (item);
    s_node_t *node = list4x_node (self, index);
    list4x_set_item (self, node, item);
}


void *list4x_prepend (list4x_t *self, void *item) {
    assert (self);
    assert (item);

    s_node_t *node = list4x_new_node (self, item);
    s_node_relink (node, self->head, self->head->next);
    node->attached = true;
    self->size++;

    // Adjust sorting state
    if (!list4x_node_is_sorted_with_next (self, node))
        list4x_set_not_sorted (self);

    return node;
}


void *list4x_append (list4x_t *self, void *item) {
    assert (self);
    assert (item);

    s_node_t *node = list4x_new_node (self, item);
    s_node_relink (node, self->head->prev, self->head);
    node->attached = true;
    self->size++;

    // Adjust sorting state
    if (!list4x_node_is_sorted_with_prev (self, node))
        list4x_set_not_sorted (self);

    return node;
}


void *list4x_pop (list4x_t *self, void *handle) {
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


void *list4x_pop_first (list4x_t *self) {
    assert (self);
    if (self->size == 0)
        return NULL;
    return list4x_pop (self, self->head->next);
}


void *list4x_pop_last (list4x_t *self) {
    assert (self);
    if (self->size == 0)
        return NULL;
    return list4x_pop (self, self->head->prev);
}


void *list4x_pop_at (list4x_t *self, size_t index) {
    assert (self);
    assert (index < list4x_size (self));
    s_node_t *node = list4x_node (self, index);
    return list4x_pop (self, node);
}


void list4x_extend (list4x_t *self, list4x_t *list) {
    assert (self);
    assert (list);
    void *item;
    list4x_iterator_t iter = list4x_iter_init (list, true);
    while ((item = list4x_iter (list, &iter)) != NULL)
        list4x_append (self, item);
}


void list4x_extend_array (list4x_t *self, void **array, size_t length) {
    assert (self);
    assert (array);

    s_node_t *node;
    for (size_t index = 0; index < length; index++) {
        node = list4x_new_node (self, array[index]);
        list4x_append (self, node);
    }
}


void *list4x_insert_at (list4x_t *self, size_t index, void *item) {
    assert (self);
    assert (index < list4x_size (self));
    assert (item);

    s_node_t *node = list4x_node (self, index);
    s_node_t *new_node = list4x_new_node (self, item);
    s_node_relink (new_node, node->prev, node);
    new_node->attached = true;
    self->size++;

    // Adjust sorting state
    if (!list4x_node_is_sorted_with_prev (self, new_node) ||
        !list4x_node_is_sorted_with_next (self, new_node))
        list4x_set_not_sorted (self);

    return new_node;
}


void *list4x_insert_sorted (list4x_t *self, void *item) {
    assert (self);

    // If list is not sorted, sort it in ascending order
    if (!list4x_is_sorted (self)) {
        print_warning ("List is not sorted yet. Sort it in ascending order.\n");
        list4x_sort (self, true);
    }

    // Create new node from item
    s_node_t *node = list4x_new_node (self, item);

    // Insert the new node to the right position
    // list4x_reorder() will deal with list size correctly
    list4x_reorder (self, node);

    return node;
}


void list4x_remove (list4x_t *self, void *handle) {
    assert (self);
    assert (handle);
    void *item = list4x_pop (self, handle);
    if (self->destructor)
        self->destructor (&item);
}


void list4x_remove_item (list4x_t *self, void *item) {
    assert (self);
    assert (item);

    // If list is sorted, find item then count to head and tail
    if (list4x_is_sorted (self)) {
        s_node_t *node_found = (s_node_t *) list4x_find (self, item);
        if (node_found == NULL)
            return;

        // Remove to tail
        s_node_t *node = node_found->next;
        s_node_t *next;
        while (node != self->head) {
            next = node->next;
            if (self->comparator (node->item, item) == 0)
                list4x_remove (self, node);
            else
                break;
            node = next;
        }

        // Remove to head
        node = node_found->prev;
        while (node != self->head) {
            next = node->prev;
            if (self->comparator (node->item, item) == 0)
                list4x_remove (self, node);
            else
                break;
            node = next;
        }

        // Remove the found node
        list4x_remove (self, node_found);
    }

    // If list is not sorted, remove from head to tail
    else {
        s_node_t *node = self->head->next;
        s_node_t *next;
        while (node != self->head) {
            next = node->next;
            if (self->comparator (node->item, item) == 0)
                list4x_remove (self, node);
            node = next;
        }
    }
}


void list4x_remove_first (list4x_t *self) {
    assert (self);
    if (self->size == 0)
        return;
    list4x_remove (self, self->head->next);
}


void list4x_remove_last (list4x_t *self) {
    assert (self);
    if (self->size == 0)
        return;
    list4x_remove (self, self->head->prev);
}


void list4x_remove_at (list4x_t *self, size_t index) {
    assert (self);
    assert (index < list4x_size (self));
    s_node_t *node = list4x_node (self, index);
    list4x_remove (self, node);
}


void list4x_remove_slice (list4x_t *self, size_t from_index, size_t to_index) {
    assert (self);
    size_t size = list4x_size (self);
    assert (from_index <= to_index);
    assert (to_index < size);
    s_node_t *node_preceding = list4x_node (self, from_index)->prev;
    for (size_t cnt = 0; cnt < to_index - from_index + 1; cnt++)
        list4x_remove (self, node_preceding->next);
}


void list4x_purge (list4x_t *self) {
    assert (self);
    if (self->size == 0)
        return;
    list4x_remove_slice (self, 0, self->size - 1);
}


void list4x_sort (list4x_t *self, bool ascending) {
    assert (self);

    // If the list is sorted in opposite order, reverse it
    if ((ascending && list4x_is_sorted_descending (self)) ||
        (!ascending && list4x_is_sorted_ascending (self)))
        list4x_reverse (self);

    // Otherwise, do a quick sort
    else {
        list4x_quick_sort (self,
                           self->head->next,
                           self->head->prev,
                           ascending);
        list4x_set_sorted (self, ascending);
    }
}


void list4x_reorder (list4x_t *self, void *handle) {
    assert (self);
    assert (handle);
    // list should be sorted
    assert (list4x_is_sorted (self));

    s_node_t *node = (s_node_t *) handle;

    // If node is in list, detach it first
    if (node->attached) {
        s_node_relink (node, node->prev, node->next);
        // bug fixed: adjust list size, or binary search can not do right.
        self->size--;
    }

    // Binary search the position to insert
    s_indicator_t indicator = list4x_binary_search (self, node->item);
    s_node_t *node_after = indicator.node;

    // Insert the node back
    s_node_relink (node, node_after->prev, node_after);
    node->attached = true; // in case that the node is not in list before
    self->size++;

    // @todo remove the following assertions after test
    assert (list4x_node_is_sorted_with_prev (self, node));
    assert (list4x_node_is_sorted_with_next (self, node));
}


void list4x_reverse (list4x_t *self) {
    assert (self);

    bool is_sorted_before = list4x_is_sorted (self);
    bool ascending_before = list4x_is_sorted_ascending (self);

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
        list4x_set_sorted (self, !ascending_before);
}


void list4x_shuffle (list4x_t *self, rng_t *rng) {
    assert (self);

    // Set as not sorted regardless of actual state
    list4x_set_not_sorted (self);

    size_t size = list4x_size (self);
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


size_t list4x_index (list4x_t *self, void *item) {
    assert (self);
    assert (item);
    s_indicator_t indicator = list4x_find_node (self, item);
    return indicator.index;
}


void *list4x_find (list4x_t *self, void *item) {
    assert (self);
    assert (item);
    s_indicator_t indicator = list4x_find_node (self, item);
    return indicator.node;
}


bool list4x_includes (list4x_t *self, void *item) {
    assert (self);
    assert (item);
    return list4x_find (self, item) != NULL;
}


size_t list4x_count (list4x_t *self, void *item) {
    assert (self);
    assert (item);
    size_t cnt;

    // If list is sorted, find value then count to head and tail
    if (list4x_is_sorted (self)) {
        s_node_t *node_found = (s_node_t *) list4x_find (self, item);
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


// void *list4x_next (list4x_t *self, void *handle) {
//     assert (self);
//     assert (handle);
//     assert (((s_node_t *) handle)->attached);
//     s_node_t *node = ((s_node_t *) handle)->next;
//     return node == self->head ? NULL : node->item;
// }


// void *list4x_prev (list4x_t *self, void *handle) {
//     assert (self);
//     assert (handle);
//     assert (((s_node_t *) handle)->attached);
//     s_node_t *node = ((s_node_t *) handle)->prev;
//     return node == self->head ? NULL : node->item;
// }


list4x_iterator_t list4x_iter_init (list4x_t *self, bool forward) {
    assert (self);
    return (list4x_iterator_t) {self->head, forward};
}


list4x_iterator_t list4x_iter_init_from (list4x_t *self,
                                         void *handle,
                                         bool forward) {
    assert (self);
    assert (handle);
    s_node_t *node = (s_node_t *) handle;
    assert (node->attached);
    return (list4x_iterator_t) {node, forward};
}


void *list4x_iter (list4x_t *self, list4x_iterator_t *iterator) {
    assert (self);
    assert (iterator);
    iterator->handle = iterator->forward ?
                       ((s_node_t *)(iterator->handle))->next :
                       ((s_node_t *)(iterator->handle))->prev;

    return (iterator->handle == self->head) ?
           NULL :
           ((s_node_t *)(iterator->handle))->item;
}


void *list4x_iter_pop (list4x_t *self, list4x_iterator_t *iterator) {
    assert (self);
    assert (iterator);

    s_node_t *node = (s_node_t *)(iterator->handle);
    assert (node != self->head);

    // Move handle back to last position
    iterator->handle = iterator->forward ?
                       node->prev :
                       node->next;

    return list4x_pop (self, node);
}


void list4x_iter_remove (list4x_t *self, list4x_iterator_t *iterator) {
    assert (self);
    assert (iterator);
    assert ((s_node_t *)(iterator->handle) != self->head);
    void *item = list4x_iter_pop (self, iterator);
    if (self->destructor)
        self->destructor (&item);
}


list4x_t *list4x_duplicate (const list4x_t *self) {
    if (!self)
        return NULL;

    list4x_t *copy = list4x_new ();

    copy->sorting_state = self->sorting_state;
    copy->destructor = self->destructor;
    copy->duplicator = self->duplicator;
    copy->comparator = self->comparator;
    copy->printer = self->printer;

    //  Copy nodes
    for (s_node_t *node = self->head->next;
         node != self->head;
         node = node->next)
        list4x_append (copy, node->item);

    return copy;
}


bool list4x_equal (const list4x_t *self, const list4x_t *list) {
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


void list4x_print (const list4x_t *self) {
    assert (self);
    printf ("\nlist4x size: %zu, sorted: %s\n", self->size,
            (self->sorting_state == LIST4X_ASCENDING_SORTED) ?
            "ascending" : (self->sorting_state == LIST4X_DESCENDING_SORTED ?
                          "descending" :
                          "no"));
    printf ("destructor: %s, duplicator: %s, comparator: %s, printer: %s\n",
            self->destructor ? "set" : "none",
            self->duplicator ? "set" : "none",
            self->comparator ? "set" : "none",
            self->printer    ? "set" : "none");
    printf ("---------------------------------------------------------------\n");
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


void list4x_assert_sort (list4x_t *self, const char *order) {
    assert (self);
    if (streq (order, "ascending")) {
        assert (list4x_is_sorted (self));
        assert (list4x_is_sorted_ascending (self));
        assert (!list4x_is_sorted_descending (self));
    }
    else if (streq (order, "descending")) {
        assert (list4x_is_sorted (self));
        assert (!list4x_is_sorted_ascending (self));
        assert (list4x_is_sorted_descending (self));
    }
    else if (streq (order, "no")) {
        assert (!list4x_is_sorted (self));
        assert (!list4x_is_sorted_ascending (self));
        assert (!list4x_is_sorted_descending (self));
    }
    else
        assert (false);

    if (list4x_is_sorted (self)) {
        assert (self->comparator);
        s_node_t *node = self->head->next;
        while (node != self->head) {
            assert (list4x_node_is_sorted_with_prev (self, node));
            assert (list4x_node_is_sorted_with_next (self, node));
            node = node->next;
        }
    }
}


void list4x_test (bool verbose) {
    print_info (" * list4x: \n");

    list4x_t *list = list4x_new ();
    assert (list);
    assert (list4x_size (list) == 0);
    assert (list4x_is_sorted (list) == false);
    assert (list4x_is_sorted_ascending (list) == false);
    assert (list4x_is_sorted_descending (list) == false);

    // Operations on empty list
    assert (list4x_first (list) == NULL);
    assert (list4x_last (list) == NULL);
    assert (list4x_pop_first (list) == NULL);
    assert (list4x_pop_last (list) == NULL);

    printf ("\nIterate empty list: \n");
    list4x_iterator_t iter = list4x_iter_init (list, true);
    char *str;
    while ((str = (char *) list4x_iter (list, &iter)) != NULL)
        printf ("%s, ", str);
    printf ("\n");

    list4x_set_destructor (list, (free_func_t) string_free);
    list4x_set_duplicator (list, (duplicate_func_t) string_duplicate);
    list4x_set_comparator (list, (compare_func_t) string_compare);
    list4x_set_printer (list, (print_func_t) string_print);

    assert (list4x_find (list, "hello") == NULL);
    assert (list4x_index (list, "hello") == SIZE_NONE);

    list4x_sort (list, true);
    assert (list4x_size (list) == 0);
    assert (list4x_is_sorted (list));
    assert (list4x_is_sorted_ascending (list));
    assert (list4x_is_sorted_descending (list) == false);
    list4x_sort (list, false);
    assert (list4x_size (list) == 0);
    assert (list4x_is_sorted (list));
    assert (list4x_is_sorted_descending (list));
    assert (list4x_is_sorted_ascending (list) == false);
    list4x_reverse (list);
    assert (list4x_size (list) == 0);
    assert (list4x_is_sorted (list));
    assert (list4x_is_sorted_ascending (list));
    assert (list4x_is_sorted_descending (list) == false);
    rng_t *rng = rng_new ();
    list4x_shuffle (list, rng);
    rng_free (&rng);
    assert (list4x_size (list) == 0);
    assert (list4x_is_sorted (list) == false);
    assert (list4x_is_sorted_ascending (list) == false);
    assert (list4x_is_sorted_descending (list) == false);
    list4x_purge (list);


    //
    list4x_append (list, "world");
    assert (list4x_size (list) == 1);
    assert (streq ((char *) list4x_first (list), "world"));
    assert (streq ((char *) list4x_last (list), "world"));
    assert (streq ((char *) list4x_item_at (list, 0), "world"));
    list4x_print (list);

    list4x_append (list, "hello");
    list4x_print (list);
    assert (list4x_size (list) == 2);
    assert (streq ((char *) list4x_first (list), "world"));
    assert (streq ((char *) list4x_last (list), "hello"));
    assert (streq ((char *) list4x_item_at (list, 0), "world"));
    assert (streq ((char *) list4x_item_at (list, 1), "hello"));
    assert (list4x_index (list, "hello") == 1);
    assert (list4x_index (list, "world") == 0);
    void *h1 = list4x_find (list, "world");
    void *h2 = list4x_find (list, "hello");
    assert (streq ((char *) list4x_item (list, h1), "world"));
    assert (streq ((char *) list4x_item (list, h2), "hello"));

    list4x_sort (list, true);
    list4x_print (list);

    list4x_append (list, "mini");
    list4x_sort (list, false);
    list4x_print (list);
    list4x_sort (list, true);
    list4x_print (list);
    print_debug ("");

    assert (list4x_size (list) == 3);
    assert (list4x_index (list, "hello") == 0);
    assert (list4x_index (list, "world") == 2);
    assert (streq (list4x_item_at (list, 0), "hello"));
    assert (streq (list4x_item_at (list, 1), "mini"));
    list4x_remove_at (list, 0);
    assert (list4x_size (list) == 2);
    list4x_print (list);

    char *string = (char *) list4x_pop_last (list);
    assert (streq (string, "world"));
    free (string);
    assert (list4x_size (list) == 1);

    list4x_purge (list);
    assert (list4x_size (list) == 0);

    // Now populate the list with items
    list4x_prepend (list, "five");
    list4x_append (list, "six");
    list4x_prepend (list, "four");
    list4x_append (list, "seven");
    list4x_prepend (list, "three");
    list4x_append (list, "eight");
    list4x_prepend (list, "two");
    list4x_append (list, "nine");
    list4x_prepend (list, "one");
    list4x_append (list, "ten");
    list4x_print (list);

    // Reverse
    for (size_t i = 0; i < 101; i++)
        list4x_reverse (list);
    print_debug ("after reverse: \n");
    list4x_print (list);

    // Shuffle
    rng = rng_new ();
    for (size_t i = 0; i < 100; i++)
        list4x_shuffle (list, rng);
    print_debug ("after shuffle: \n");
    list4x_print (list);
    rng_free (&rng);


    // Sort by alphabetical order
    list4x_sort (list, true);
    print_debug ("after sort: \n");
    list4x_print (list);
    assert (streq ((char *) list4x_first (list), "eight"));
    assert (streq ((char *) list4x_last (list), "two"));
    list4x_reverse (list);
    list4x_print (list);

    list4x_insert_sorted (list, "eleven");
    list4x_insert_sorted (list, "eleven");
    void *h3 = list4x_insert_sorted (list, "eleven");
    list4x_remove (list, h3);
    list4x_insert_sorted (list, "twelve");
    print_debug ("");
    list4x_print (list);

    list4x_remove_item (list, "eleven");
    list4x_remove_item (list, "six");
    print_debug ("");
    list4x_print (list);

    assert (list4x_includes (list, "nine"));
    assert (list4x_includes (list, "ninty") == false);
    assert (streq ((char *) list4x_item_at (list, list4x_index (list, "three")), "three"));
    assert (streq ((char *) list4x_item (list, list4x_find (list, "one")), "one"));

    // Copy a list
    list4x_t *copy = list4x_duplicate (list);
    assert (copy);
    assert (list4x_equal (list, copy));
    assert (list4x_size (copy) == 10);
    list4x_sort (copy, false);
    list4x_print (copy);
    assert (streq ((char *) list4x_first (copy), "two"));
    assert (streq ((char *) list4x_last (copy), "eight"));
    list4x_free (&copy);

    // Insert to sorted list
    list4x_sort (list, false);
    void *handle = list4x_insert_sorted (list, "fk");
    assert (streq (list4x_item (list, handle), "fk"));
    list4x_print (list);

    // iteration
    iter = list4x_iter_init (list, true);
    while ((str = (char *) list4x_iter (list, &iter)) != NULL)
        printf ("%s, ", str);
    printf ("\n");

    iter = list4x_iter_init (list, false);
    while ((str = (char *) list4x_iter (list, &iter)) != NULL) {
        if (streq (str, "eleven"))
            list4x_iter_remove (list, &iter);
        // printf ("%s, ", str);
    }
    // printf ("\n")

    iter = list4x_iter_init (list, true);
    while ((str = (char *) list4x_iter (list, &iter)) != NULL)
        printf ("%s, ", str);
    printf ("\n");

    list4x_sort (list, false);
    list4x_print (list);
    handle = list4x_find (list, "ten");
    print_debug ("");
    iter = list4x_iter_init_from (list, handle, false);
    while ((str = (char *) list4x_iter (list, &iter)) != NULL)
        printf ("%s, ", str);
    printf ("\n");

    assert (streq (list4x_item (list, handle), "ten"));
    char *item = list4x_item (list, handle);
    item[0] = 'a';
    list4x_print (list);
    list4x_reorder (list, handle);
    assert (streq (list4x_item (list, handle), "aen"));
    list4x_print (list);

    list4x_purge (list);
    list4x_sort (list, false);
    list4x_insert_sorted (list, "mall");
    list4x_insert_sorted (list, "hotel");
    list4x_insert_sorted (list, "zoo");
    list4x_print (list);


    list4x_free (&list);
    print_info ("OK\n");
}
