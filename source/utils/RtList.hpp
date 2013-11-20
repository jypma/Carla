/*
 * High-level, real-time safe, templated, C++ doubly-linked list
 * Copyright (C) 2013 Filipe Coelho <falktx@falktx.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * For a full copy of the GNU General Public License see the doc/GPL.txt file.
 */

#ifndef RT_LIST_HPP_INCLUDED
#define RT_LIST_HPP_INCLUDED

#include "List.hpp"

extern "C" {
#include "rtmempool/rtmempool.h"
}

// -----------------------------------------------------------------------
// Realtime safe list

template<typename T>
class RtList : public AbstractList<T>
{
public:
    // -------------------------------------------------------------------
    // RtMemPool C++ class

    class Pool
    {
    public:
        Pool(const size_t minPreallocated, const size_t maxPreallocated)
            : fHandle(nullptr),
              fDataSize(sizeof(typename AbstractList<T>::Data))
        {
            resize(minPreallocated, maxPreallocated);
        }

        ~Pool()
        {
            if (fHandle != nullptr)
            {
                rtsafe_memory_pool_destroy(fHandle);
                fHandle = nullptr;
            }
        }

        void* allocate_atomic() const
        {
            return rtsafe_memory_pool_allocate_atomic(fHandle);
        }

        void* allocate_sleepy() const
        {
            return rtsafe_memory_pool_allocate_sleepy(fHandle);
        }

        void deallocate(void* const dataPtr) const
        {
            rtsafe_memory_pool_deallocate(fHandle, dataPtr);
        }

        void resize(const size_t minPreallocated, const size_t maxPreallocated)
        {
            if (fHandle != nullptr)
            {
                rtsafe_memory_pool_destroy(fHandle);
                fHandle = nullptr;
            }

            rtsafe_memory_pool_create(&fHandle, nullptr, fDataSize, minPreallocated, maxPreallocated);
            CARLA_ASSERT(fHandle != nullptr);
        }

        bool operator==(const Pool& pool) const noexcept
        {
            return (fHandle == pool.fHandle && fDataSize == pool.fDataSize);
        }

        bool operator!=(const Pool& pool) const noexcept
        {
            return (fHandle != pool.fHandle || fDataSize != pool.fDataSize);
        }

    private:
        mutable RtMemPool_Handle fHandle;
        const size_t             fDataSize;
    };

    // -------------------------------------------------------------------
    // Now the actual rt-list code

    RtList(Pool& memPool)
        : fMemPool(memPool)
    {
    }

    void append_sleepy(const T& value)
    {
        if (typename AbstractList<T>::Data* const data = _allocate_sleepy())
        {
            new(data)typename AbstractList<T>::Data();
            data->value = value;
            list_add_tail(&data->siblings, &this->fQueue);
            ++(this->fCount);
        }
    }

    void insert_sleepy(const T& value)
    {
        if (typename AbstractList<T>::Data* const data = _allocate_sleepy())
        {
            new(data)typename AbstractList<T>::Data();
            data->value = value;
            list_add(&data->siblings, &this->fQueue);
            ++(this->fCount);
        }
    }

    void resize(const size_t minPreallocated, const size_t maxPreallocated)
    {
        this->clear();

        fMemPool.resize(minPreallocated, maxPreallocated);
    }

    void spliceAppend(RtList& list, const bool init = true)
    {
        CARLA_ASSERT(fMemPool == list.fMemPool);

        AbstractList<T>::spliceAppend(list, init);
    }

    void spliceInsert(RtList& list, const bool init = true)
    {
        CARLA_ASSERT(fMemPool == list.fMemPool);

        AbstractList<T>::spliceInsert(list, init);
    }

private:
    Pool& fMemPool;

    typename AbstractList<T>::Data* _allocate() override
    {
        return (typename AbstractList<T>::Data*)fMemPool.allocate_atomic();
    }

    typename AbstractList<T>::Data* _allocate_sleepy()
    {
        return (typename AbstractList<T>::Data*)fMemPool.allocate_sleepy();
    }

    void _deallocate(typename AbstractList<T>::Data*& dataPtr) override
    {
        CARLA_SAFE_ASSERT_RETURN(dataPtr != nullptr,);

        fMemPool.deallocate(dataPtr);
        dataPtr = nullptr;
    }

    LIST_DECLARATIONS(RtList)
};

// -----------------------------------------------------------------------

#endif // RT_LIST_HPP_INCLUDED
