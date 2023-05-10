#include "include/atomic_queue/atomic_queue.h"
#include <pthread.h>
#include <cstdint>
#include <iostream>

unsigned constexpr CAPACITY = 3;

using Element = uint32_t;
Element constexpr NIL = static_cast<Element>(-1);
using Queue = atomic_queue::AtomicQueueB<Element, std::allocator<Element>, NIL>;


int main(int argc, char *argv[])
{

    Element value1 = 1;
    Element value2 = 2;

    // Create a queue object shared between all producers and consumers.
    Queue q{CAPACITY};

    for (size_t i = 0; i < 10; i++)
    {
        q.push(i);
        printf("size: %d\n", q.get_size());
    }

    Element v1;

    for (size_t i = 0; i < 10; i++)
    {
        v1 = q.pop();
        printf("value: %d\n", v1);

    }
}
