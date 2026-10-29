// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <cstring>

#include "check.hpp"
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif

// A real SDK stack handle is above the 32 MB console RAM. Reserve address space, commit only
// the regions the fixture uses, and let an accidental access to any other region fail.
class GuestMemory {
public:
    GuestMemory() {
#if defined(_WIN32)
        m_data =
            static_cast<std::uint8_t*>(VirtualAlloc(nullptr, span, MEM_RESERVE, PAGE_NOACCESS));
        CHECK(m_data && VirtualAlloc(m_data, ram_size, MEM_COMMIT, PAGE_READWRITE));
        CHECK(VirtualAlloc(m_data + 0xE0000000u, 0x100000, MEM_COMMIT, PAGE_READWRITE));
#else
        void* p = mmap(nullptr, span, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        CHECK(p != MAP_FAILED);
        m_data = static_cast<std::uint8_t*>(p);
        CHECK(mprotect(m_data, ram_size, PROT_READ | PROT_WRITE) == 0);
        CHECK(mprotect(m_data + 0xE0000000u, 0x100000, PROT_READ | PROT_WRITE) == 0);
#endif
    }

    ~GuestMemory() {
#if defined(_WIN32)
        VirtualFree(m_data, 0, MEM_RELEASE);
#else
        munmap(m_data, span);
#endif
    }

    GuestMemory(const GuestMemory&) = delete;
    GuestMemory& operator=(const GuestMemory&) = delete;

    std::uint8_t* data() { return m_data; }

    std::uint8_t* begin() { return m_data; }

    std::uint8_t* end() { return m_data + ram_size; }

    std::size_t size() const { return span; }

private:
    static constexpr std::size_t span = std::size_t{1} << 32, ram_size = 0x02000000;
    std::uint8_t* m_data = nullptr;
};
