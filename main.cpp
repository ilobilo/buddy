#include "buddy.hpp"
#include <new>

int main()
{
    // 14 bytes will be rounded up to the next power-of-2 (16)
    BuddyAlloc *alloc = new BuddyAlloc(14);

    // Expand the heap by 15 bytes
    // Size (16 + 25 = 41) will be rounded up to 64 bytes
    alloc->expand(25);

    // Set heap size to 128 bytes
    alloc->setsize(128);

    // Enable debug mode
    alloc->debug = true;

    // Allocate 4 * int bytes
    int *ptr = (int*)alloc->malloc(4 * sizeof(int));

    // Assign values
    ptr[0] = 423;
    ptr[1] = 345;
    ptr[2] = 876;
    ptr[3] = 132;

    // Print the contents
    for (size_t i = 0; i < 4; i++) std::cout << i + 1 << ". " << ptr[i] << std::endl;

    // Print allocated size of ptr
    std::cout << std::endl << "Current size: " << alloc->allocsize(ptr) << " bytes" << std::endl << std::endl;

    // Reallocate more memory to ptr
    ptr = (int*)alloc->realloc(ptr, 6 * sizeof(int));

    // Assign values
    ptr[4] = 2235;
    ptr[5] = 897878;

    // Print again
    for (size_t i = 0; i < 6; i++) std::cout << i + 1 << ". " << ptr[i] << std::endl;

    // Print allocated size of ptr
    std::cout << std::endl << "Current size: " << alloc->allocsize(ptr) << " bytes" << std::endl << std::endl;

    // Free ptr
    alloc->free(ptr);

    // Free the heap
    delete alloc;
}