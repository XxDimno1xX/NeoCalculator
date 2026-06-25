/*
 * NeoCalculator - NumOS
 * Copyright (C) 2026 Juan Ramon
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#pragma once

#include <cstddef>
#include <stdexcept>
#ifdef ARDUINO
#include <esp_heap_caps.h>
#else
// Native (emulator_pc): the ESP-IDF heap-caps allocator does not exist. Fall back
// to the standard C heap with an identical PSRAMBuffer<T> API. Firmware behaviour
// is unchanged — everything ESP-specific stays under #ifdef ARDUINO.
#include <cstdlib>
#endif

namespace utils {

template<typename T>
class PSRAMBuffer {
public:
    PSRAMBuffer() = default;

    explicit PSRAMBuffer(size_t count) {
        allocate(count);
    }

    PSRAMBuffer(const PSRAMBuffer&) = delete;
    PSRAMBuffer& operator=(const PSRAMBuffer&) = delete;

    PSRAMBuffer(PSRAMBuffer&& other) noexcept
        : _data(other._data), _count(other._count) {
        other._data = nullptr;
        other._count = 0;
    }

    PSRAMBuffer& operator=(PSRAMBuffer&& other) noexcept {
        if (this != &other) {
            reset();
            _data = other._data;
            _count = other._count;
            other._data = nullptr;
            other._count = 0;
        }
        return *this;
    }

    ~PSRAMBuffer() {
        reset();
    }

    bool allocate(size_t count) {
        reset();
        if (count == 0) {
            return true;
        }
#ifdef ARDUINO
        _data = reinterpret_cast<T*>(heap_caps_malloc(count * sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
#else
        _data = reinterpret_cast<T*>(std::malloc(count * sizeof(T)));
#endif
        if (!_data) {
            _count = 0;
            return false;
        }
        _count = count;
        return true;
    }

    void reset() {
        if (_data) {
#ifdef ARDUINO
            heap_caps_free(_data);
#else
            std::free(_data);
#endif
            _data = nullptr;
            _count = 0;
        }
    }

    T* data() noexcept { return _data; }
    const T* data() const noexcept { return _data; }

    size_t size() const noexcept { return _count; }

    T& operator[](size_t idx) {
        if (idx >= _count) {
            throw std::out_of_range("PSRAMBuffer index out of range");
        }
        return _data[idx];
    }

    const T& operator[](size_t idx) const {
        if (idx >= _count) {
            // undefined behavior in embedded builds; we keep safe fallback.
            return _data[0];
        }
        return _data[idx];
    }

    explicit operator bool() const noexcept {
        return _data != nullptr;
    }

private:
    T*     _data  = nullptr;
    size_t _count = 0;
};

} // namespace utils
