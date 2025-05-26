#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "slab.h"

// Define MP2_MIN_AVAIL_SLAB if not already defined
#ifndef MP2_MIN_AVAIL_SLAB
#define MP2_MIN_AVAIL_SLAB 2
#endif

// Helper function to get pointer to first object in a slab
void *slab_first_object(struct slab *s) {
    // Place objects after slab header plus an additional uint16 for in-use counter
    uint slab_size = sizeof(struct slab) + sizeof(uint16);
    // Align to pointer boundary for better performance
    return (void*)ROUNDUP((uint64)s + slab_size, sizeof(void*));
}

// Get the in-use counter from a slab
uint16 get_slab_in_use(struct slab *s) {
    uint16 *counter = (uint16*)((char*)s + sizeof(struct slab));
    return *counter;
}

// Set the in-use counter for a slab
void set_slab_in_use(struct slab *s, uint16 count) {
    uint16 *counter = (uint16*)((char*)s + sizeof(struct slab));
    *counter = count;
}

// Helper function to get next entry or null
static inline struct slab* list_next_entry_or_null(struct slab *entry, struct list_head *head) {
    if (entry->list.next == head)
        return 0;
    return list_entry(entry->list.next, struct slab, list);
}

struct kmem_cache *kmem_cache_create(char *name, uint object_size) {
    struct kmem_cache *cache = (struct kmem_cache*)kalloc();
    if(!cache) return 0;
    
    memset(cache, 0, sizeof(*cache));
    strncpy(cache->name, name, MP2_CACHE_MAX_NAME-1);
    cache->object_size = object_size;
    initlock(&cache->lock, name);
    
    // Initialize list heads
    INIT_LIST_HEAD(&cache->partial);
    INIT_LIST_HEAD(&cache->full);
    INIT_LIST_HEAD(&cache->free);
    
    // Make sure object_size is at least the size of a pointer for freelist management
    if (object_size < sizeof(struct run*)) {
        object_size = sizeof(struct run*);
    }
    
    // Align object size to ensure proper alignment
    uint aligned_obj_size = ROUNDUP(object_size, sizeof(void*));
    
    // Calculate max objects using aligned size - account for in_use storage
    uint slab_overhead = sizeof(struct slab) + sizeof(uint16); // Add space for in_use counter
    slab_overhead = ROUNDUP(slab_overhead, sizeof(void*));     // Align to pointer size
    
    cache->max_objects = (PGSIZE - slab_overhead) / aligned_obj_size;
    
    // Ensure we have room for at least one object
    if (cache->max_objects == 0) {
        kfree(cache);
        return 0;
    }
    
    // Calculate in-cache objects with alignment
    uint cache_overhead = sizeof(struct kmem_cache);
    uint aligned_start = ROUNDUP(cache_overhead, sizeof(void*));
    cache->in_cache_obj = (PGSIZE - aligned_start) / aligned_obj_size;
    
    // Setup in-cache objects if possible
    if(cache->in_cache_obj > 0) {
        // First, zero the entire cache area
        char *start = (char*)cache + aligned_start;
        memset(start, 0, cache->in_cache_obj * aligned_obj_size);
        
        // Then initialize the freelist links
        struct run *r = (struct run*)start;
        cache->cache_freelist = r;
        
        for(int i = 0; i < cache->in_cache_obj - 1; i++) {
            r->next = (struct run*)(start + (i+1) * aligned_obj_size);
            r = r->next;
        }
        r->next = 0;
    }

    printf("[SLAB] New kmem_cache (name: %s, object size: %d bytes, at: %p, max objects per slab: %d, support in cache obj: %d) is created\n", 
           cache->name, cache->object_size, cache, cache->max_objects, cache->in_cache_obj);
           
    return cache;
}

