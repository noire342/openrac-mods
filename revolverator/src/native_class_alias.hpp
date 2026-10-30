// SPDX-License-Identifier: MIT
// Copyright (c) 2026 noire342
#pragma once
#include "memory.hpp"
#include <cstdint>

namespace revolverator {

// A vendor-preview class with native ammunition data and a distinct render ID.
// InitMobyInstance copies the existing slot's pclass/update but retains the
// requested ID at Moby+A6. One free ID can therefore share class225's slot;
// no resource relocation, allocation, callback or resident-slot changes occur.
class NativeAmmoAlias final : public Memory {
public:
    static constexpr unsigned source_class=225;
    static constexpr unsigned native_class=2016;
    static constexpr unsigned id_capacity=0x800;
    static constexpr unsigned slot_capacity=224;

    explicit NativeAmmoAlias(const openrac_api* api):Memory(api){}
    ~NativeAmmoAlias(){reset();}
    NativeAmmoAlias(const NativeAmmoAlias&)=delete;
    NativeAmmoAlias& operator=(const NativeAmmoAlias&)=delete;

    // Call before NativeVendor.frame/build; publish f30=native_class only while
    // ready(). Reset the vendor's use first, then this alias, before level unload.
    void frame();
    void reset();
    bool ready() const;

private:
    int level_=-1;
    unsigned map_=0;
    std::uint8_t slot_=0xff;
    bool owned_=false;
    bool supported() const;
    bool source_valid(unsigned map,std::uint8_t slot) const;
};
}
