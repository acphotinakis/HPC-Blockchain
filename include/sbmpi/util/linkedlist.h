#ifndef SBMPI_LINKEDLIST_H
#define SBMPI_LINKEDLIST_H

/**
 * @file linkedlist.h
 * @brief Defines a generic, doubly-linked list data structure.
 *
 * This file provides the interface for a LinkedList class, which is implemented
 * in `src/util/linkedlist.cpp`. This component is a general-purpose utility
 * for managing ordered collections of data where insertions and deletions
 * are frequent.
 */

template <typename T>
class LinkedListNode
{
 public:
  T                  data;
  LinkedListNode<T>* prev;
  LinkedListNode<T>* next;

  LinkedListNode(T data);
};

template <typename T>
class LinkedList
{
 public:
  LinkedList();
  ~LinkedList();

  void append(T data);
  void prepend(T data);
  bool remove(T data);
  void print() const;
  int  size() const;

 private:
  LinkedListNode<T>* head;
  LinkedListNode<T>* tail;
  int                count;
};

#endif  // SBMPI_LINKEDLIST_H