void *kmem_cache_alloc(struct kmem_cache *cache) {
    if(!cache) return 0;

    acquire(&cache->lock);
    printf("[SLAB] Alloc request on cache %s\n", cache->name);

    void *obj = 0;
    
    // Try in-cache allocation first
    if(cache->cache_freelist) {
        struct run *r = cache->cache_freelist;
        cache->cache_freelist = r->next;
        obj = r;
        goto found;
    }
    
    // Try finding a slab with free objects
    struct slab *s = NULL;
    if (!list_empty(&cache->partial)) {
        s = list_first_entry(&cache->partial, struct slab, list);
    } else if (!list_empty(&cache->free)) {
        s = list_first_entry(&cache->free, struct slab, list);
        list_del(&s->list); // Remove from free list
        list_add(&s->list, &cache->partial); // Add to partial list
    } else {
        // Need new slab
        s = (struct slab*)kalloc();
        if(!s) {
            release(&cache->lock);
            return 0;
        }
        
        // Always zero the ENTIRE page first for clean initialization
        memset(s, 0, PGSIZE);
        INIT_LIST_HEAD(&s->list);
        set_slab_in_use(s, 0);
        
        // Use aligned object size for consistent memory layout
        uint aligned_obj_size = ROUNDUP(cache->object_size, sizeof(void*));
        
        // Initialize freelist with proper alignment
        char *start = (char*)slab_first_object(s);
        struct run *r = (struct run*)start;
        s->freelist = r;
        
        // Clear memory before setting up links
        memset(start, 0, cache->max_objects * aligned_obj_size);
        
        // Set up the freelist links
        for(int i = 0; i < cache->max_objects - 1; i++) {
            r->next = (struct run*)(start + (i+1) * aligned_obj_size);
            r = r->next;
        }
        r->next = 0;
        
        list_add(&s->list, &cache->partial);
        cache->total_slabs++;
        printf("[SLAB] A new slab %p (%s) is allocated\n", s, cache->name);
    }
    
    // Get object from slab's freelist
    struct run *r = s->freelist;
    s->freelist = r->next;
    obj = r;
    set_slab_in_use(s, get_slab_in_use(s) + 1); // Increment in_use counter
    
    // Move to full list if needed
    if(!s->freelist) {
        list_move(&s->list, &cache->full);
    }

found:
    if(obj) {
        // Clear the memory EXCEPT the first word which might be part of freelist
        // Clear first word separately (since we've already extracted next pointer)
        *(void**)obj = 0;
        
        // Clear the rest of the object
        if(cache->object_size > sizeof(void*)) {
            memset((char*)obj + sizeof(void*), 0, cache->object_size - sizeof(void*));
        }
        
        uint64 slab_addr = (uint64)obj & ~(uint64)(PGSIZE-1);
        printf("[SLAB] Object %p in slab %p (%s) is allocated and initialized\n", 
               obj, (void*)slab_addr, cache->name);
    }
    
    release(&cache->lock);
    return obj;
}

