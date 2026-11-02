// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "pal_font_advances.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>
#include <string_view>
namespace salvage::presentation {
enum class Font {Normal,Small};
struct Caption {std::string text;Font font=Font::Normal;};
// Coordinates are native font pixels. These functions own no SDK/game state.
inline int text_width(std::string_view value,Font font){const auto& table=font==Font::Small?small_advances:normal_advances;int width=0,longest=0;for(const unsigned char c:value){if(c==1){longest=std::max(longest,width);width=0;}else if(c>=2)width+=c<table.size()?table[c]:30;}return std::max(longest,width);}
inline std::string ellipsize(std::string_view value,int available,Font font){if(available<=0)return {};if(text_width(value,font)<=available)return std::string(value);constexpr std::string_view dots="...";const int reserve=text_width(dots,font);if(reserve>available)return {};std::size_t end=0;int width=0;while(end<value.size()){const unsigned char c=value[end];const int next=text_width(value.substr(end,1),font);if(c==1||width+next+reserve>available)break;width+=next;++end;}while(end&&end<value.size()&&(static_cast<unsigned char>(value[end])&0xc0u)==0x80u)--end;while(end&&value[end-1]==' ')--end;return std::string(value.substr(0,end))+std::string(dots);}
inline Caption fit_line(std::string_view value,int available){if(text_width(value,Font::Normal)<=available)return {std::string(value),Font::Normal};return {ellipsize(value,available,Font::Small),Font::Small};}
inline Caption list_caption(std::string_view name,std::uint64_t quantity,int available){const auto suffix="  x"+std::to_string(quantity),full=std::string(name)+suffix;if(text_width(full,Font::Normal)<=available)return {full,Font::Normal};if(text_width(full,Font::Small)<=available)return {full,Font::Small};const Font font=Font::Small;const int suffix_budget=std::max(0,available/3);const auto badge=ellipsize(suffix,suffix_budget,font);return {ellipsize(name,available-text_width(badge,font),font)+badge,font};}
inline Caption name_caption(std::string_view value,int available,int height){if(text_width(value,Font::Normal)<=available)return {std::string(value),Font::Normal};if(text_width(value,Font::Small)<=available)return {std::string(value),Font::Small};if(height<32)return {ellipsize(value,available,Font::Small),Font::Small};
 std::size_t split=std::string_view::npos;int imbalance=std::numeric_limits<int>::max();for(std::size_t i=0;i<value.size();++i)if(value[i]==' '){const int left=text_width(value.substr(0,i),Font::Small),right=text_width(value.substr(i+1),Font::Small);if(left<=available&&right<=available&&std::abs(left-right)<imbalance){split=i;imbalance=std::abs(left-right);}}
 if(split!=std::string_view::npos)return {std::string(value.substr(0,split))+"\1"+std::string(value.substr(split+1)),Font::Small};
 std::size_t first=0;for(std::size_t i=0;i<value.size();++i)if(value[i]==' '&&text_width(value.substr(0,i),Font::Small)<=available)first=i;if(!first)return {ellipsize(value,available,Font::Small),Font::Small};return {std::string(value.substr(0,first))+"\1"+ellipsize(value.substr(first+1),available,Font::Small),Font::Small};
}
inline std::string exact_quantity(std::uint64_t value){auto text=std::to_string(value);for(std::size_t i=text.size();i>3;){i-=3;text.insert(i,1,' ');}return text;}
}
