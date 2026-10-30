// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include <cstddef>
#include <cstdint>

namespace revolverator {

// Session-owned storage reserved from LoadLevelGameplay's returned heap cursor.
// The hook must return reserve()'s result, so subsequent native users see this
// range as occupied. Release every arena user before reset()/the next reserve;
// the level loader owns and reclaims the underlying storage on a level change.
class GuestArena final {
public:
    static constexpr std::uint32_t ceiling=0x1ff8000;
    static constexpr std::uint32_t alignment=16;
    GuestArena()=default;
    GuestArena(const GuestArena&)=delete;
    GuestArena& operator=(const GuestArena&)=delete;

    std::uint32_t reserve(std::uint32_t origin,std::size_t bytes) {
        reset();
        if(origin<0x10000||origin>ceiling||bytes==0||bytes>ceiling)return origin;
        const auto start=(origin+alignment-1)&~(alignment-1);
        const auto rounded=(bytes+alignment-1)&~std::size_t(alignment-1);
        if(start>ceiling||rounded>ceiling-start)return origin;
        begin_=cursor_=start;end_=start+std::uint32_t(rounded);
        return end_;
    }
    std::uint32_t allocate(std::size_t bytes) {
        if(!begin_||bytes==0||bytes>available())return 0;
        const auto rounded=(bytes+alignment-1)&~std::size_t(alignment-1);
        if(rounded>available())return 0;
        const auto result=cursor_;cursor_+=std::uint32_t(rounded);return result;
    }
    void reset(){begin_=cursor_=end_=0;}
    std::uint32_t begin()const{return begin_;}
    std::uint32_t end()const{return end_;}
    std::uint32_t available()const{return end_-cursor_;}
    std::uint32_t used()const{return cursor_-begin_;}
    bool owns(std::uint32_t at,std::size_t bytes)const {
        return begin_&&at>=begin_&&at<=end_&&bytes<=end_-at;
    }
private:
    std::uint32_t begin_{},cursor_{},end_{};
};
}