void kmem_cache_free(struct kmem_cache *cache, void *obj) {
    if(!cache || !obj) return;

    static const char* STATE_FULL = "full";
    static const char* STATE_PARTIAL = "partial";
    static const char* STATE_FREE = "free";
    static const char* STATE_CACHE = "cache";

    acquire(&cache->lock);
    
    uint64 obj_addr = (uint64)obj;
    uint64 cache_addr = (uint64)cache;
    uint aligned_start = ROUNDUP(sizeof(struct kmem_cache), sizeof(void*));
    
    // Check if object is from in-cache allocation with more precise bounds
    if(obj_addr >= cache_addr + aligned_start && obj_addr < cache_addr + PGSIZE) {
        printf("[SLAB] Free %p in slab %p (%s)\n", obj, cache, cache->name);
        
        // First clear object memory except for first word (which will store next ptr)
        if(cache->object_size > sizeof(void*)) {
            memset((char*)obj + sizeof(void*), 0, cache->object_size - sizeof(void*));
        }
        
        // Then set up the freelist pointer safely
        struct run *r = (struct run*)obj;
        r->next = cache->cache_freelist;
        cache->cache_freelist = r;
        
        printf("[SLAB] State transition: %s -> %s\n", STATE_CACHE, STATE_CACHE);
        printf("[SLAB] End of free\n");
        release(&cache->lock);
        return;
    }
    
    // Get slab from object address
    uint64 slab_addr = obj_addr & ~(uint64)(PGSIZE-1);
    struct slab *s = (struct slab*)slab_addr;
    
    printf("[SLAB] Free %p in slab %p (%s)\n", obj, s, cache->name);
    
    // Get current slab state
    const char *before_state = (s->freelist == 0) ? STATE_FULL : 
                              (get_slab_in_use(s) > 0) ? STATE_PARTIAL : STATE_FREE;
    
    // Add object back to freelist
    struct run *r = (struct run*)obj;
    
    // Handle regular object free - first clear memory except first word
    if(cache->object_size > sizeof(void*)) {
        memset((char*)obj + sizeof(void*), 0, cache->object_size - sizeof(void*));
    }
    
    // Then set up the freelist pointer safely
    r->next = s->freelist;
    s->freelist = r;
    set_slab_in_use(s, get_slab_in_use(s) - 1); // Decrement in_use counter
    
    // Handle transitions
    if (get_slab_in_use(s) == 0) {
        // Count all available slabs (including this one)
        int available_slabs = 1;  // Start with 1 for current slab
        struct slab *tmp;
        
        // Count partial slabs
        list_for_each_entry(tmp, &cache->partial, list) {
            available_slabs++;
        }
        
        // Count free slabs
        list_for_each_entry(tmp, &cache->free, list) {
            available_slabs++;
        }
        
        // Remove from current list
        list_del(&s->list);
        
        // Free slab if we have enough available slabs
        if (available_slabs > MP2_MIN_AVAIL_SLAB) {
            printf("[SLAB] slab %p (%s) is freed due to save memory\n", s, cache->name);
            cache->total_slabs--;
            kfree(s);
            printf("[SLAB] State transition: %s -> freed\n", before_state);
        } else {
            list_add(&s->list, &cache->free);
            printf("[SLAB] State transition: %s -> %s\n", before_state, STATE_FREE);
        }
    } else if (s->freelist) {
        if (before_state == STATE_FULL) {
            list_move(&s->list, &cache->partial);
            printf("[SLAB] State transition: %s -> %s\n", before_state, STATE_PARTIAL);
        }
    }

    printf("[SLAB] End of free\n");
    release(&cache->lock);
}

