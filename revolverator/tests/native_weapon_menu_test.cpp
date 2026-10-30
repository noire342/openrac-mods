// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_weapon_menu.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace revolverator;
using namespace arsenal;
namespace {
int checks;
void require(bool condition,const char* message){++checks;if(!condition){std::fprintf(stderr,"weapon menu: %s\n",message);std::exit(1);}}
constexpr unsigned Page=0x1e00000,NativeOwned=0x13d4c0,AlternateOwned=0x13d388;
constexpr unsigned Pad=0x13c940+0x1c4,System=NativeWeaponMenu::MenuSystem;
constexpr unsigned Cells=NativeWeaponMenu::WeaponCells,Weapon=NativeWeaponMenu::WeaponGrid;
constexpr unsigned Quick=NativeWeaponMenu::QuickWeaponGrid,Gadgets=NativeWeaponMenu::QuickGadgetGrid,Ring=NativeWeaponMenu::QuickRing;
constexpr unsigned UpdateCode=0x210000,DrawCode=0x210100;
constexpr std::array<int,15> NativeIds{{10,16,15,20,17,14,9,11,18,13,25,24,19,21,23}};
struct Fixture;Fixture* live;
struct Draw {int frame,x,y,w,h,alpha;};
struct Invocation {unsigned buttons;WeaponKey projected;};
struct Fixture:ArsenalMenuModel {
    std::vector<unsigned char> memory=std::vector<unsigned char>(0x2000000);
    openrac_api api{};GuestArena arena;Registry state;int current_level=1;
    WeaponKey view=NativeWalloperKey;
    int commits{},restores{},native_confirms{},native_slot_assigns{},ring_calls{},paging_calls{},finish_calls{},hover_sounds{};
    unsigned heap=0x1800000;bool fail_allocate=false;
    std::vector<Invocation> updates;
    std::vector<Draw> sprites;
    std::array<int,8> ring_seen{};
    WeaponKey ring_view{};
    float drawn_icon_width{},drawn_icon_height{};
    std::vector<int> visible_ids;
    template<class T>void put(unsigned at,const T& value){std::memcpy(memory.data()+at,&value,sizeof value);}
    template<class T>T get(unsigned at)const{T v{};std::memcpy(&v,memory.data()+at,sizeof v);return v;}
    Registry& registry()override{return state;}
    void project(WeaponKey key)override{
        view=key;const bool custom=state.descriptor(key)!=nullptr;
        const bool own=custom?state.owned(key):state.walloper().owned;
        put<std::uint8_t>(NativeOwned+18,own?1:0);
    }
    void commit()override{++commits;}
    void restore_gameplay()override{++restores;view=state.equipped();put<std::uint8_t>(NativeOwned+18,state.walloper().owned?1:0);}
    static unsigned allocate(unsigned n){if(live->fail_allocate)return 0;const auto at=live->heap;live->heap+=(n+15)&~15u;return at;}
    static int update(unsigned screen){
        const unsigned buttons=live->get<unsigned>(Pad);
        live->updates.push_back({buttons,live->view});
        if(live->get<unsigned>(Page+0x40)!=screen)return 0;
        if(buttons&0xd00)return 1;
        int selected=live->get<int>(screen+0x3c),rows=live->get<int>(screen+0x40),cols=live->get<int>(screen+0x44);
        int row=selected/cols,column=selected%cols;
        if(buttons&0x1000){
            if(row>0)selected-=cols;
            else if(const auto next=live->get<unsigned>(screen+0x4c)){
                live->put(Page+0x80,next);const int nr=live->get<int>(next+0x40),nc=live->get<int>(next+0x44);
                live->put(next+0x3c,(nr-1)*nc+std::min(column,nc-1));
            }else selected=(rows-1)*cols+column;
        }
        if(buttons&0x4000){
            if(row+1<rows)selected+=cols;
            else if(const auto next=live->get<unsigned>(screen+0x50)){
                live->put(Page+0x80,next);const int nc=live->get<int>(next+0x44);live->put(next+0x3c,std::min(column,nc-1));
            }else selected=column;
        }
        if(buttons&0x8000)selected=column?selected-1:selected+cols-1;
        if(buttons&0x2000)selected=column+1<cols?selected+1:selected-cols+1;
        live->put(screen+0x3c,selected);
        if(buttons&0x40){
            const auto cell=live->get<WeaponMenuCell>(live->get<unsigned>(screen+0x48)+unsigned(selected)*10);
            require(cell.id>=0&&cell.id<=36,"a native confirm never receives a logical key");
            if(cell.id&&live->get<std::uint8_t>(NativeOwned+unsigned(cell.id))){
                live->put(System+0x30,int(cell.id));++live->native_confirms;
            }
        }
        return 0;
    }
    static int draw(unsigned screen){
        live->visible_ids.clear();
        live->drawn_icon_width=live->get<float>(0x160290);live->drawn_icon_height=live->get<float>(0x160294);
        const int count=live->get<int>(screen+0x40)*live->get<int>(screen+0x44);
        const auto cells=live->get<unsigned>(screen+0x48);
        for(int i=0;i<count;++i){
            const auto cell=live->get<WeaponMenuCell>(cells+unsigned(i)*10);
            require(cell.id>=0&&cell.id<=36,"a native draw never receives a logical item index");
            const bool own=live->get<std::uint8_t>((cell.kind?AlternateOwned:NativeOwned)+unsigned(cell.id))!=0;
            if(own)live->visible_ids.push_back(cell.id);
        }
        return 2;
    }
    static int slots(unsigned screen){
        const unsigned buttons=live->get<unsigned>(Pad);int slot=live->get<int>(screen+0x50);
        if(buttons&8)slot=(slot+1)%8;if(buttons&4)slot=(slot+7)%8;
        live->put(screen+0x50,slot);
        const auto focus=live->get<unsigned>(Page+0x40);const int selected=live->get<int>(focus+0x3c);
        const auto cell=live->get<WeaponMenuCell>(live->get<unsigned>(focus+0x48)+unsigned(selected)*10);
        if((buttons&0x40)&&cell.id&&live->get<std::uint8_t>(NativeOwned+unsigned(cell.id))){
            auto values=live->get<std::array<unsigned,8>>(screen+0x30);
            for(auto& id:values)if(id==unsigned(cell.id)){id=0;break;}
            values[unsigned(slot)]=unsigned(cell.id);live->put(screen+0x30,values);
            live->put(screen+0x50,(slot+1)%8);++live->native_slot_assigns;
        }
        return 0;
    }
    static int ring(unsigned screen){++live->ring_calls;live->ring_seen=live->get<std::array<int,8>>(screen+0x30);live->ring_view=live->view;return 2;}
    static int noop(unsigned){return 0;}
    static int find(int bank,int frame){return bank*8+frame;}
    static void sprite(int f,int x,int y,int w,int h,int a){live->sprites.push_back({f,x,y,w,h,a});}
    static void paging(int){++live->paging_calls;}
    static void finish(){++live->finish_calls;}
    static float cosine(float x){return std::cos(x);}
    static float sine(float x){return std::sin(x);}
    static float normalize(float x){constexpr float pi=3.1415927f;while(x>pi)x-=pi*2;while(x<-pi)x+=pi*2;return x;}
    static int sound(int,int,unsigned){++live->hover_sounds;return 0;}
    Fixture(){
        live=this;api.memory=memory.data();api.game="rac1";api.version="ntsc";
        api.loaded_level=+[]{return live->current_level;};api.relocate_data=+[](unsigned at){return at;};api.relocate_level_data=+[](int,unsigned at){return at;};
        api.function=+[](const char* name)->void*{
            if(!std::strcmp(name,"FUN_001ffc30"))return reinterpret_cast<void*>(&sprite);
            if(!std::strcmp(name,"FUN_001ff960"))return reinterpret_cast<void*>(&find);
            if(!std::strcmp(name,"FUN_001f4280"))return reinterpret_cast<void*>(&paging);
            if(!std::strcmp(name,"FUN_001f4398"))return reinterpret_cast<void*>(&finish);
            if(!std::strcmp(name,"FUN_001f9dc8"))return reinterpret_cast<void*>(&cosine);
            if(!std::strcmp(name,"FUN_001f9de0"))return reinterpret_cast<void*>(&sine);
            if(!std::strcmp(name,"FUN_001fa610"))return reinterpret_cast<void*>(&normalize);
            if(!std::strcmp(name,"FUN_0022da68"))return reinterpret_cast<void*>(&sound);
            if(!std::strcmp(name,"FUN_001ff288"))return reinterpret_cast<void*>(&allocate);
            if(!std::strcmp(name,"FUN_0021b858"))return reinterpret_cast<void*>(&update);
            if(!std::strcmp(name,"FUN_0021d948"))return reinterpret_cast<void*>(&draw);
            return nullptr;
        };
        api.function_at=+[](unsigned at)->void*{return at==UpdateCode?reinterpret_cast<void*>(&update):at==DrawCode?reinterpret_cast<void*>(&draw):nullptr;};
        arena.reserve(0x1000000,65536);
        state.register_weapon({RevolveratorKey,"revolverator",18,60037,10,15000,12});
        state.set_state(RevolveratorKey,{true,10,0});state.set_walloper({true,1,1,0,0});
        state.equip(NativeWalloperKey);
        std::array<WeaponMenuCell,15> cells{};
        for(unsigned i=0;i<cells.size();++i){cells[i]={std::uint16_t(60000+NativeIds[i]),0,0,std::int16_t(NativeIds[i]),0};put<std::uint8_t>(NativeOwned+unsigned(NativeIds[i]),1);}
        put(Cells,cells);
        for(unsigned address:{Weapon,Quick}){
            put(address,UpdateCode);put(address+4,DrawCode);put(address+0x40,3);put(address+0x44,5);put(address+0x48,Cells);put(address+0x30,0x20);
        }
        put(Quick+0x4c,Gadgets);put(Quick+0x50,Gadgets);
        put(Gadgets+0x40,2);put(Gadgets+0x44,3);put(Gadgets+0x4c,Quick);put(Gadgets+0x50,Quick);
        put(System+4,Page);put(Page+0x40,Weapon);
        put(Ring+0x20,512);put(Ring+0x24,416);
        put(0x160290,.125f);put(0x160294,.125f);
        put<std::uint8_t>(AlternateOwned,1);put<std::uint8_t>(AlternateOwned+18,7);
    }
};
}

