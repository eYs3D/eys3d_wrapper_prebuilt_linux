/*
 * Copyright (C) 2025 eYs3D Corporation
 * All rights reserved.
 * This project is licensed under the Apache License, Version 2.0.
 */

#pragma once

#include <cstdlib>
#include <new>
#include <memory>

#ifdef WIN32
#include <windows.h>
#include <malloc.h>
#else
#include <unistd.h>
#endif

namespace libeYs3D {
namespace devices {

template<typename T>
class AlignedAllocator {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    
    // Get system page size for optimal alignment
    static size_t getPageSize() {
#ifdef WIN32
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        return si.dwPageSize;
#else
        return sysconf(_SC_PAGESIZE);
#endif
    }
    
    template<typename U>
    struct rebind {
        using other = AlignedAllocator<U>;
    };
    
    AlignedAllocator() = default;
    
    template<typename U>
    AlignedAllocator(const AlignedAllocator<U>&) noexcept {}
    
    pointer allocate(size_type n) {
        size_t pageSize = getPageSize();
        size_t size = n * sizeof(T);
        
        // Align size to page boundaries
        size_t alignedSize = ((size + pageSize - 1) / pageSize) * pageSize;
        
        void* ptr = nullptr;
        
#ifdef WIN32
        ptr = _aligned_malloc(alignedSize, pageSize);
        if (!ptr) {
            throw std::bad_alloc();
        }
#else
        if (posix_memalign(&ptr, pageSize, alignedSize) != 0) {
            throw std::bad_alloc();
        }
#endif
        
        return static_cast<pointer>(ptr);
    }
    
    void deallocate(pointer ptr, size_type n) noexcept {
        (void)n; // Suppress unused parameter warning
#ifdef WIN32
        _aligned_free(ptr);
#else
        free(ptr);
#endif
    }
    
    // Required for C++17 allocator requirements
    template<typename U>
    bool operator==(const AlignedAllocator<U>&) const noexcept { 
        return true; 
    }
    
    template<typename U>
    bool operator!=(const AlignedAllocator<U>&) const noexcept { 
        return false; 
    }
};

} // namespace devices
} // namespace libeYs3D