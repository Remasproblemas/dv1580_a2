#define _POSIX_C_SOURCE 200112L // Rwlock will not work without it
#include "memory_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <pthread.h>

pthread_rwlock_t list_rwlock = PTHREAD_RWLOCK_INITIALIZER;

  typedef struct Node {
    uint16_t data; // Stores the data as an unsigned 16-bit integer
    struct Node* next; // A pointer to the next node in the List
  } Node;

#define NODE_SIZE sizeof(Node)

//This function sets up the list and prepares it for operations.
void list_init(Node** head, size_t size) {
    pthread_rwlock_wrlock(&list_rwlock);
    
    mem_init(size);
    if (!head) {
        *head = NULL;
    }
    pthread_rwlock_unlock(&list_rwlock);
}

//Adds a new node with the specified data at end of the linked list. 
void list_insert(Node** head, uint16_t data) {
  pthread_rwlock_wrlock(&list_rwlock);

  Node *new_node = (Node*) mem_alloc(NODE_SIZE); // ALLOCATE MEMORY FOR THE NEW NODE
  
  // If the allocation failed, print a message and exit.
  if (new_node == NULL) {
    printf("Failed to allocate memory for new Node\n");
    pthread_rwlock_unlock(&list_rwlock);
    return;
  }
  new_node->data = data; // Set the data for the NODE
  new_node->next = NULL; // Set Null pointer to indicate end of list

  // If there is no head, set the new node as head.
  if (*head == NULL) {
    *head = new_node;
  } else {
    
    Node *current = *head;
    while (current->next != NULL){ // Find the last Node
      current = current->next;
    }
    current->next = new_node;
  }
  pthread_rwlock_unlock(&list_rwlock);
}

// Inserts a new node with the specified data immediately after a given node.
void list_insert_after(Node* prev_node, uint16_t data){
  pthread_rwlock_wrlock(&list_rwlock);
  Node *new_node = (Node*) mem_alloc(NODE_SIZE); // ALLOCATE MEMORY FOR THE NEW NODE
  
  // If the allocation failed, print a message and exit.
  if (new_node == NULL) {
    printf("Failed to allocate memory for new Node\n");
    pthread_rwlock_unlock(&list_rwlock);
    return;
  }
  new_node->data = data; // Set the data for the node

  // Checks if the previous node is the last one in the list
  if (prev_node->next == NULL) {
    prev_node->next = new_node;
    new_node->next = NULL;

  // If it isn't the last one, set the pointer of the previous node the new node and have the previous node point to the new one. 
  } else {
    new_node->next = prev_node->next;
    prev_node->next = new_node;
  }
  pthread_rwlock_unlock(&list_rwlock);
}

// Inserts a new node with the specified data immediately before a given node in the list. 
void list_insert_before(Node** head, Node* next_node, uint16_t data){
  pthread_rwlock_wrlock(&list_rwlock);
  Node *new_node = (Node*) mem_alloc(NODE_SIZE); // ALLOCATE MEMORY FOR THE NEW NODE
  // If the allocation failed, print a message and exit.
  if (new_node == NULL) {
    printf("Failed to allocate memory for new Node\n");
    pthread_rwlock_unlock(&list_rwlock);
    return;
  }
  new_node->data = data; // Set the data for the node
  
  // If head is the node we want to insert before, set the pointer of new node to next_node and have head point to new_node.
  if (*head == next_node) {
    new_node->next = next_node;
    *head = new_node;

  } else {

    // Traverse the list until next_node is found
    Node *current = *head;
    while (current->next != next_node){ // Find next_node
      current = current->next;
    }

    // Set the pointer of new_node to next node and current node to new node
    new_node->next = next_node;
    current->next = new_node;
  }
  pthread_rwlock_unlock(&list_rwlock);
}