int main(){
    {
        Fixture f;const auto source=f.get<std::array<WeaponMenuCell,15>>(Cells);
        NativeWeaponMenu menu(&f.api,f.arena,f);menu.frame();
        require(menu.ready()&&menu.catalog_size()==16,"the sixteenth weapon expands both native grids");
        require(f.get<int>(Weapon+0x40)==4&&f.get<int>(Quick+0x40)==4&&f.get<int>(Weapon+0x44)==5,"both existing panels use four rows and five columns");
        require(f.get<unsigned>(Weapon+0x48)!=Cells&&f.get<unsigned>(Quick+0x48)!=Cells,"cell arrays are cloned instead of overflowing adjacent native data");
        require(f.get<std::array<WeaponMenuCell,15>>(Cells)==source,"the fifteen original weapon entries remain intact");
        const auto clone=f.get<unsigned>(Weapon+0x48);
        require(f.get<WeaponMenuCell>(clone+15*10).id==18&&f.get<WeaponMenuCell>(clone+15*10).kind==1,"the custom cell retains a safe carrier and a distinct kind");
        require(f.get<WeaponMenuCell>(clone+16*10).id==0,"unused cells are native empty slots");
        f.put(Weapon+0x3c,15);require(menu.selected_key()==RevolveratorKey,"the two visual Walloper carrier entries resolve to separate logical keys");
        f.put(Weapon+0x3c,8);require(menu.selected_key()==NativeWalloperKey,"the original Walloper cell retains its own identity");
        f.put(Weapon+0x3c,15);menu.reset();
        require(f.get<unsigned>(Weapon+0x48)==Cells&&f.get<int>(Weapon+0x40)==3&&f.get<int>(Weapon+0x3c)==14,"unloading restores the old table and clamps its selection");
    }
    {
        Fixture f;f.state.set_walloper({false,0,0,0,0});f.put<std::uint8_t>(NativeOwned+18,0);
        NativeWeaponMenu menu(&f.api,f.arena,f);menu.frame();
        require(menu.grid_draw(Weapon,Fixture::draw)==2,"native draw flags are preserved");
        require(std::abs(f.drawn_icon_width-.1f)<.000001f&&std::abs(f.drawn_icon_height-.1f)<.000001f,
                "the four-row native grids default to eighty-percent icons with vertical clearance");
        require(f.visible_ids.size()==15&&std::count(f.visible_ids.begin(),f.visible_ids.end(),18)==1,"only the owned custom drill is shown when Walloper is unowned");
        require(f.get<std::uint8_t>(AlternateOwned)==1&&f.get<std::uint8_t>(AlternateOwned+18)==7,"temporary alternate ownership bytes are restored");
        f.state.set_state(RevolveratorKey,{false,0,0});f.state.set_walloper({true,1,1,0,0});f.put<std::uint8_t>(NativeOwned+18,1);
        menu.grid_draw(Weapon,Fixture::draw);
        require(f.visible_ids.size()==15&&std::count(f.visible_ids.begin(),f.visible_ids.end(),18)==1,"an unowned custom weapon does not borrow Walloper ownership");
        f.state.set_state(RevolveratorKey,{true,10,0});
        menu.grid_draw(Weapon,Fixture::draw);
        require(f.visible_ids.size()==16,"both weapons coexist when independently owned");
        menu.set_icon_scale(.8f);const float width=f.get<float>(0x160290),height=f.get<float>(0x160294);
        menu.grid_draw(Weapon,Fixture::draw);
        require(f.get<float>(0x160290)==width&&f.get<float>(0x160294)==height,"per-grid icon scaling cannot shrink other menu panels");
    }
    {
        Fixture f;f.api.relocate_data=+[](unsigned at){
            // Simulate a resolver that has no code references for raw widget
            // labels. Only the real cell table remains a reliable anchor.
            if(at==Weapon||at==Quick||at==Ring||at==Gadgets)return 0x1f00000u;
            return at;
        };
        NativeWeaponMenu menu(&f.api,f.arena,f);menu.frame();
        require(menu.ready()&&menu.screen_address(Ring)==Ring,"static widgets resolve from the validated weapon-cell anchor");
        f.put<std::int16_t>(Cells+6,8);
        require(menu.screen_address(Ring)==0,"an incompatible weapon table cannot invent widget addresses");
    }
    {
        Fixture f;NativeWeaponMenu menu(&f.api,f.arena,f);menu.frame();
        f.put(Weapon+0x3c,10);f.put(Pad,0x4040u);
        require(menu.grid_update(Weapon,Fixture::update)==0,"navigation and confirmation retain native result");
        require(f.get<int>(Weapon+0x3c)==15&&f.state.equipped()==RevolveratorKey,"down plus confirm equips the destination, not the previous row");
        require(f.native_confirms==1&&f.updates.size()==2&&f.updates[0].buttons==0x4000&&f.updates[1].buttons==0x40,"one native confirm is emitted after one navigation pass");
        require(f.updates[1].projected==RevolveratorKey&&f.get<unsigned>(Pad)==0x4040,"confirmation sees the right logical projection and preserves the pad word");
        f.updates.clear();f.put(Pad,0x840u);const auto previous=f.state.equipped();
        require(menu.grid_update(Weapon,Fixture::update)==1&&f.updates.size()==1&&f.state.equipped()==previous,"native close requests prevent a second confirm call");
        f.state.set_state(RevolveratorKey,{false,0,0});f.put(Pad,0x40u);f.state.equip(NativeWalloperKey);
        menu.grid_update(Weapon,Fixture::update);
        require(f.state.equipped()==NativeWalloperKey&&f.native_confirms==1,"confirm cannot equip an unowned drill through owned Walloper");
    }
    {
        Fixture f;NativeWeaponMenu menu(&f.api,f.arena,f);menu.frame();
        f.state.assign_quick(0,NativeWalloperKey);f.state.assign_quick(1,RevolveratorKey);
        f.put(Page+0x40,Quick);f.put(Quick+0x3c,15);f.put(Ring+0x50,2);
        menu.quick_load(Ring,Fixture::noop);f.put(Ring+0x50,2);f.put(Pad,0x40u);
        menu.slots_update(Ring,Fixture::slots);
        require(f.native_slot_assigns==1,"assignment keeps the native statistics path once");
        require(f.state.quick()[0]==NativeWalloperKey&&f.state.quick()[1]==0&&f.state.quick()[2]==RevolveratorKey,"logical duplicate removal moves only the drill favourite");
        const auto native=f.get<std::array<unsigned,8>>(Ring+0x30);
        require(native[0]==18&&native[2]==18,"different logical favourites safely share carrier18 in the native editor");
        f.put(Quick+0x3c,8);f.put(Ring+0x50,3);menu.slots_update(Ring,Fixture::slots);
        require(f.state.quick()[0]==0&&f.state.quick()[2]==RevolveratorKey&&f.state.quick()[3]==NativeWalloperKey,"moving Walloper keeps the drill binding");
        f.state.equip(RevolveratorKey);f.restore_gameplay();
        menu.quick_draw(Ring,Fixture::ring);
        require(f.ring_calls==1&&f.ring_seen[2]==0&&f.ring_seen[3]==18,"only the drill icon is hidden during the original ring draw");
        require(f.ring_view==NativeWalloperKey&&f.view==RevolveratorKey,"native ring draws the stock Walloper definition then restores the equipped drill");
        require(f.get<std::array<unsigned,8>>(Ring+0x30)==f.state.native_slots(),"native favourites are restored immediately after drawing");
        require(f.sprites.size()==1&&f.sprites[0].frame==60037*8&&f.sprites[0].x==407&&f.sprites[0].y==191,"the drill overlays its exact native east ring cell");
        require(f.sprites[0].w==32&&f.sprites[0].h==32&&f.paging_calls==1&&f.finish_calls==1,"native ring dimensions and paging are unchanged");
    }
    {
        Fixture f;
        for(unsigned i=0;i<5;++i){const WeaponKey key=0x10020+i;require(f.state.register_weapon({key,"future-"+std::to_string(i),18,60038+i,10,1,1}),"additional descriptors register");f.state.set_state(key,{true,10,0});}
        NativeWeaponMenu menu(&f.api,f.arena,f);menu.frame();
        require(menu.catalog_size()==21&&f.get<int>(Weapon+0x40)==4,"larger catalogs use a bounded four-row window");
        f.put(Weapon+0x3c,15);f.put(Pad,0x4000u);menu.grid_update(Weapon,Fixture::update);
        require(menu.first_row(Weapon)==1&&f.get<int>(Weapon+0x3c)==15&&menu.selected_key()==0x10024,"down at the visible edge scrolls to the twenty-first weapon");
        f.put(Pad,0x4000u);menu.grid_update(Weapon,Fixture::update);
        require(menu.first_row(Weapon)==0&&f.get<int>(Weapon+0x3c)==0&&menu.selected_key()==10,"native wrap returns to the beginning of the full catalog");
        f.put(Weapon+0x3c,0);f.put(Pad,0x1000u);menu.grid_update(Weapon,Fixture::update);
        require(menu.first_row(Weapon)==1&&f.get<int>(Weapon+0x3c)==15,"up from the first item reaches the real last catalog row");
        f.put(Page+0x40,Gadgets);f.put(Gadgets+0x3c,3);f.put(Pad,0x4000u);menu.grid_update(Gadgets,Fixture::update);
        require(menu.first_row(Quick)==0&&f.get<unsigned>(Page+0x80)==Quick,"return from the lower gadget edge enters the first weapon row");
        f.put(Page+0x40,Gadgets);f.put(Gadgets+0x3c,0);f.put(Pad,0x1000u);menu.grid_update(Gadgets,Fixture::update);
        require(menu.first_row(Quick)==1&&f.get<int>(Quick+0x3c)==15,"return from the upper gadget edge enters the final weapon row");
    }
    {
        Fixture f;NativeWeaponMenu menu(&f.api,f.arena,f);menu.frame();
        f.put(Weapon+0x48,0x1f00000u);menu.reset();
        require(f.get<unsigned>(Weapon+0x48)==0x1f00000,"teardown preserves a later owner's replacement grid");
        require(f.get<unsigned>(Quick+0x48)==Cells,"other unchanged owned grids still restore");
    }
    std::printf("weapon menu: %d native layout, ownership, navigation, favourite and rendering checks passed\n",checks);
}
