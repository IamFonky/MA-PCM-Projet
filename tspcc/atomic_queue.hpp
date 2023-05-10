#include <iostream>
#include <cstdint>
#include <atomic>
#include "path.hpp"

struct NODE; // Allows using addr without cast

typedef struct ADDRESS
{
    uint32_t ref;
    struct NODE *addr;
} ADDRESS;

typedef union
{
    ADDRESS link;
    uint64_t val;
} CONVERSION;

typedef struct NODE
{
    Path *value;
    CONVERSION next;
} NODE;

class AtomicQueue
{
private:
    enum
    {
        Head = 0,
        Tail = 1
    };

    CONVERSION Fifo[2];
    CONVERSION FreeNodes[2];

public:
    AtomicQueue()
    {
        InitializeQueue();
    }

    bool full()
    {
        return Fifo[Head].link.addr->next.link.addr == Fifo[Tail].link.addr;
    }

    bool InitializeQueue()
    {
        NODE *node = (NODE *)malloc(sizeof(NODE));
        if (node == NULL)
            return false;
        node->next.link.addr = NULL;
        Fifo[Head].link.addr = Fifo[Tail].link.addr = node;
        if ((node = (NODE *)malloc(sizeof(NODE))) == NULL)
        {
            free(Fifo[Head].link.addr);
            return false;
        }
        node->next.link.addr = NULL;
        FreeNodes[Head].link.addr = node;
        FreeNodes[Tail].link.addr = node;
        return true;
    }

    bool Enq(Path *value)
    {
        NODE *node = GetFreeNode();
        if (node == NULL)
            return false;
        node->value = value;
        EnqNode(Fifo, node);
        return true;
    }

    void FreeNode(NODE *node)
    {
        EnqNode(FreeNodes, node);
    }

    void EnqNode(CONVERSION *queue, NODE *node)
    {
        CONVERSION  newNode;
        node->next.link.ref += 1;
        node->next.link.addr = NULL;
        while (true)
        {
            last.val = queue[Tail].val;
            next.val = last.link.addr->next.val;
            if (last.val == queue[Tail].val)
                if (next.link.addr == NULL)
                {
                    newNode.link.addr = node;
                    newNode.link.ref = next.link.ref + 1;
                    if (__atomic_compare_exchange(&last.link.addr->next.val, &next.val, &newNode.val, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED))
                        break;
                }
                else
                {
                    next.link.ref = last.link.ref + 1;
                    __atomic_compare_exchange(&queue[Tail].val, &last.val, &next.val, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
                }
        }
        newNode.link.addr = node;
        newNode.link.ref = last.link.ref + 1;
        __atomic_compare_exchange(&queue[Tail].val, &last.val, &newNode.val, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
    }

    Path *Deq()
    {
        CONVERSION first, last, next;
        Path *value;
        while (true)
        {
            first.val = Fifo[Head].val;
            last.val = Fifo[Tail].val;
            next.val = first.link.addr->next.val;
            if (first.val == Fifo[Head].val)
                if (first.link.addr == last.link.addr)
                {
                    if (next.link.addr == NULL)
                        return NULL;
                    next.link.ref = last.link.ref + 1;
                    __atomic_compare_exchange(&Fifo[Tail].val, &last.val, &next.val, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
                }
                else
                {
                    value = next.link.addr->value;
                    next.link.ref = first.link.ref + 1;
                    if (__atomic_compare_exchange(&Fifo[Head].val, &first.val, &next.val, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED))
                    {
                        FreeNode(first.link.addr);
                        return value;
                    }
                }
        }
    }

    NODE *GetFreeNode()
    {
        CONVERSION first, last, next;
        while (true)
        {
            first.val = FreeNodes[Head].val;
            last.val = FreeNodes[Tail].val;
            next.val = first.link.addr->next.val;
            if (first.val == FreeNodes[Head].val)
                if (first.link.addr == last.link.addr)
                {
                    if (next.link.addr == NULL)
                        return (NODE *)malloc(sizeof(NODE));
                    next.link.ref = last.link.ref + 1;
                    __atomic_compare_exchange(&FreeNodes[Tail].val, &last.val, &next.val, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
                }
                else
                {
                    next.link.ref = first.link.ref + 1;
                    if (__atomic_compare_exchange(&FreeNodes[Head].val, &first.val, &next.val, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED))
                        return first.link.addr;
                }
        }
    }
};
