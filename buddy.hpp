// Copyright (C) 2021  ilobilo

#pragma once

#include <iostream>
#include <cassert>
#include <cstring>
#include <cmath>

#define DIV_ROUNDUP(A, B) \
({ \
    typeof(A) _a_ = A; \
    typeof(B) _b_ = B; \
    (_a_ + (_b_ - 1)) / _b_; \
})

#define ALIGN_UP(A, B) \
({ \
    typeof(A) _a__ = A; \
    typeof(B) _b__ = B; \
    DIV_ROUNDUP(_a__, _b__) * _b__; \
})

#define POWER_OF_2(var) (!((var) & ((var) - 1)))

struct BuddyBlock
{
    size_t size;
    bool free;
};

class BuddyAlloc
{
    public:
    BuddyBlock *head;
    BuddyBlock *tail;
    void *data = nullptr;

    volatile bool expanded = false;

    BuddyBlock *next(BuddyBlock *block)
    {
        return (BuddyBlock*)((char*)block + block->size);
    }

    BuddyBlock *split(BuddyBlock *block, size_t size)
    {
        if (block != NULL && size != 0)
        {
            while (size < block->size)
            {
                size_t sz = block->size >> 1;
                block->size = sz;
                block = this->next(block);
                block->size = sz;
                block->free = true;
            }
            if (size <= block->size) return block;
        }
        return NULL;
    }

    BuddyBlock *find_best(size_t size)
    {
        if (size == 0) return NULL;

        BuddyBlock *best_block = NULL;
        BuddyBlock *block = this->head;
        BuddyBlock *buddy = this->next(block);

        if (buddy == this->tail && block->free) return this->split(block, size);

        while (block < this->tail && buddy < this->tail)
        {
            if (block->free && buddy->free && block->size == buddy->size)
            {
                block->size <<= 1;
                if (size <= block->size && (best_block == NULL || block->size <= best_block->size)) best_block = block;

                block = this->next(buddy);
                if (block < this->tail) buddy = this->next(block);
                continue;
            }

            if (block->free && size <= block->size && (best_block == NULL || block->size <= best_block->size)) best_block = block;
            if (buddy->free && size <= buddy->size && (best_block == NULL || buddy->size < best_block->size)) best_block = buddy;

            if (block->size <= buddy->size)
            {
                block = this->next(buddy);
                if (block < this->tail) buddy = this->next(block);
            }
            else
            {
                block = buddy;
                buddy = this->next(buddy);
            }
        }

        if (best_block != NULL) return this->split(best_block, size);

        return NULL;
    }

    size_t required_size(size_t size)
    {
        size_t actual_size = sizeof(BuddyBlock);

        size += sizeof(BuddyBlock);
        size = ALIGN_UP(size, sizeof(BuddyBlock));

        while (size > actual_size) actual_size <<= 1;

        return actual_size;
    }

    void coalescence()
    {
        while (true)
        {
            BuddyBlock *block = this->head;
            BuddyBlock *buddy = this->next(block);

            bool no_coalescence = true;
            while (block < this->tail && buddy < this->tail)
            {
                if (block->free && buddy->free && block->size == buddy->size)
                {
                    block->size <<= 1;
                    block = this->next(block);
                    if (block < this->tail)
                    {
                        buddy = this->next(block);
                        no_coalescence = false;
                    }
                }
                else if (block->size < buddy->size)
                {
                    block = buddy;
                    buddy = this->next(buddy);
                }
                else
                {
                    block = this->next(buddy);
                    if (block < this->tail) buddy = this->next(block);
                }
            }

            if (no_coalescence) return;
        }
    }

    public:
    bool debug = false;

    BuddyAlloc(size_t size)
    {
        this->expand(size);
    }

    ~BuddyAlloc()
    {
        this->head = nullptr;
        this->tail = nullptr;
        std::free(this->data);
    }

    void expand(size_t size)
    {
        assert(size != 0 && "Size can not be zero!");

        if (this->head) size += this->head->size;
        size = pow(2, ceil(log(size) / log(2)));

        assert(POWER_OF_2(size) && "Size is not power of two!");

        this->data = std::realloc(this->data, size);
        assert(data != NULL && "Could not allocate memory!");

        this->head = (BuddyBlock*)data;
        this->head->size = size;
        this->head->free = true;

        this->tail = next(head);

        if (this->debug) std::cout << "Expanded the heap. Current size: " << size << " bytes" << std::endl;
    }

    void setsize(size_t size)
    {
        assert(size != 0 && "Size can not be zero!");
        assert(size > this->head->size && "Size needs to be higher than current size!");
        size -= this->head->size;
        this->expand(size);
    }

    void *malloc(size_t size)
    {
        if (size == 0) return NULL;

        size_t actual_size = this->required_size(size);

        BuddyBlock *found = this->find_best(actual_size);
        if (found == NULL)
        {
            this->coalescence();
            found = this->find_best(actual_size);
        }

        if (found != NULL)
        {
            if (this->debug) std::cout << "Allocated " << size << " bytes" << std::endl;
            found->free = false;
            this->expanded = false;
            return (void*)((char*)found + sizeof(BuddyBlock));
        }

        if (this->expanded)
        {
            assert(!this->debug && "Could not expand the heap!");
            this->expanded = false;
            return NULL;
        }
        this->expanded = true;
        this->expand(size);
        return this->malloc(size);
    }

    void *calloc(size_t num, size_t size)
    {
        void *ptr = this->malloc(num * size);
        if (!ptr) return NULL;

        std::memset(ptr, 0, num * size);
        return ptr;
    }

    void *realloc(void *ptr, size_t size)
    {
        if (!ptr) return this->malloc(size);

        BuddyBlock *block = (BuddyBlock*)((char*)ptr - sizeof(BuddyBlock));
        size_t oldsize = block->size;

        if (size == 0)
        {
            this->free(ptr);
            return NULL;
        }
        if (size < oldsize) oldsize = size;

        void *newptr = this->malloc(size);
        if (newptr == NULL) return ptr;

        std::memcpy(newptr, ptr, oldsize);
        this->free(ptr);
        return newptr;
    }

    void free(void *ptr)
    {
        if (ptr == NULL) return;

        assert(this->head <= ptr && "Head is not smaller than pointer!");
        assert(ptr < this->tail && "Pointer is not smaller than tail!");

        BuddyBlock *block = (BuddyBlock*)((char*)ptr - sizeof(BuddyBlock));
        block->free = true;

        if (this->debug) std::cout << "Freed " << block->size - sizeof(BuddyBlock) << " bytes" << std::endl;

        this->coalescence();
    }

    size_t allocsize(void *ptr)
    {
        if (!ptr) return 0;
        return ((BuddyBlock*)((char*)ptr - sizeof(BuddyBlock)))->size - sizeof(BuddyBlock);
    }
};