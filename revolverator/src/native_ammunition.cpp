// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_ammunition.hpp"
#include "localization.hpp"
#include "native_arsenal_layout.hpp"
#include "native_weapon_alias.hpp"

#include <algorithm>
#include <cstring>
#include <utility>

namespace revolverator {
namespace {
using namespace native_arsenal_layout;
constexpr unsigned kTextBank = 0x15f6a0;
constexpr unsigned kTextCount = 0x1996fc;
constexpr unsigned kTextIdOffset = sizeof(std::uint32_t);
constexpr int kMaxTextCount = 20000;
constexpr int kPickupTextCount = 2;
constexpr int kFirstPickupTextId = 32700;
constexpr int kPickupTextIdLimit = 32766;
constexpr unsigned kPickupPvarsSize = 0x30;
constexpr unsigned kPickupItem = 0x08;
constexpr std::uint8_t kFirstInactivePickupState = 0xfd;
constexpr unsigned kMaxTrackedPickups = 512;
}

NativeAmmunition::NativeAmmunition(const openrac_api* api,GuestArena* arena,bool hud_fallback)
    : Memory(api,arena),
      take_ammo_(function<TakeAmmo>("FUN_L00_00233db8")) {hud_fallback_=hud_fallback;}

NativeAmmunition::~NativeAmmunition() { reset(); }

bool NativeAmmunition::in_level() const {
    return level_ >= 0 && level_ <= last_campaign_level && api_->loaded_level() == level_;
}

bool NativeAmmunition::patch16(unsigned address,std::uint16_t replacement) {
    if(!valid(address,sizeof(replacement))) return false;
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
    if(text_bank_.copy && bank == text_bank_.copy && count == text_bank_.count + kPickupTextCount)
        return true;
    if(text_bank_.copy) return false;
    if(count <= 0 || count > kMaxTextCount || !valid(bank,std::size_t(count)*sizeof(TextRecord))) return false;

    int first_id = kFirstPickupTextId;
    const auto occupied = [&](int id) {
        for(int i = 0; i < count; ++i)
            if(read<int>(bank + unsigned(i)*sizeof(TextRecord) + kTextIdOffset) == id) return true;
        return false;
    };
    while(first_id < kPickupTextIdLimit && (occupied(first_id) || occupied(first_id+1)))
        first_id += kPickupTextCount;
    if(first_id >= kPickupTextIdLimit) return false;

    // These are ordinary native printf-style pickup messages, not an overlay.
    // Native localization chooses the surrounding message styling and timing.
    const auto& strings=localization::strings_for_language(read<int>(global(localization::native_language_address)));
    const char* plural = strings.pickup_plural;
    const char* singular = strings.pickup_singular;
    const unsigned original_records_size = unsigned(count)*sizeof(TextRecord);
    const unsigned records_size = original_records_size+kPickupTextCount*sizeof(TextRecord);
    const unsigned plural_size = unsigned(std::strlen(plural))+1;
    const unsigned singular_size = unsigned(std::strlen(singular))+1;
    const unsigned strings_size = plural_size+singular_size;
    const unsigned copy = allocate_guest(records_size+strings_size);
    if(!valid(copy,records_size+strings_size)) return false;
    std::memcpy(api_->memory+copy,api_->memory+bank,original_records_size);
    const unsigned plural_at = copy+records_size;
    const unsigned singular_at = plural_at+plural_size;
    std::memcpy(api_->memory+plural_at,plural,plural_size);
    std::memcpy(api_->memory+singular_at,singular,singular_size);
    const TextRecord plural_record{plural_at,unsigned(first_id),0,0};
    const TextRecord singular_record{singular_at,unsigned(first_id+1),0,0};
    write(copy+original_records_size,plural_record);
    write(copy+original_records_size+sizeof(TextRecord),singular_record);

    TextBank lease;
    lease.original = bank; lease.copy = copy; lease.count = count;
    lease.snapshot.resize(unsigned(count));
    std::memcpy(lease.snapshot.data(),api_->memory+bank,original_records_size);
    text_bank_ = std::move(lease);
    plural_id_ = first_id; singular_id_ = first_id+1;
    write(bank_address,copy);
    write(count_address,count+kPickupTextCount);
    return true;
}

bool NativeAmmunition::patch_metadata() {
    const unsigned vendor = level(vendor_item_info)+item_id*vendor_item_info_size;
    const unsigned definitions = level(level_item_definitions)+item_id*item_definition_size;
    if(!valid(vendor,vendor_item_info_size) || !valid(definitions,item_definition_size) ||
       read<int>(definitions+item_definition_class) != int(NativeWeaponAlias::source_class)) return false;
    const bool custom_messages=extend_text_bank();

    // Nonzero unit prices enable ammo in the native inventory, HUD and vendor.
    const std::pair<unsigned,std::uint16_t> table_fields[] = {
        {ammo_unit_price,unit_price},
        {challenge_ammo_unit_price,unit_price},
        {ammo_pickup_amount,pickup},
        {ammo_capacity,capacity},
        {initial_ammo,capacity}
    };
    for(const auto& field:table_fields)
        if(!patch16(vendor+field.first,field.second)) return false;

    const unsigned resident = global(resident_item_definitions)+item_id*item_definition_size;
    for(unsigned definition:{definitions,resident}) {
        if(!valid(definition,item_definition_size) ||
           read<int>(definition+item_definition_class) != int(NativeWeaponAlias::source_class)) continue;
        if(!patch16(definition+ammo_pickup_class,native_pickup_class)) return false;
        if(custom_messages&&(!patch16(definition+pickup_plural_text,std::uint16_t(plural_id_)) ||
           !patch16(definition+pickup_singular_text,std::uint16_t(singular_id_)))) return false;
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

NativeAmmunition::AttackResult NativeAmmunition::begin_attack() {
    frame();
    if(!in_level() || !metadata_ready_) return AttackResult::Blocked;
    const unsigned hero_address = global(hero);
    if(read<int>(hero_address+held_item) != item_id ||
       read<int>(hero_address+held_item_state) != held_item_ready ||
       read<int>(hero_address+hero_health) <= 0 || read<int>(level(game_mode)) != gameplay_mode) return AttackResult::Blocked;
    if(take_ammo_(item_id,1)) return AttackResult::Spent;
    return count()==0 ? AttackResult::Empty : AttackResult::Blocked;
}

int NativeAmmunition::count() const {
    return in_level() ? std::max(0,read<int>(global(weapon_ammo_counts)+item_id*sizeof(int))) : 0;
}

int NativeAmmunition::maximum() const {
    return in_level() && metadata_ready_
        ? int(read<std::uint16_t>(level(vendor_item_info)+item_id*vendor_item_info_size+ammo_capacity)) : 0;
}

bool NativeAmmunition::pickup_valid(const Pickup& lease) const {
    return in_level() && valid(lease.address,moby_size) &&
        read<unsigned>(lease.address+moby_pvars) == lease.pvars &&
        read<unsigned>(lease.address+moby_spawn_frame) == lease.stamp &&
        valid(lease.pvars,kPickupPvarsSize) && read<int>(lease.pvars+kPickupItem) == item_id;
}

NativeAmmunition::Pickup* NativeAmmunition::find_pickup(unsigned moby) {
    const auto found = std::find_if(pickups_.begin(),pickups_.end(),
        [moby](const Pickup& lease){ return lease.address == moby; });
    return found == pickups_.end() ? nullptr : &*found;
}

void NativeAmmunition::before_update(unsigned moby) {
    if(!in_level() || !metadata_ready_ || !valid(moby,moby_size)) return;
    const auto state = read<std::uint8_t>(moby+moby_state);
    const auto klass = read<std::uint16_t>(moby+moby_class);
    const auto vars = read<unsigned>(moby+moby_pvars);
    if(state >= kFirstInactivePickupState || !valid(vars,kPickupPvarsSize) || read<int>(vars+kPickupItem) != item_id ||
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
        if(pickups_.size() >= kMaxTrackedPickups) return;
        pickups_.push_back({moby,vars,read<unsigned>(moby+moby_spawn_frame),native_pickup_class});
    }
    write<std::uint16_t>(moby+moby_class,native_pickup_class);
}

void NativeAmmunition::after_update(unsigned moby) {
    auto* lease = find_pickup(moby);
    if(!lease) return;
    if(!pickup_valid(*lease) || read<std::uint8_t>(moby+moby_state) >= kFirstInactivePickupState) {
        pickups_.erase(std::remove_if(pickups_.begin(),pickups_.end(),
            [moby](const Pickup& p){return p.address == moby;}),pickups_.end());
        return;
    }
    if(read<std::uint16_t>(moby+moby_class) == native_pickup_class)
        write<std::uint16_t>(moby+moby_class,visual_class);
}

void NativeAmmunition::restore_text_bank() {
    if(!text_bank_.copy || read<unsigned>(global(kTextBank)) != text_bank_.copy ||
       read<int>(global(kTextCount)) != text_bank_.count+kPickupTextCount) return;
    // Preserve another mod's edits made to original entries of the extended
    // bank, unless that mod independently changed the original record too.
    for(int i = 0; i < text_bank_.count; ++i) {
        const unsigned old_at = text_bank_.original+unsigned(i)*sizeof(TextRecord);
        const unsigned current_at = text_bank_.copy+unsigned(i)*sizeof(TextRecord);
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
            if(pickup_valid(lease) && read<std::uint16_t>(lease.address+moby_class) == visual_class)
                write(lease.address+moby_class,lease.original_class);
        for(auto it = patches_.rbegin(); it != patches_.rend(); ++it)
            if(read<std::uint16_t>(it->address) == it->replacement) write(it->address,it->original);
        restore_text_bank();
    }
    pickups_.clear(); patches_.clear(); text_bank_={};
    level_=-1; metadata_ready_=false;plural_id_=singular_id_=0;
}
}
