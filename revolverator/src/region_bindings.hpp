// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once

namespace revolverator {

enum class Region { Ntsc, Pal };
struct MemcardLayout {
    unsigned card_stride,port,slot,type,free,format,save_index,scan_save_index,sync_result;
    unsigned card_errors,main_size,record_size;
    unsigned command,result,active_card,current_card,record_index,busy,file_descriptor;
    unsigned operation,substep,pending_operation,pending_card,error,error_card,buffer,bytes,pending_mask;
};

// Disc-specific names and canonical addresses, before the SDK resolves the
// loaded program's copies. Never pass an already-resolved guest pointer here.
class RegionBindings final {
public:
    static const RegionBindings* for_version(const char* version) noexcept;
    static const RegionBindings& ntsc() noexcept;
    static const RegionBindings& pal() noexcept;

    Region region() const noexcept { return region_; }
    const char* version() const noexcept;
    const char* card_serial() const noexcept;
    const char* card_directory() const noexcept;
    unsigned animation_transfer_offset() const noexcept;
    float nominal_hz() const noexcept;
    const MemcardLayout& memcard() const noexcept;

    // NTSC canonical addresses are explicit migration keys. PAL rejects an
    // unknown key instead of treating it as an address in the other disc.
    unsigned data(unsigned ntsc_canonical) const;
    unsigned level(unsigned overlay, unsigned ntsc_canonical) const;
    unsigned callback(unsigned overlay, unsigned ntsc_canonical) const;

    // Stable semantic names and PAL names pass through. A region-sensitive
    // FUN_ name without an approved PAL counterpart returns nullptr.
    const char* function(const char* ntsc_name) const noexcept;

private:
    constexpr explicit RegionBindings(Region region) : region_(region) {}
    Region region_;
};

} // namespace revolverator
