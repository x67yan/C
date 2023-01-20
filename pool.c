// "pool.c" [IMPLEMENTATION]

// see "pool.h" for details

#include "pool.h"
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

struct llnode{
  int s;
  char *start;
  struct llnode *prev;
  struct llnode *next;
};

struct pool {
  char *h;
  struct llnode *active;
  struct llnode *available;
};


// create_node(size, begin) creates a node with size been size and start been 
//   begin
// requires: begin must be valid and size > 0
// effect: allocate memory(caller must free)
// time: O(1)
static struct llnode *create_node(int size, char* begin) {
  assert(size > 0);
  assert(begin);
  struct llnode *node = malloc(sizeof(struct llnode));
  node->s = size;
  node->start = begin;
  node->prev = NULL;
  node->next = NULL;
  return node;
}


struct pool *pool_create(int size){
  assert(size > 0);
  struct pool *p = malloc(sizeof(struct pool));
  p->h = malloc(size * sizeof(char));
  p->active = NULL;
  p->available = create_node(size, p->h);
  return p;
}


bool pool_destroy(struct pool *p){
  assert(p);
  if (p->active) {
    return false;
  } else {
    free(p->h);
    free(p->available);
    free(p);
    return true;
  }
}


// add_active(node, p) add nodes to p->active
// requires: node and p must be valid
// effect: modifies p and node
// time: O(1)
static void add_active(struct llnode *node, struct pool *p) {
  assert(node);
  assert(p);
  struct llnode *curr_node = p->active;
  if (curr_node) {
    while (curr_node->start < node->start && curr_node->next) {
      struct llnode *next_node = curr_node->next;
      curr_node = next_node;
    }
    struct llnode *pre_node = curr_node->prev;
    if (curr_node->start > node->start) {
      if (pre_node == NULL) {
        p->active = node;
      } else {
        pre_node->next = node;
      }
      node->prev = pre_node;
      node->next = curr_node;
      curr_node->prev = node;
    } else {
      curr_node->next = node;
      node->prev = curr_node;
      node->next = NULL;
    }
  } else {
    node->prev = NULL;
    node->next = NULL;
    p->active = node;
  }
}


// same_size_available(node, p) take node away from p->available
// requires: node and p must be valid
// effect: modifies p
// time: O(1)
static void same_size_available(struct llnode *node,  struct pool *p){
  assert(p);
  assert(node);
  struct llnode *pre_node = node->prev;
  struct llnode *next_node = node->next;
  if (pre_node == NULL && next_node == NULL) {
    p->available = NULL;
  } else if (next_node == NULL) {
    pre_node->next = NULL;
  } else if (pre_node == NULL) {
    next_node->prev = NULL;
    p->available = next_node;
  } else {
    pre_node->next = next_node;
    next_node->prev = pre_node;
  }
}


// more_size_available(node, p, size) takes part of menmory referenced by node
//   away from p->available and the size of this part of memory is size
// requires: size > 0 and node, p must be valid
// effect: modifies p and node
static void more_size_available(struct llnode *node, struct pool *p, int size){
  assert(node);
  assert(p);
  assert(size > 0);
  int node_size = node->s;
  struct llnode *next_node = node->next;
  struct llnode *pre_node = node->prev;
  char *new_start = node->start + size;
  struct llnode *new_node = create_node(node_size - size, new_start);
  if (pre_node == NULL) {
    p->available = new_node;
    new_node->prev = NULL;
  } else if (pre_node != NULL) {
    pre_node->next = new_node;
    new_node->prev = pre_node;
  }
  if (next_node == NULL) {
    new_node->next = next_node;
  } else if (next_node != NULL) {
    new_node->next = next_node;
    next_node->prev = new_node;
  }
  node->s = size;
}


char *pool_alloc(struct pool *p, int size){
  assert(p);
  assert(size > 0);
  if (p->available) {
    struct llnode *node = p->available;
    while (node) {
      int node_size = node->s;
      struct llnode *next_node = node->next;
      if (node_size < size) {
        node = next_node;
        continue;
      } else if (node_size == size) {
        same_size_available(node, p);
      } else if (node_size > size) {
        more_size_available(node, p, size);
      }
      add_active(node, p);
      return node->start;
    }
  }
  return NULL;
}


