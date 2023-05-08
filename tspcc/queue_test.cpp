#include "include/atomic_queue/atomic_queue.h"
#include <pthread.h>
#include <cstdint>
#include <iostream>

unsigned constexpr CAPACITY = 1024;

using Element = uint32_t;                                                        // Queue element type.
Element constexpr NIL = static_cast<Element>(-1);                                // Atomic elements require a special value that cannot be pushed/popped.
using Queue = atomic_queue::AtomicQueueB<Element, std::allocator<Element>, NIL>; // Use heap-allocated buffer.



int main(int argc, char *argv[])
{

    Element value1 = 1;
    Element value2 = 2;

    // Create a queue object shared between all producers and consumers.
    Queue q{CAPACITY};
    q.push(value1);
    q.push(value2);

    Element v2= q.pop();
    printf("value1: %d\n", v2);
    v2 = q.pop();
    printf("value2: %d\n", v2);
}
