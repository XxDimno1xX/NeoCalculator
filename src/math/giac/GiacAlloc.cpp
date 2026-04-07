// GiacAlloc.cpp
// Optional global operator new/delete override to prefer PSRAM for Giac allocations.
// This file is only enabled when building with NUMOS_USE_GIAC and BOARD_HAS_PSRAM.

#include <new>
#include <cstdlib>

#if defined(NUMOS_USE_GIAC) && defined(ARDUINO) && defined(BOARD_HAS_PSRAM)
#include <esp_heap_caps.h>
#include <Arduino.h>

void* operator new(std::size_t sz) {
    if (sz == 0) sz = 1;
    void* p = heap_caps_malloc(sz, MALLOC_CAP_SPIRAM);
    if (!p) p = malloc(sz); // fallback to internal heap
    if (!p) throw std::bad_alloc();
    return p;
}

void operator delete(void* p) noexcept {
    if (!p) return;
    heap_caps_free(p);
}

void* operator new[](std::size_t sz) {
    if (sz == 0) sz = 1;
    void* p = heap_caps_malloc(sz, MALLOC_CAP_SPIRAM);
    if (!p) p = malloc(sz);
    if (!p) throw std::bad_alloc();
    return p;
}

void operator delete[](void* p) noexcept {
    if (!p) return;
    heap_caps_free(p);
}

// sized delete (C++14)
void operator delete(void* p, std::size_t) noexcept { if (p) heap_caps_free(p); }
void operator delete[](void* p, std::size_t) noexcept { if (p) heap_caps_free(p); }

#endif // NUMOS_USE_GIAC && ARDUINO && BOARD_HAS_PSRAM
