// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "overlay_core.hpp"
#include "client_profile.hpp"
#include <cstdlib>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace file_overlay;
namespace {
unsigned checks{};
#define CHECK(x) do { ++checks; if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } } while(false)
void rejects(const std::function<void()>& function) { try { function(); } catch (const std::runtime_error&) { ++checks; return; } CHECK(false); }
std::string config(unsigned count = 19, const std::string& original = "C:/game/level", const std::string& replacement = "D:/staged/level") {
    std::string text = "{\"format\":1,\"aliases\":[";
    for (unsigned i = 0; i < count; ++i) {
        if (i) text += ',';
        text += "{\"original\":\"" + original + std::to_string(i) + "/manifest.json\",\"replacement\":\"" + replacement + std::to_string(i) + "/manifest.json\"}";
    }
    return text + "]}";
}
void parsing() {
    const auto good = config(); const auto aliases = parse_config(good);
    CHECK(aliases.size() == 19); CHECK(aliases.front().original == "C:/game/level0/manifest.json"); CHECK(aliases.back().replacement == "D:/staged/level18/manifest.json");
    CHECK(parse_config("\xef\xbb\xbf" + good).size() == 19);
    CHECK(parse_config(" \r\n\t" + good + "\n").size() == 19);
    for (unsigned count : {0u, 1u, 18u, 20u, 100u}) rejects([&] { parse_config(config(count)); });
    for (std::size_t i = 0; i < good.size(); ++i) rejects([&] { parse_config(std::string_view(good).substr(0, i)); });
    rejects([&] { parse_config(good + "garbage"); });
    auto bad = good; bad.replace(bad.find("\"format\":1"), 10, "\"format\":2"); rejects([&] { parse_config(bad); });
    bad = good; bad.replace(bad.find("\"format\":1"), 10, "\"format\":10"); rejects([&] { parse_config(bad); });
    bad = good; bad.replace(bad.find("\"format\":1"), 10, "\"format\":1,\"format\":1"); rejects([&] { parse_config(bad); });
    bad = good; bad.replace(bad.find("\"format\":1"), 10, "\"unknown\":1"); rejects([&] { parse_config(bad); });
    for (const std::string escaped : {"\\u0000", "\\uD800", "\\uDC00", "\\uD800\\u0020", "\\x00", "\\n"}) rejects([&] { parse_config(config(19, "C:/" + escaped)); });
    const auto unicode = parse_config(config(19, "C:/\\u00e9/\\uD83D\\uDE80/"));
    CHECK(unicode[0].original.find("\xc3\xa9/\xf0\x9f\x9a\x80") != std::string::npos);
    rejects([&] { parse_config(std::string(MaximumConfigBytes + 1, ' ')); });
}
void policy() {
    constexpr unsigned read = 0x80000000u;
    CHECK(read_only_open(read, 3, 0)); CHECK(read_only_open(1, 3, 0));
    CHECK(read_only_open(read | 0x100000 | 0x80 | 8 | 0x20000, 3, 0x08000000));
    CHECK(!read_only_open(0, 3, 0)); CHECK(!read_only_open(0x80, 3, 0));
    for (unsigned disposition = 0; disposition <= 8; ++disposition) CHECK(read_only_open(read, disposition, 0) == (disposition == 3));
    for (unsigned write_bit : {0x40000000u,0x10000000u,2u,4u,16u,256u,0x10000u,0x40000u,0x80000u,0x20000000u}) CHECK(!read_only_open(read | write_bit, 3, 0));
    CHECK(!read_only_open(read, 3, 0x04000000));
}
std::vector<PathPair> pairs() {
    std::vector<PathPair> out;
    for (unsigned i = 0; i < 19; ++i) out.push_back({L"c:\\game\\level" + std::to_wstring(i) + L"\\manifest.json", L"d:\\staged\\level" + std::to_wstring(i) + L"\\manifest.json"});
    return out;
}
void immutable_map() {
    const PathMap map(pairs()); CHECK(map.size() == 19);
    for (unsigned i = 0; i < 19; ++i) {
        const auto* replacement = map.find(L"c:\\game\\level" + std::to_wstring(i) + L"\\manifest.json");
        CHECK(replacement); CHECK(*replacement == L"d:\\staged\\level" + std::to_wstring(i) + L"\\manifest.json");
    }
    CHECK(!map.find(L"c:\\game\\level1\\manifest.json.bak"));
    CHECK(!map.find(L"c:\\game\\other\\manifest.json"));
    CHECK(!map.find(L"c:\\game\\level1\\mesh.bin"));
    auto bad = pairs(); bad[5].normalized_original = bad[0].normalized_original; rejects([&] { PathMap duplicate(bad); });
    bad = pairs(); bad[0].replacement = bad[0].normalized_original; rejects([&] { PathMap self(bad); });
    bad = pairs(); bad[0].replacement.clear(); rejects([&] { PathMap empty(bad); });
    bad = pairs(); bad.pop_back(); rejects([&] { PathMap short_map(bad); });
}
void client_profiles() {
    const char* ntsc09="b7f3762aa7076f0d93cad4033b49b4e966975a17885a05d70f61c9fe8df94081";
    const char* ntsc10="f70a9f2bc6de39ed42b9f9f64fc5cea98ffad8b8bd7795c7adfb965a9875c95b";
    const char* pal10="e720a59d130b4b2289a4eb15cad3e33f739a72a385992db171ef43490205fb0e";
    for(const auto digest:{ntsc09,ntsc10,pal10})CHECK(supported_client(digest));
    CHECK(supported_client(ntsc09,"ntsc"));CHECK(supported_client(ntsc10,"ntsc"));CHECK(supported_client(pal10,"pal"));
    CHECK(!supported_client(ntsc09,"pal"));CHECK(!supported_client(ntsc10,"pal"));CHECK(!supported_client(pal10,"ntsc"));
    for(const auto digest:{ntsc09,ntsc10,pal10}){
        CHECK(!supported_client(digest,""));CHECK(!supported_client(digest,"unknown"));
        std::string changed(digest);changed[0]='0';CHECK(!supported_client(changed));
        CHECK(!supported_client(std::string(digest)+"0"));CHECK(!supported_client(std::string(digest).substr(1)));
    }
    CHECK(!supported_client(""));CHECK(!supported_client(std::string(64,'0')));
}
}
int main() { parsing(); policy(); immutable_map(); client_profiles(); std::cout << checks << " file-overlay host checks passed\n"; }
