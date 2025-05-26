#pragma once

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "list.h"

// #define MP2_CACHE_MAX_NAME 32

// Define ROUNDUP for proper memory alignment
#define ROUNDUP(a, b) ((((a) + (b) - 1) / (b)) * (b))

// Simple struct for freelist management
struct run {
  struct run *next;
};

// Optimized to exactly 3 pointers in size - no more, no less
struct slab {
  struct run *freelist;    // First pointer for freelist management
  struct list_head list;   // Second and third pointers for list management (next/prev)
  // The in_use counter is stored right after the struct in memory
};

struct kmem_cache {
  char name[MP2_CACHE_MAX_NAME];
  uint object_size;
  struct spinlock lock;
  
  struct list_head partial; // partially used slabs
  struct list_head full;    // fully used slabs
  struct list_head free;    // completely free slabs
  
  // For tracking objects per slab
  uint max_objects;
  uint total_slabs;

  // For in-cache objects (bonus)
  int in_cache_obj;
  struct run *cache_freelist;
  char cache_area[0];      // Flexible array member for in-cache objects
};

// Initialize a slab allocator, creating a kmem_cache for system objects
struct kmem_cache *kmem_cache_create(char *name, uint object_size);

// Allocate a system object and return its memory address
void *kmem_cache_alloc(struct kmem_cache *cache);

// Free the specified system object "obj"
void kmem_cache_free(struct kmem_cache *cache, void *obj);

// Destroy the kmem_cache
void kmem_cache_destroy(struct kmem_cache *cache);

// Print kmem_cache information
void print_kmem_cache(struct kmem_cache *cache, void (*slab_obj_printer)(void *));

// Helper functions
void *slab_first_object(struct slab *s);
uint16 get_slab_in_use(struct slab *s);
void set_slab_in_use(struct slab *s, uint16 count);
