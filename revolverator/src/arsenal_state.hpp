// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace arsenal {
using WeaponKey=std::uint32_t;
inline constexpr WeaponKey NativeWalloperKey=18;
inline constexpr WeaponKey RevolveratorKey=0x10012;
inline constexpr WeaponKey NativeWrenchKey=8;

struct Descriptor {
    WeaponKey key{};
    std::string stable_id;
    std::uint32_t carrier{};
    std::uint32_t icon_bank{};
    std::uint32_t capacity{};
    std::uint32_t price{};
    std::uint32_t first_level{};
};
struct WalloperNativeState {
    bool owned=false;
    // unlocked is reserved in the v1/v2 codec. Native adapters neither read
    // nor apply this legacy field; the original address was a skill point.
    std::uint8_t unlocked{},acquired{};
    int ammo{};
    std::uint8_t text_variant{};
};
struct State {
    bool owned=false;
    std::uint32_t ammo{},xp{};
    bool golden=false;
};
enum class GoldenPurchaseResult { Purchased, AlreadyOwned, BaseMissing, InsufficientBolts, InvalidPrice, UnknownWeapon };
using CustomState=State;
struct Record {
    WeaponKey key{};
    std::string stable_id;
    State state;
};
struct Snapshot {
    std::uint32_t format=1;
    WalloperNativeState walloper;
    std::vector<Record> records;
    std::array<WeaponKey,8> quick{};
    WeaponKey equipped=NativeWrenchKey;
};

// Pure logical inventory. It never indexes or writes native item arrays. The
// platform adapter checks ownership for native items other than Walloper before
// calling equip/assign_quick; those items remain the game's responsibility.
class Registry final {
public:
    static constexpr std::size_t MaxDescriptors=64,MaxSnapshotRecords=256;
    static constexpr std::uint32_t MaximumXp=1000000000;
    static constexpr std::uint32_t MaximumCapacity=65535;
    static constexpr WeaponKey MaximumNativeKey=36,FirstCustomKey=0x10000;

    bool register_weapon(const Descriptor& descriptor);
    void seal();
    bool sealed()const{return sealed_;}
    const std::vector<Descriptor>& definitions()const{return descriptors_;}
    const std::vector<Descriptor>& descriptors()const{return descriptors_;}
    const Descriptor* descriptor(WeaponKey key)const;
    const State* custom_state(WeaponKey key)const;
    const State* state(WeaponKey key)const{return custom_state(key);}
    const WalloperNativeState& walloper()const{return walloper_;}
    bool set_walloper(const WalloperNativeState& state);
    bool set_state(WeaponKey key,const State& state);
    bool set_custom_state(WeaponKey key,const State& state){return set_state(key,state);}
    bool golden(WeaponKey key)const;
    // Changes only this custom weapon's upgrade bit and the caller's balance.
    // No native Gold flag/counter, ammunition, XP or binding is modified.
    GoldenPurchaseResult purchase_golden(WeaponKey key,int& bolts,std::uint32_t price);

    // owned() covers our custom records, Walloper and the permanent wrench.
    // Other native ownership must be read from the native inventory by its owner.
    bool owned(WeaponKey key)const;
    bool equip(WeaponKey key);
    WeaponKey equipped()const;
    // Moves an existing binding of the same logical key; two different logical
    // weapons may deliberately have the same projected native carrier.
    bool assign_quick(std::size_t slot,WeaponKey key);
    const std::array<WeaponKey,8>& quick()const{return quick_;}
    const std::array<WeaponKey,8>& quick_bindings()const{return quick_;}
    std::array<std::uint32_t,8> native_slots()const;
    std::uint64_t revision()const{return revision_;}

    Snapshot snapshot()const;
    // Validation is atomic. Missing known records start unowned. Unknown custom
    // records and their logical favourite positions survive removal/reinstall;
    // they project to empty native slots and cannot be equipped while absent.
    bool restore(const Snapshot& snapshot);
private:
    std::vector<Descriptor> descriptors_;
    std::vector<Record> records_;
    WalloperNativeState walloper_;
    std::array<WeaponKey,8> quick_{};
    WeaponKey selected_=NativeWrenchKey;
    std::uint64_t revision_{};
    bool sealed_=false;
    bool can_equip(WeaponKey key)const;
    void unbind(WeaponKey key);
};
}
