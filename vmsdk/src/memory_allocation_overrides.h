/*
 * Copyright (c) 2025, ValkeySearch contributors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef VMSDK_SRC_MEMORY_ALLOCATION_OVERRIDES_H_
#define VMSDK_SRC_MEMORY_ALLOCATION_OVERRIDES_H_

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>

extern "C" {
// NOLINTNEXTLINE
void* (*__real_malloc)(size_t) = malloc;
// NOLINTNEXTLINE
void (*__real_free)(void*) = free;
// NOLINTNEXTLINE
void* (*__real_calloc)(size_t, size_t) = calloc;
// NOLINTNEXTLINE
void* (*__real_realloc)(void*, size_t) = realloc;
// NOLINTNEXTLINE
void* (*__real_aligned_alloc)(size_t, size_t) = aligned_alloc;
// NOLINTNEXTLINE
int (*__real_posix_memalign)(void**, size_t, size_t) = posix_memalign;
// NOLINTNEXTLINE
void* (*__real_valloc)(size_t) = valloc;
// NOLINTNEXTLINE
__attribute__((weak)) size_t empty_usable_size(void* ptr) noexcept;
}  // extern "C"

extern "C" {
// See https://www.gnu.org/software/libc/manual/html_node/Replacing-malloc.html
// NOLINTNEXTLINE
void* __wrap_malloc(size_t size) noexcept;
// NOLINTNEXTLINE
void __wrap_free(void* ptr) noexcept;
// NOLINTNEXTLINE
void* __wrap_calloc(size_t __nmemb, size_t size) noexcept;
// NOLINTNEXTLINE
void* __wrap_realloc(void* ptr, size_t size) noexcept;
// NOLINTNEXTLINE
void* __wrap_aligned_alloc(size_t __alignment, size_t __size) noexcept;
// NOLINTNEXTLINE
int __wrap_malloc_usable_size(void* ptr) noexcept;
// NOLINTNEXTLINE
int __wrap_posix_memalign(void** r, size_t __alignment, size_t __size);
// NOLINTNEXTLINE
void* __wrap_valloc(size_t size) noexcept;
}  // extern "C"

// NOLINTNEXTLINE
#define malloc(...) __wrap_malloc(__VA_ARGS__)
// NOLINTNEXTLINE
#define calloc(...) __wrap_calloc(__VA_ARGS__)
// NOLINTNEXTLINE
#define realloc(...) __wrap_realloc(__VA_ARGS__)
// NOLINTNEXTLINE
#define free(...) __wrap_free(__VA_ARGS__)
// NOLINTNEXTLINE
#define aligned_alloc(...) __wrap_aligned_alloc(__VA_ARGS__)
// NOLINTNEXTLINE
#define posix_memalign(...) __wrap_posix_memalign(__VA_ARGS__)
// NOLINTNEXTLINE
#define valloc(...) __wrap_valloc(__VA_ARGS__)

// NOLINTNEXTLINE
void* operator new(size_t __sz) noexcept(false);
void operator delete(void* p) noexcept;
// NOLINTNEXTLINE
void operator delete(void* p, size_t __sz) noexcept;
// NOLINTNEXTLINE
void* operator new[](size_t __sz) noexcept(false);
void operator delete[](void* p) noexcept;
// NOLINTNEXTLINE
void operator delete[](void* p, size_t __sz) noexcept;
// NOLINTNEXTLINE
void* operator new(size_t __sz, const std::nothrow_t& nt) noexcept;
// NOLINTNEXTLINE
void* operator new[](size_t __sz, const std::nothrow_t& nt) noexcept;
void operator delete(void* p, const std::nothrow_t& nt) noexcept;
void operator delete[](void* p, const std::nothrow_t& nt) noexcept;
// NOLINTNEXTLINE
void* operator new(size_t __sz, std::align_val_t alignment) noexcept(false);
// NOLINTNEXTLINE
void* operator new(size_t __sz, std::align_val_t alignment,
                   const std::nothrow_t&) noexcept;
void operator delete(void* p, std::align_val_t alignment) noexcept;
void operator delete(void* p, std::align_val_t alignment,
                     const std::nothrow_t&) noexcept;
// NOLINTNEXTLINE
void operator delete(void* p, size_t __sz, std::align_val_t alignment) noexcept;
// NOLINTNEXTLINE
void* operator new[](size_t __sz, std::align_val_t alignment) noexcept(false);
// NOLINTNEXTLINE
void* operator new[](size_t __sz, std::align_val_t alignment,
                     const std::nothrow_t&) noexcept;
void operator delete[](void* p, std::align_val_t alignment) noexcept;
void operator delete[](void* p, std::align_val_t alignment,
                       const std::nothrow_t&) noexcept;
// NOLINTNEXTLINE
void operator delete[](void* p, size_t __sz,
                       std::align_val_t alignment) noexcept;

inline void SetRealAllocators(void* (*malloc_fn)(size_t),
                              void (*free_fn)(void*),
                              void* (*calloc_fn)(size_t, size_t),
                              void* (*realloc_fn)(void*, size_t),
                              void* (*aligned_alloc_fn)(size_t, size_t),
                              int (*posix_memalign_fn)(void**, size_t, size_t),
                              void* (*valloc_fn)(size_t)) {
  __real_malloc = malloc_fn;
  __real_free = free_fn;
  __real_calloc = calloc_fn;
  __real_realloc = realloc_fn;
  __real_aligned_alloc = aligned_alloc_fn;
  __real_posix_memalign = posix_memalign_fn;
  __real_valloc = valloc_fn;
}

#endif  // VMSDK_SRC_MEMORY_ALLOCATION_OVERRIDES_H_