void print_kmem_cache(struct kmem_cache *cache, void (*print_fn)(void *)) {
    if(!cache) return;
    
    acquire(&cache->lock);
    
    printf("[SLAB] kmem_cache { name: %s, object_size: %d, at: %p, in_cache_obj: %d }\n",
           cache->name, cache->object_size, cache, cache->in_cache_obj);
    
    // Print in-cache objects if any
    if(cache->in_cache_obj > 0) {
        printf("[SLAB]    [ cache    slabs ]\n");
        printf("[SLAB]        [ slab %p ] { freelist: %p, nxt: 0x0 }\n", 
               cache, cache->cache_freelist);
               
        if(print_fn) {
            uint aligned_start = ROUNDUP(sizeof(struct kmem_cache), sizeof(void*));
            uint aligned_obj_size = ROUNDUP(cache->object_size, sizeof(void*));
            char *start = (char*)cache + aligned_start;
            
            for(int i = 0; i < cache->in_cache_obj; i++) {
                void *obj = start + i * aligned_obj_size;
                void *as_ptr = *(void**)obj;  // Safely read the first pointer-sized value
                // Print on single line, not with newline after "as_obj: {"
                printf("[SLAB]           [ idx %d ] { addr: %p, as_ptr: %p, as_obj: {", 
                       i, obj, as_ptr);
                if(print_fn) {
                    print_fn(obj);
                }
                printf("} }\n");
            }
        }
    }
    
    // Print partial slabs
    if(!list_empty(&cache->partial)) {
        printf("[SLAB]    [ partial    slabs ]\n");
        struct slab *s;
        list_for_each_entry(s, &cache->partial, list) {
            printf("[SLAB]        [ slab %p ] { freelist: %p, in_use: %d, nxt: %p }\n", 
                   s, s->freelist, get_slab_in_use(s), list_next_entry_or_null(s, &cache->partial));
            if(print_fn) {
                uint aligned_obj_size = ROUNDUP(cache->object_size, sizeof(void*));
                char *start = (char*)slab_first_object(s);
                
                for(int i = 0; i < cache->max_objects; i++) {
                    void *obj = start + i * aligned_obj_size;
                    void *as_ptr = *(void**)obj;  // Safely read the first pointer-sized value
                    printf("[SLAB]           [ idx %d ] { addr: %p, as_ptr: %p, as_obj: {", 
                           i, obj, as_ptr);
                    print_fn(obj);
                    printf("} }\n");
                }
            }
        }
    }
    
    // Print full slabs
    if(!list_empty(&cache->full)) {
        printf("[SLAB]    [ full    slabs ]\n");
        struct slab *s;
        list_for_each_entry(s, &cache->full, list) {
            printf("[SLAB]        [ slab %p ] { freelist: %p, in_use: %d, nxt: %p }\n", 
                   s, s->freelist, get_slab_in_use(s), list_next_entry_or_null(s, &cache->full));
            if(print_fn) {
                uint aligned_obj_size = ROUNDUP(cache->object_size, sizeof(void*));
                char *start = (char*)slab_first_object(s);
                
                for(int i = 0; i < cache->max_objects; i++) {
                    void *obj = start + i * aligned_obj_size;
                    void *as_ptr = *(void**)obj;  // Safely read the first pointer-sized value
                    printf("[SLAB]           [ idx %d ] { addr: %p, as_ptr: %p, as_obj: {", 
                           i, obj, as_ptr);
                    print_fn(obj);
                    printf("} }\n");
                }
            }
        }
    }
    
    // Print free slabs
    if(!list_empty(&cache->free)) {
        printf("[SLAB]    [ free    slabs ]\n");
        struct slab *s;
        list_for_each_entry(s, &cache->free, list) {
            printf("[SLAB]        [ slab %p ] { freelist: %p, in_use: %d, nxt: %p }\n", 
                   s, s->freelist, get_slab_in_use(s), list_next_entry_or_null(s, &cache->free));
            
            if(print_fn) {
                uint aligned_obj_size = ROUNDUP(cache->object_size, sizeof(void*));
                char *start = (char*)slab_first_object(s);
                
                for(int i = 0; i < cache->max_objects; i++) {
                    void *obj = start + i * aligned_obj_size;
                    void *as_ptr = *(void**)obj;  // Safely read the first pointer-sized value
                    printf("[SLAB]           [ idx %d ] { addr: %p, as_ptr: %p, as_obj: {", 
                           i, obj, as_ptr);
                    print_fn(obj);
                    printf("} }\n");
                }
            }
        }
    }

    printf("[SLAB] print_kmem_cache end\n");
    release(&cache->lock);
}

void kmem_cache_destroy(struct kmem_cache *cache) {
    if(!cache) return;
    
    acquire(&cache->lock);
    
    // Free all slabs
    struct slab *s, *tmp;
    list_for_each_entry_safe(s, tmp, &cache->full, list) {
        list_del(&s->list);
        kfree(s);
    }
    list_for_each_entry_safe(s, tmp, &cache->partial, list) {
        list_del(&s->list);
        kfree(s);
    }
    list_for_each_entry_safe(s, tmp, &cache->free, list) {
        list_del(&s->list);
        kfree(s);
    }
    
    // Free cache itself
    release(&cache->lock);
    kfree(cache);
}