// Removes a node with the specified data from the linked list.
void list_delete(Node** head, uint16_t data){  
  pthread_rwlock_wrlock(&list_rwlock);
  Node *current = *head;
  
  //Checks if head has the correct data and frees it's memory.
  if (current->data == data) {
    Node* node_to_free = current; // Set a pointer to the node we want to free
    *head = current->next; // Point head to the next node
    mem_free(node_to_free);  // Free the memory
  
  } else {

    // Traverse the list until the correct node is found.
    while (current->next->data != data){
      printf("%d\n", current->next->data);
      current = current->next;
    }
    Node* node_to_free = current->next; // Set a pointer to the node we want to free
    current->next = current->next->next; // Set pointer of the current node to the node after the one we want to free
    mem_free(node_to_free); // Free the memory
  }
  pthread_rwlock_unlock(&list_rwlock);
}

// Searches for a node with the specified data and returns a pointer to it.
Node* list_search(Node** head, uint16_t data){
  pthread_rwlock_rdlock(&list_rwlock);

  Node *current = *head;

  // Traverser the list until the correct node is found.
  while (current->data != data){ 
    // If we reach the end and haven't found the node, return NULL
    if (current->next == NULL){
      pthread_rwlock_unlock(&list_rwlock);
      return NULL;
    }
    current = current->next;
  }
  pthread_rwlock_unlock(&list_rwlock);
  return current;
}

// Prints all the elements in the linked list.
void list_display_internal(Node** head){
  Node *current = *head;
  printf("[");

  while (current != NULL)
  {
      printf("%d", current->data);
      current = current->next;
      if (current != NULL){
        printf(", ");
      }
  }

  printf("]");
}

void list_display(Node** head) {
  pthread_rwlock_rdlock(&list_rwlock);
  list_display_internal(head);
  pthread_rwlock_unlock(&list_rwlock);
}

// Prints all elements of the list between two nodes (start_node and end_node).
void list_display_range(Node** head, Node* start_node, Node* end_node){
  pthread_rwlock_rdlock(&list_rwlock);
  if (!start_node && !end_node){
    list_display_internal(head);
  } else {
    
    // Set the starting Node, if start_node argument is NULL set the current node to head.
    Node* current = (start_node != NULL) ? start_node : *head;
    printf("[");

    // Traverser the list and print the data
    while (current != NULL) {
        printf("%d", current->data);

        // As long as we have not reached the end or end_node, print ", "
        if (current->next != NULL && current != end_node) {
            printf(", "); 
        }

        // If we've reached end_node print "]"
        if (current == end_node) {
          printf("]");
          pthread_rwlock_unlock(&list_rwlock);
          return;
        }
        current = current->next;
    }
    if (end_node != NULL) {
        printf("%d", end_node->data);
    }
    printf("]");
    }
  pthread_rwlock_unlock(&list_rwlock);
}

//Will simply return the count of nodes.
int list_count_nodes(Node** head) {
  pthread_rwlock_rdlock(&list_rwlock);
  Node *current = *head;
  
  // Return 0 if there are no nodes in the list.
  if(current == NULL){
    pthread_rwlock_unlock(&list_rwlock);
    return 0;
  }
  
  // If we are here there is at least one node in the list.
  int counter = 1;
  
  // Traverse the list and count the nodes.
  while (current->next != NULL){
    current = current->next;
    counter += 1;
  }
  pthread_rwlock_unlock(&list_rwlock);
  return counter;
}

//Frees all the nodes in the linked list.
void list_cleanup(Node** head) {
  pthread_rwlock_wrlock(&list_rwlock);
  // Set up two pointers, one "normal pointer" and "next_node" to act as a temporary pointer.
  Node *current = *head;
  Node *next_node;

  // Traverse the list and free the memory for each node.
  while (current != NULL){
    next_node = current->next; // Have next_node pointer keep track of the next node
    mem_free(current); // Free the memory
    current = next_node; // "Go to" next node
  }
  *head = NULL;
  mem_deinit(); // Deinitializes the memory pool
  pthread_rwlock_unlock(&list_rwlock);
}