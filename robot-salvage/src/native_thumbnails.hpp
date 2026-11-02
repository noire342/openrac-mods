// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core.hpp"
#include "native_memory.hpp"
#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
namespace salvage {
struct ThumbnailBitmap {
 static constexpr unsigned dimension=128;
 std::array<unsigned char,dimension*dimension> pixels{};
 // Native PSMT8 CSM1-swizzled CLUT, RGBA with PS2 alpha0..128.
 std::array<unsigned char,1024> palette{};
 std::uint64_t source_hash{};
};
// Actual textured owned-disc geometry, rasterized by the CPU without Blender,
// a network service, model substitution or generated artwork.
bool render_thumbnail(const std::filesystem::path& glb,ThumbnailBitmap&,std::string& error);
bool write_thumbnail_png(const std::filesystem::path&,const ThumbnailBitmap&,std::string& error);
// Arguments exclude the executable. Match the official runtime's content root,
// including its legacy flat PAL layout; never guess an unrelated installation.
std::filesystem::path thumbnail_levels(const std::vector<std::filesystem::path>& arguments,
 std::string_view version,const std::filesystem::path& environment_data={});
class NativeThumbnails final {
public:
 NativeThumbnails(const openrac_api*,GuestArena&,std::filesystem::path cache);
 NativeThumbnails(const openrac_api*,GuestArena&,std::filesystem::path levels,std::filesystem::path cache);
 ~NativeThumbnails();
 NativeThumbnails(const NativeThumbnails&)=delete;
 NativeThumbnails& operator=(const NativeThumbnails&)=delete;
 bool draw(unsigned screen,const PartDescriptor&);
 void reset();
 const std::string& last_error()const noexcept;
private:
 struct State;std::unique_ptr<State> state_;
};
}
