
#ifndef _lifo_hpp
#define _lifo_hpp

#include <atomic>

template <typename T>
class AtomicLifo
{
private:
    T **data;
    int top;
    int size;
    uint64_t op_counter;

public:
    AtomicLifo(int size)
    {
        data = new T*[size];
        top = -1;
        op_counter = 0;
        this->size = size;
    }

    ~AtomicLifo()
    {
        delete data;
    }

    bool push(T* item)
    {
        uint64_t counter = op_counter;
        int t = top;
        int newt = t+1;

        if(newt >= size){
            return false;
        }
        if(!__atomic_compare_exchange_n(&op_counter, &counter, counter + 1, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)){
            return false;
        }
        if(!__atomic_compare_exchange(&top, &t, &newt, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)){
            return false;
        }
        data[newt] = item;
        return true;
    }


    T* pop()
    {
        uint64_t counter = op_counter;
        int t = top;
        T* item;

        if(t < 0){
            return nullptr;
        }
        int newt = t-1;
        if(!__atomic_compare_exchange_n(&op_counter, &counter, counter + 1, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)){
            return nullptr;
        }
        if(!__atomic_compare_exchange(&top, &t, &newt, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)){
            return nullptr;
        }
        item = data[t];

        return item;
    }

    int get_size(){
        return top+1;
    }
};

#endif //_lifo_hpp