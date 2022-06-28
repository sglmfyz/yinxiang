/*
 * Copyright (c) 2004, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

/**
 * \file
 * Linked list library implementation.
 *
 * \author Adam Dunkels <adam@sics.se>
 *
 */

/**
 * \addtogroup list
 * @{
 */

#include "slist.h"

#define NULL 0

struct slist {
  struct slist *next;
};

/*---------------------------------------------------------------------------*/
/**
 * Initialize a list.
 *
 * This function initalizes a list. The list will be empty after this
 * function has been called.
 *
 * \param list The list to be initialized.
 */
void
slist_init(slist_t list)
{
  *list = NULL;
}
/*---------------------------------------------------------------------------*/
/**
 * Get a pointer to the first element of a list.
 *
 * This function returns a pointer to the first element of the
 * list. The element will \b not be removed from the list.
 *
 * \param list The list.
 * \return A pointer to the first element on the list.
 *
 * \sa list_tail()
 */
void *
slist_head(slist_t list)
{
  return *list;
}
/*---------------------------------------------------------------------------*/
/**
 * Duplicate a list.
 *
 * This function duplicates a list by copying the list reference, but
 * not the elements.
 *
 * \note This function does \b not copy the elements of the list, but
 * merely duplicates the pointer to the first element of the list.
 *
 * \param dest The destination list.
 * \param src The source list.
 */
void
slist_copy(slist_t dest, slist_t src)
{
  *dest = *src;
}
/*---------------------------------------------------------------------------*/
/**
 * Get the tail of a list.
 *
 * This function returns a pointer to the elements following the first
 * element of a list. No elements are removed by this function.
 *
 * \param list The list
 * \return A pointer to the element after the first element on the list.
 *
 * \sa list_head()
 */
void *
slist_tail(slist_t list)
{
  struct slist *l;
  
  if(*list == NULL) {
    return NULL;
  }
  
  for(l = *list; l->next != NULL; l = l->next);
  
  return l;
}
/*---------------------------------------------------------------------------*/
/**
 * Add an item at the end of a list.
 *
 * This function adds an item to the end of the list.
 *
 * \param list The list.
 * \param item A pointer to the item to be added.
 *
 * \sa list_push()
 *
 */
void
slist_add(slist_t list, void *item)
{
  struct slist *l;

  /* Make sure not to add the same element twice */
  slist_remove(list, item);

  ((struct slist *)item)->next = NULL;
  
  l = slist_tail(list);

  if(l == NULL) {
    *list = item;
  } else {
    l->next = item;
  }
}
/*---------------------------------------------------------------------------*/
/**
 * Add an item to the start of the list.
 */
void
slist_push(slist_t list, void *item)
{
  /*  struct list *l;*/

  /* Make sure not to add the same element twice */
  slist_remove(list, item);

  ((struct slist *)item)->next = *list;
  *list = item;
}
/*---------------------------------------------------------------------------*/
/**
 * Remove the last object on the list.
 *
 * This function removes the last object on the list and returns it.
 *
 * \param list The list
 * \return The removed object
 *
 */
void *
slist_chop(slist_t list)
{
  struct slist *l, *r;
  
  if(*list == NULL) {
    return NULL;
  }
  if(((struct slist *)*list)->next == NULL) {
    l = *list;
    *list = NULL;
    return l;
  }
  
  for(l = *list; l->next->next != NULL; l = l->next);

  r = l->next;
  l->next = NULL;
  
  return r;
}
/*---------------------------------------------------------------------------*/
/**
 * Remove the first object on a list.
 *
 * This function removes the first object on the list and returns a
 * pointer to it.
 *
 * \param list The list.
 * \return Pointer to the removed element of list.
 */
/*---------------------------------------------------------------------------*/
void *
slist_pop(slist_t list)
{
  struct slist *l;
  l = *list;
  if(*list != NULL) {
    *list = ((struct slist *)*list)->next;
  }

  return l;
}
/*---------------------------------------------------------------------------*/
/**
 * Remove a specific element from a list.
 *
 * This function removes a specified element from the list.
 *
 * \param list The list.
 * \param item The item that is to be removed from the list.
 *
 */
/*---------------------------------------------------------------------------*/
void
slist_remove(slist_t list, void *item)
{
  struct slist *l, *r;
  
  if(*list == NULL) {
    return;
  }
  
  r = NULL;
  for(l = *list; l != NULL; l = l->next) {
    if(l == item) {
      if(r == NULL) {
	/* First on list */
	*list = l->next;
      } else {
	/* Not first on list */
	r->next = l->next;
      }
      l->next = NULL;
      return;
    }
    r = l;
  }
}
/*---------------------------------------------------------------------------*/
/**
 * Get the length of a list.
 *
 * This function counts the number of elements on a specified list.
 *
 * \param list The list.
 * \return The length of the list.
 */
/*---------------------------------------------------------------------------*/
int
slist_length(slist_t list)
{
  struct slist *l;
  int n = 0;

  for(l = *list; l != NULL; l = l->next) {
    ++n;
  }

  return n;
}
/*---------------------------------------------------------------------------*/
/**
 * \brief      Insert an item after a specified item on the list
 * \param list The list
 * \param previtem The item after which the new item should be inserted
 * \param newitem  The new item that is to be inserted
 * \author     Adam Dunkels
 *
 *             This function inserts an item right after a specified
 *             item on the list. This function is useful when using
 *             the list module to ordered lists.
 *
 *             If previtem is NULL, the new item is placed at the
 *             start of the list.
 *
 */
void
slist_insert(slist_t list, void *previtem, void *newitem)
{
  if(previtem == NULL) {
    slist_push(list, newitem);
  } else {
  
    ((struct slist *)newitem)->next = ((struct slist *)previtem)->next;
    ((struct slist *)previtem)->next = newitem;
  }
}
/*---------------------------------------------------------------------------*/
/**
 * \brief      Get the next item following this item
 * \param item A list item
 * \returns    A next item on the list
 *
 *             This function takes a list item and returns the next
 *             item on the list, or NULL if there are no more items on
 *             the list. This function is used when iterating through
 *             lists.
 */
void *
slist_item_next(void *item)
{
  return item == NULL? NULL: ((struct slist *)item)->next;
}
/*---------------------------------------------------------------------------*/
/** @} */
