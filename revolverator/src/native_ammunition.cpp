// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_ammunition.hpp"
#include "localization.hpp"

#include <algorithm>
#include <cstring>
#include <utility>

namespace revolverator {
namespace {
constexpr unsigned kVendorItems = 0x1c40b0;
constexpr unsigned kItemDefinitions = 0x179ac0;
constexpr unsigned kResidentItemDefinitions = 0x1863d0;
constexpr unsigned kAmmoCounts = 0x13d428;
constexpr unsigned kTextBank = 0x15f6a0;
constexpr unsigned kTextCount = 0x1996fc;
constexpr unsigned kHero = 0x13f350;
constexpr unsigned kSlotSize = 76;
constexpr unsigned kVendorSlotSize = 24;
constexpr unsigned kMobySize = 256;
constexpr unsigned kPvarsSize = 48;
constexpr unsigned kPluralOffset = 0x34;
constexpr unsigned kSingularOffset = 0x36;
constexpr unsigned kAmmoClassOffset = 0x3a;
using TextRecord = std::array<std::uint32_t,4>;
}

NativeAmmunition::NativeAmmunition(const openrac_api* api,GuestArena* arena,bool hud_fallback)
    : Memory(api,arena),
      take_ammo_(function<TakeAmmo>("FUN_L00_00233db8")) {hud_fallback_=hud_fallback;}

NativeAmmunition::~NativeAmmunition() { reset(); }

bool NativeAmmunition::in_level() const {
    return level_ >= 0 && level_ < 19 && api_->loaded_level() == level_;
}

bool NativeAmmunition::patch16(unsigned address,std::uint16_t replacement) {
    if(!valid(address,2)) return false;
    const auto existing = std::find_if(patches_.begin(),patches_.end(),
        [address](const Patch& patch){ return patch.address == address; });
    if(existing != patches_.end()) return existing->replacement == replacement;
    patches_.push_back({address,read<std::uint16_t>(address),replacement});
    write(address,replacement);
    return true;
}

bool NativeAmmunition::extend_text_bank() {
    const unsigned bank_address = global(kTextBank), count_address = global(kTextCount);
    const unsigned bank = read<unsigned>(bank_address);
    const int count = read<int>(count_address);
    if(text_bank_.copy && bank == text_bank_.copy && count == text_bank_.count + 2)
        return true;
    if(text_bank_.copy) return false;
    if(count <= 0 || count > 20000 || !valid(bank,std::size_t(count)*16)) return false;

    int first_id = 32700;
    const auto occupied = [&](int id) {
        for(int i = 0; i < count; ++i)
            if(read<int>(bank + unsigned(i)*16 + 4) == id) return true;
        return false;
    };
    while(first_id < 32766 && (occupied(first_id) || occupied(first_id+1))) first_id += 2;
    if(first_id >= 32766) return false;

    // These are ordinary native printf-style pickup messages, not an overlay.
    // Native localization chooses the surrounding message styling and timing.
    const auto& strings=localization::strings_for_language(read<int>(global(localization::native_language_address)));
    const char* plural = strings.pickup_plural;
    const char* singular = strings.pickup_singular;
    const unsigned records_size = unsigned(count+2)*16;
    const unsigned strings_size = unsigned(std::strlen(plural)+std::strlen(singular)+2);
    const unsigned copy = allocate_guest(records_size+strings_size);
    if(!valid(copy,records_size+strings_size)) return false;
    std::memcpy(api_->memory+copy,api_->memory+bank,unsigned(count)*16);
    const unsigned plural_at = copy+records_size;
    const unsigned singular_at = plural_at+unsigned(std::strlen(plural))+1;
    std::memcpy(api_->memory+plural_at,plural,std::strlen(plural)+1);
    std::memcpy(api_->memory+singular_at,singular,std::strlen(singular)+1);
    const TextRecord plural_record{plural_at,unsigned(first_id),0,0};
    const TextRecord singular_record{singular_at,unsigned(first_id+1),0,0};
    write(copy+unsigned(count)*16,plural_record);
    write(copy+unsigned(count+1)*16,singular_record);

    TextBank lease;
    lease.original = bank; lease.copy = copy; lease.count = count;
    lease.snapshot.resize(unsigned(count));
    std::memcpy(lease.snapshot.data(),api_->memory+bank,unsigned(count)*16);
    text_bank_ = std::move(lease);
    plural_id_ = first_id; singular_id_ = first_id+1;
    write(bank_address,copy);
    write(count_address,count+2);
    return true;
}

bool NativeAmmunition::patch_metadata() {
    const unsigned vendor = level(kVendorItems)+item_id*kVendorSlotSize;
    const unsigned definitions = level(kItemDefinitions)+item_id*kSlotSize;
    if(!valid(vendor,kVendorSlotSize) || !valid(definitions,kSlotSize) ||
       read<int>(definitions+0x10) != 180) return false;
    const bool custom_messages=extend_text_bank();

    // h8/hA are the regular and challenge-mode ammunition unit prices. Their
    // nonzero value also causes native inventory/HUD/vendor code to list ammo.
    const std::pair<unsigned,std::uint16_t> table_fields[] = {
        {8u,std::uint16_t(unit_price)},{10u,std::uint16_t(unit_price)},
        {12u,std::uint16_t(pickup)},{14u,std::uint16_t(capacity)},{18u,std::uint16_t(capacity)}
    };
    for(const auto& field:table_fields)
        if(!patch16(vendor+field.first,field.second)) return false;

    const unsigned resident = global(kResidentItemDefinitions)+item_id*kSlotSize;
    for(unsigned definition:{definitions,resident}) {
        if(!valid(definition,kSlotSize) || read<int>(definition+0x10) != 180) continue;
        if(!patch16(definition+kAmmoClassOffset,native_pickup_class)) return false;
        if(custom_messages&&(!patch16(definition+kPluralOffset,std::uint16_t(plural_id_)) ||
           !patch16(definition+kSingularOffset,std::uint16_t(singular_id_)))) return false;
    }
    // patch16 de-duplicates fields when the resident address aliases the level's.
    return true;
}

void NativeAmmunition::frame() {
    const int current = api_->loaded_level();
    if(current != level_) { reset(); level_ = current; }
    if(!in_level()) return;
    if(!metadata_ready_) metadata_ready_ = patch_metadata();
}

bool NativeAmmunition::begin_attack() {
    frame();
    if(!in_level() || !metadata_ready_) return false;
    const unsigned hero = global(kHero);
    if(read<int>(hero+0x10b8) != item_id || read<int>(hero+0x10b4) != 2 ||
       read<int>(hero+0x22a8) <= 0 || read<int>(level(0x15f5c4)) != 0) return false;
    return take_ammo_(item_id,1) != 0;
}

int NativeAmmunition::count() const {
    return in_level() ? std::max(0,read<int>(global(kAmmoCounts)+item_id*4)) : 0;
}

int NativeAmmunition::maximum() const {
    return in_level() && metadata_ready_
        ? int(read<std::uint16_t>(level(kVendorItems)+item_id*kVendorSlotSize+14)) : 0;
}

bool NativeAmmunition::pickup_valid(const Pickup& lease) const {
    return in_level() && valid(lease.address,kMobySize) &&
        read<unsigned>(lease.address+0x78) == lease.pvars &&
        read<unsigned>(lease.address+0x38) == lease.stamp &&
        valid(lease.pvars,kPvarsSize) && read<int>(lease.pvars+8) == item_id;
}

NativeAmmunition::Pickup* NativeAmmunition::find_pickup(unsigned moby) {
    const auto found = std::find_if(pickups_.begin(),pickups_.end(),
        [moby](const Pickup& lease){ return lease.address == moby; });
    return found == pickups_.end() ? nullptr : &*found;
}

void NativeAmmunition::before_update(unsigned moby) {
    if(!in_level() || !metadata_ready_ || !valid(moby,kMobySize)) return;
    const auto state = read<unsigned char>(moby+0x20);
    const auto klass = read<std::uint16_t>(moby+0xa6);
    const auto vars = read<unsigned>(moby+0x78);
    if(state >= 0xfd || !valid(vars,kPvarsSize) || read<int>(vars+8) != item_id ||
       (klass != native_pickup_class && klass != visual_class)) return;
    auto* lease = find_pickup(moby);
    if(lease && !pickup_valid(*lease)) {
        pickups_.erase(std::remove_if(pickups_.begin(),pickups_.end(),
            [moby](const Pickup& p){return p.address == moby;}),pickups_.end());
        lease = nullptr;
    }
    if(!lease) {
        // The native spawned pool is 255 entries. Retain a hard bound even if a
        // malformed runtime repeatedly offers new entity addresses.
        if(pickups_.size() >= 512) return;
        pickups_.push_back({moby,vars,read<unsigned>(moby+0x38),native_pickup_class});
    }
    write<std::uint16_t>(moby+0xa6,native_pickup_class);
}

void NativeAmmunition::after_update(unsigned moby) {
    auto* lease = find_pickup(moby);
    if(!lease) return;
    if(!pickup_valid(*lease) || read<unsigned char>(moby+0x20) >= 0xfd) {
        pickups_.erase(std::remove_if(pickups_.begin(),pickups_.end(),
            [moby](const Pickup& p){return p.address == moby;}),pickups_.end());
        return;
    }
    if(read<std::uint16_t>(moby+0xa6) == native_pickup_class)
        write<std::uint16_t>(moby+0xa6,visual_class);
}

void NativeAmmunition::restore_text_bank() {
    if(!text_bank_.copy || read<unsigned>(global(kTextBank)) != text_bank_.copy ||
       read<int>(global(kTextCount)) != text_bank_.count+2) return;
    // Preserve another mod's edits made to original entries of the extended
    // bank, unless that mod independently changed the original record too.
    for(int i = 0; i < text_bank_.count; ++i) {
        const unsigned old_at = text_bank_.original+unsigned(i)*16;
        const unsigned current_at = text_bank_.copy+unsigned(i)*16;
        const auto original = read<TextRecord>(old_at);
        const auto current = read<TextRecord>(current_at);
        if(original == text_bank_.snapshot[unsigned(i)] && current != original)
            write(old_at,current);
    }
    write(global(kTextBank),text_bank_.original);
    write(global(kTextCount),text_bank_.count);
}

void NativeAmmunition::reset() {
    if(in_level()) {
        for(const auto& lease:pickups_)
            if(pickup_valid(lease) && read<std::uint16_t>(lease.address+0xa6) == visual_class)
                write(lease.address+0xa6,lease.original_class);
        for(auto it = patches_.rbegin(); it != patches_.rend(); ++it)
            if(read<std::uint16_t>(it->address) == it->replacement) write(it->address,it->original);
        restore_text_bank();
    }
    pickups_.clear(); patches_.clear(); text_bank_={};
    level_=-1; metadata_ready_=false;plural_id_=singular_id_=0;
}
}
