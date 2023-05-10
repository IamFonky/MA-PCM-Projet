#include "include/atomic_queue/atomic_queue.h"
#include "graph.hpp"
#include "path.hpp"
#include "tspfile.hpp"


#include <pthread.h>
#include <cstdint>
#include <iostream>

unsigned constexpr CAPACITY = 10;

using Element = Path*;                                                        // Queue element type.
Element constexpr NIL = static_cast<Element>(NULL);                                // Atomic elements require a special value that cannot be pushed/popped.
using Queue = atomic_queue::AtomicQueueB<Element, std::allocator<Element>, NIL>; // Use heap-allocated buffer.


int main(int argc, char *argv[])
{
	Graph* g = TSPFile::graph("./dj38.tsp");

	Element path1 = new Path(g);
	path1->add(0);
    std::cout << "path1 " << path1 << '\n';

	Element path2 = new Path(path1);
    path2->add(1);
    path2->add(2);
    std::cout << "path2 " << path2 << '\n';

    // Create a queue object shared between all producers and consumers.
    Queue q{CAPACITY};

    std::cout << "pushing path2 in queue " << '\n';
    q.push(path2);
    std::cout << "poping path2 in queue " << '\n';
    Element test = q.pop();
    std::cout << "poped path " << test << '\n';

    std::cout << "create path from poped path " << test << '\n';
	Element path3 = new Path(test);
    path3->add(3);
    path3->add(4);
    path3->add(5);
    std::cout << "path3 " << path3 << '\n';

    // for (size_t i = 0; i < 10; i++)
    // {
    //     q.push(i);
    //     printf("size: %d\n", q.get_size());
    // }

    // Element v1;

    // for (size_t i = 0; i < 10; i++)
    // {
    //     v1 = q.pop();
    //     printf("value: %d\n", v1);

    // }
}
