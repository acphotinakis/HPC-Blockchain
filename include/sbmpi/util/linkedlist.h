#ifndef SBMPI_LINKEDLIST_H
#define SBMPI_LINKEDLIST_H

#include <iostream>

namespace sbmpi {
namespace util {

template <typename T>
class LinkedListNode {
 public:
  T data;
  LinkedListNode<T>* prev;
  LinkedListNode<T>* next;

  LinkedListNode(T data) : data(data), prev(nullptr), next(nullptr) {}
};

template <typename T>
class LinkedList {
 public:
  LinkedList() : head(nullptr), tail(nullptr), count(0) {}
  ~LinkedList() {
    LinkedListNode<T>* current = head;
    while (current) {
      LinkedListNode<T>* next = current->next;
      delete current;
      current = next;
    }
  }

  void append(T data) {
    LinkedListNode<T>* newNode = new LinkedListNode<T>(data);
    if (!tail) {
      head = tail = newNode;
    } else {
      tail->next = newNode;
      newNode->prev = tail;
      tail = newNode;
    }
    count++;
  }

  void prepend(T data) {
    LinkedListNode<T>* newNode = new LinkedListNode<T>(data);
    if (!head) {
      head = tail = newNode;
    } else {
      head->prev = newNode;
      newNode->next = head;
      head = newNode;
    }
    count++;
  }

  bool remove(T data) {
    LinkedListNode<T>* current = head;
    while (current) {
      if (current->data == data) {
        if (current->prev) {
          current->prev->next = current->next;
        } else {
          head = current->next;
        }
        if (current->next) {
          current->next->prev = current->prev;
        } else {
          tail = current->prev;
        }
        delete current;
        count--;
        return true;
      }
      current = current->next;
    }
    return false;
  }

  void print() const {
    LinkedListNode<T>* current = head;
    while (current) {
      std::cout << current->data << " ";
      current = current->next;
    }
    std::cout << std::endl;
  }

  int size() const { return count; }

 private:
  LinkedListNode<T>* head;
  LinkedListNode<T>* tail;
  int count;
};

}  // namespace util
}  // namespace sbmpi

#endif  // SBMPI_LINKEDLIST_H