// add_available(node, p) add node back to p->available
// requires: node and p must be valid
// effect: modifies p and node
// time: O(n), n is the number of arrays in available part of p
static void add_available(struct llnode *node, struct pool *p){
  assert(node);
  assert(p);
  if (p->available) {
    struct llnode *curr_node = p->available;
    while(1){
      struct llnode *next_node = curr_node->next;
      if (curr_node->start > node->start) break;
      if (next_node == NULL) break;
      curr_node = next_node;
    }
    struct llnode *pre_node = NULL;
    if (curr_node->start > node->start) {
      pre_node = curr_node->prev;
    } else {
      pre_node = curr_node;
      curr_node = NULL;
    }
    struct llnode *new_prev = NULL;
    if (pre_node == NULL) {
      new_prev = node;
      p->available = new_prev;
    } else if (pre_node->start + pre_node->s == node->start) {
      pre_node->s += node->s;
      free(node);
      new_prev = pre_node;
    } else {
      pre_node->next = node;
      node->prev = pre_node;
      new_prev = node;
    }
    if (curr_node == NULL) {
      new_prev->next = NULL;
    } else if (new_prev->start + new_prev->s == curr_node->start) {
      new_prev->s += curr_node->s;
      new_prev->next = curr_node->next;
      if (curr_node->next != NULL) {
        curr_node->next->prev = new_prev;
      }
      free(curr_node);
    } else {
      new_prev->next = curr_node;
      curr_node->prev = new_prev;
    }
  } else {
    node->next = NULL;
    node->prev = NULL;
    p->available = node;
  }
}


bool pool_free(struct pool *p, char *addr){
  assert(p);
  struct llnode *node = p->active;
  while (node) {
    if (node->start == addr) {
      struct llnode *pre_node = node->prev;
      struct llnode *next_node = node->next;
      if (pre_node == NULL && next_node == NULL) {
        p->active = NULL;
      } else if (next_node == NULL) {
        pre_node->next = NULL;
      } else if (pre_node == NULL) {
        p->active = next_node;
        next_node->prev = NULL;
      } else {
        pre_node->next = next_node;
        next_node->prev = pre_node;
      }
      add_available(node, p);
      return true;
    }
    struct llnode *next_node = node->next;
    node = next_node;
  }
  return false;
}


// find_active(addr, p) returns a node with addr inside its field if addr is
//   actiivated in p; otherwise, it returns NULL
// requires: p must be valid
// time: O(n)
static struct llnode *find_active(char *addr, struct pool *p){
  assert(p);
  struct llnode *node = p->active;
  while (node){
    if (node->start == addr) {
      return node;
    } 
    struct llnode *next_node = node->next;
    node = next_node;
  }
  return NULL;
}


// find_available(addr, size, p) returns a node with start been addr and have
//   size at least size, if such node exists; otherwise, it returns NULL
// requires: p must be valid
//           size > 0
// time: O(n)
static struct llnode *find_available(char *addr, int size, struct pool *p) {
  assert(size > 0);
  assert(p);
  struct llnode *node = p->available;
  while(node && node->start <= addr){
    if (node->start == addr) {
      if(node->s < size) return NULL;
      if(node->s == size) {
        same_size_available(node, p);
      } else {
        more_size_available(node, p, size);
      }
      return node;
    }
    struct llnode *next_node = node->next;
    node = next_node;
  }
  return NULL; 
}


char *pool_realloc(struct pool *p, char *addr, int size){
  assert(p);
  assert(size > 0);
  struct llnode *node = find_active(addr, p);
  if (node == NULL) return NULL;
  if (node->s == size) {
    return node->start;
  } else if (node->s > size) {
    struct llnode *new_available = create_node(node->s - size, 
                                               node->start + size);
    add_available(new_available, p);
    node->s = size;
    return node->start;
  } else {
    char *extra = addr + node->s;
    struct llnode *new_active = find_available(extra, size - node->s, p);
    if (new_active == NULL) {
      char *my_start = pool_alloc(p, size);
      if (my_start != NULL) {
        for(int i = 0; i < node->s; ++i){
          my_start[i] = node->start[i];
        }
        pool_free(p, addr);
        return my_start;
      } else {
        return NULL;
      }
    } else {
      free(new_active);
      node->s = size;
      return node->start;
    }
  }
}


void pool_print_active(const struct pool *p){
  assert(p);
  if (p->active) {
    int idx = 0;
    bool is_first = true;
    struct llnode *node = p->active;
    while (node) {
      idx = node->start - p->h;
      struct llnode *next_node = node->next;
      if (is_first) {
        printf("active: %d [%d]", idx, node->s);
        is_first = false;
      } else {
        printf(", %d [%d]", idx, node->s);
      }
      node = next_node;
    }
    printf("\n");
  } else {
    printf("active: none\n");
  }
}


void pool_print_available(const struct pool *p){
  assert(p);
  if (p->available) {
    int idx = 0;
    bool is_first = true;
    struct llnode *node = p->available;
    while (node) {
      idx = node->start - p->h;
      struct llnode *next_node = node->next;
      if (is_first) {
        printf("available: %d [%d]", idx, node->s);
        is_first = false;
      } else {
        printf(", %d [%d]", idx, node->s);
      }
      node = next_node;
    }
    printf("\n");
  } else {
    printf("available: none\n");
  }
}
