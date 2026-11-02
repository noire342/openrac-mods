// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core.hpp"
#include "native_memory.hpp"
#include "../../arsenal-compat/src/native_pause_menu.hpp"
#include "../../arsenal-compat/include/arsenal_pause_api.h"
#include <array>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace salvage {
// Owns a new native pause page, not the stock Weapons/Items/Goodies contents.
// The parent routes only owns_callback() guest arguments through invoke_callback()
// in its ordinary level-loader hook. No generated guest executable code is used.
class NativePresentation final:private NativeMemory {
public:
 using Thumbnail=std::function<void(unsigned,const PartDescriptor&)>;
 NativePresentation(const openrac_api*,openrac_mod*,GuestArena&,const Catalog&,const Inventory&);
 ~NativePresentation();
 void frame();
 void reset();
 void set_pause_service(const arsenal_pause_api_v1*);
 void set_thumbnail_renderer(Thumbnail renderer){thumbnail_=std::move(renderer);}
 void set_candidate(PartId id,std::optional<Vec3> position={}){candidate_=id;candidate_position_=id?position:std::optional<Vec3>{};}
 // Register only at the native first draw-list dispatch boundary, before the
 // original dispatcher. on_frame is after rendering and is too late to queue.
 bool queue_world_prompt();
 void use_native_hud_boundary(bool enabled){native_hud_boundary_=enabled;if(enabled)remove_world_prompt();}
 void draw_hud_prompt();
 void before_pause_render();
 void after_pause_render();
 void notify_collected(PartId,Quantity amount=1);
 bool owns_callback(unsigned argument)const;
 unsigned invoke_callback(unsigned argument);
 bool inventory_open()const;
 bool native_interaction_active()const;
 // Called for every physical Triangle report, including releases. Returning
 // true claims the whole press so returning to the root cannot create another
 // native press edge from the same held button.
 bool route_triangle(bool down);
 bool ready()const{return page_!=0;}
 unsigned page_address()const{return page_;}
 unsigned entry_address()const{return entry_;}
 std::size_t selection()const{return selected_;}
private:
 enum class Role {Heading,List,Thumbnail,Total,Details,Name,Quantity,Blank,Controls};
 struct Screen {unsigned address{};Role role=Role::Blank;};
 const openrac_api* api()const{return api_;}
 openrac_mod* mod_{};GuestArena& arena_;const Catalog& catalog_;const Inventory& inventory_;
 Thumbnail thumbnail_;
 arsenal::pause::NativePauseMenu pause_menu_;
 const arsenal_pause_api_v1* pause_service_{};
 arsenal_pause_owner_v1 pause_owner_{};arsenal_pause_entry_token_v1 pause_token_{};arsenal_pause_presenter_v1 pause_presenter_{};
 std::uint64_t pause_context_{},pause_revision_=~std::uint64_t(0);
 int level_=-1;unsigned root_{},page_{},entry_{},relay_{},message_{},world_prompt_{},tick_=~0u,notified_tick_=~0u;
 unsigned confirmation_tick_=~0u;
 unsigned pause_render_calls_{},pause_render_logged_{},world_prompt_calls_{},world_prompt_logged_{},world_prompt_rejects_{},world_registered_tick_=~0u;bool native_hud_boundary_=false;
 bool triangle_down_=false,triangle_claimed_=false;
 PartId candidate_{};
 std::optional<Vec3> candidate_position_;
 std::size_t selected_{};
 std::string last_message_;
 std::vector<Screen> screens_;
 bool bind();
 bool italian()const;
 std::vector<std::pair<PartId,Quantity>> rows()const;
 void text(unsigned,const std::string&,bool small=true,bool center=false,int top=4,int bottom=-1,unsigned color=0x80ffa888u,int inset=8);
 void draw_list(unsigned);
 bool message(const std::string&,int frames);
 bool world_prompt_available()const;
 void draw_world_prompt();
 void remove_world_prompt();
 void sync_pause_entries();
 void release_pause_service();
 static std::int32_t open_components(std::uint64_t,std::uint64_t);
 void open_from_menu();
};
}
