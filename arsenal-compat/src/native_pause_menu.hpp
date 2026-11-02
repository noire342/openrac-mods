// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "pause_menu_model.hpp"
#include <openrac/openrac_mod.h>
#include <array>
#include <cstring>
#include <functional>
#include <optional>
#include <string>

namespace arsenal::pause {
// Native presentation only: no inventory, catalog or game-domain dependency.
// Producers supply copied labels and a synchronous activation operation. The
// allocator must reserve guest storage until reset, including every callback.
class NativePauseMenu final {
public:
 struct Item {Entry entry;std::string label_it,label_en;std::function<void(EntryId)> activate;};
 using Allocate=std::function<unsigned(std::size_t)>;
 using Compose=std::function<Status(const std::vector<Entry>&,EntryId,Snapshot&)>;
 static constexpr std::size_t MaxCustom=32,DirectoryPageSize=7;
 static constexpr EntryId stock_id(unsigned i){return EntryId(i)+1;}
 NativePauseMenu(const openrac_api*,Allocate,Compose={});
 ~NativePauseMenu(){reset();}
 Status set_entries(std::vector<Item>);
 void set_composer(Compose compose){compose_=std::move(compose);}
 bool bind(unsigned callback_relay);
 bool frame();
 void before_render();
 void after_render();
 // The official frontend reads live preview actors after the native renderer
 // returns. Call only after that capture (SDK on_frame), before the next tick.
 void after_capture();
 std::size_t borrowed_render_rows()const{return render_leases_.size();}
 void reset(bool restore=true);
 bool owns_callback(unsigned)const;
 unsigned invoke_callback(unsigned);
 unsigned root_address()const{return root_;}
 unsigned weapon_page_address()const{return prototype_;}
 unsigned entry_address()const{return widgets_[0].screen;}
 unsigned directory_address()const{return directory_;}
 bool directory_open()const;
 const Snapshot& snapshot()const{return snapshot_;}
private:
 struct Patch {unsigned at{},original{},applied{};};
 struct Widget {unsigned screen{},items{};EntryId id{};bool footer=false;};
 struct Quad {unsigned actor{},vars{};std::array<float,16> original{},applied{};bool active=false;};
 const openrac_api* api_;Allocate allocate_;Compose compose_;Model model_;
 int level_=-1;unsigned root_{},prototype_{},directory_{},relay_{},activation_tick_=~0u,layout_tick_=~0u,last_menu_tick_=~0u;
 unsigned original_extension_animation_{};bool has_visible_custom_=false,extension_reappear_pending_=false;
 std::size_t directory_page_{};
 std::vector<Item> items_;Snapshot snapshot_;
 std::array<unsigned,7> stock_{};
 std::array<Widget,9> widgets_{}; // root group plus eight directory rows
 std::vector<Patch> patches_;std::array<Quad,14> quads_{};
 struct RenderLease {unsigned actor{},vars{},klass{};std::array<float,4> position{},sphere{};std::array<float,12> basis{};std::array<float,16> corners{};std::array<unsigned char,36> animation{};std::array<unsigned char,96> variables{};float scale{};};
 std::vector<RenderLease> render_leases_;
 struct DeferredLease {RenderLease original,applied;};std::vector<DeferredLease> deferred_leases_;
 struct VisibilityLease {RenderLease identity;unsigned short original{},applied{};};std::vector<VisibilityLease> visibility_leases_,deferred_visibility_;
 std::array<std::optional<RenderLease>,14> root_pose_cache_{},root_render_candidates_{};
 std::optional<RenderLease> extension_pose_;
 bool directory_open_pending_=false;
 struct RenderFlag {unsigned address{};int original{},applied{};};std::vector<RenderFlag> render_flags_;
 static bool valid(unsigned,std::size_t);
 template<class T>T read(unsigned at)const{T v{};if(valid(at,sizeof v))std::memcpy(&v,api_->memory+at,sizeof v);return v;}
 template<class T>void write(unsigned at,const T& v)const{if(valid(at,sizeof v))std::memcpy(api_->memory+at,&v,sizeof v);}
 unsigned g(unsigned)const;bool italian()const;
 bool discover();bool known_screen(unsigned)const;bool lease(unsigned,unsigned);
 void restore_layout();void layout(const std::vector<unsigned>& slots);
 const Item* item(EntryId)const;
 std::vector<EntryId> custom_visible()const;
 void directory_rows(const std::vector<EntryId>&);
 void text(unsigned,const std::string&);
 RenderLease pose(unsigned actor)const;
 bool same_instance(const RenderLease&)const;
 bool root_cache_ready()const;
 void apply_portable_pose(const RenderLease&,unsigned actor,unsigned vars);
};
}
