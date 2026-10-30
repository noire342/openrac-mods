// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_weapon_menu.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace revolverator {
namespace {
constexpr unsigned NativeOwned=0x13d4c0,AlternateOwned=0x13d388;
constexpr unsigned NativeQuick=0x141ea0,IconWidth=0x160290,IconHeight=0x160294;
constexpr unsigned Cross=0x40,Up=0x1000,Down=0x4000;
constexpr std::array<int,15> NativeWeapons{{10,16,15,20,17,14,9,11,18,13,25,24,19,21,23}};
class Projection {
    ArsenalMenuModel& model_;bool active_,commit_;
public:
    Projection(ArsenalMenuModel& model,arsenal::WeaponKey key,bool commit=false)
        :model_(model),active_(key!=0),commit_(commit){if(active_)model_.project(key);}
    ~Projection(){if(active_){if(commit_)model_.commit();model_.restore_gameplay();}}
};
template<class T>class ScopedWrite {
    const Memory& memory_;unsigned at_;T before_,applied_;
public:
    ScopedWrite(const Memory& memory,unsigned at,T value)
        :memory_(memory),at_(at),before_(memory.read<T>(at)),applied_(value){memory_.write(at_,applied_);}
    ~ScopedWrite(){const auto current=memory_.read<T>(at_);if(std::memcmp(&current,&applied_,sizeof(T))==0)memory_.write(at_,before_);}
};
}

NativeWeaponMenu::NativeWeaponMenu(const openrac_api* api,GuestArena& arena,ArsenalMenuModel& model)
    :Memory(api,&arena),model_(model),
     sprite_(function<Sprite>("FUN_001ffc30")),find_(function<FindFrame>("FUN_001ff960")),
     paging_(function<Paging>("FUN_001f4280")),finish_(function<FinishPaging>("FUN_001f4398")),
     cosine_(function<Trig>("FUN_001f9dc8")),sine_(function<Trig>("FUN_001f9de0")),
     normalize_(function<Trig>("FUN_001fa610")),
     sound_(function<Sound>("FUN_0022da68")){
    // Packed levels can have no spare level tail. The existing bounded HUD
    // allocator can still supply these two 200-byte cell arrays.
    hud_fallback_=true;
}
NativeWeaponMenu::~NativeWeaponMenu(){reset();}
bool NativeWeaponMenu::supported()const{
    return api_&&api_->memory&&api_->game&&api_->version&&
        std::strcmp(api_->game,"rac1")==0&&std::strcmp(api_->version,"ntsc")==0&&
        api_->loaded_level()>=0&&api_->loaded_level()<19;
}
void NativeWeaponMenu::set_icon_scale(float value){
    if(std::isfinite(value))icon_scale_=std::clamp(value,.5f,1.f);
}
unsigned NativeWeaponMenu::screen_address(unsigned canonical)const{
    if(canonical<0x1ce2c0||canonical>=0x1d2000)return 0;
    const auto anchor=global(WeaponCells);
    if(!valid(anchor,sizeof(native_)))return 0;
    const auto cells=read<decltype(native_)>(anchor);
    for(unsigned i=0;i<cells.size();++i)if(cells[i].id!=NativeWeapons[i]||cells[i].kind!=0)return 0;
    const auto address=std::int64_t(anchor)+std::int64_t(canonical)-std::int64_t(WeaponCells);
    return address>=0&&address<=0x2000000-0x60?unsigned(address):0;
}
unsigned NativeWeaponMenu::focused()const{
    const auto page=read<unsigned>(global(MenuSystem)+4);
    return valid(page,0x88)?read<unsigned>(page+0x40):0;
}
unsigned NativeWeaponMenu::pending_focus()const{
    const auto page=read<unsigned>(global(MenuSystem)+4);
    return valid(page,0x88)?read<unsigned>(page+0x80):0;
}
NativeWeaponMenu::Grid* NativeWeaponMenu::grid(unsigned screen){
    for(auto& g:grids_)if(g.active&&g.address==screen&&api_->loaded_level()==level_)return &g;
    return nullptr;
}
const NativeWeaponMenu::Grid* NativeWeaponMenu::grid(unsigned screen)const{
    for(const auto& g:grids_)if(g.active&&g.address==screen&&api_->loaded_level()==level_)return &g;
    return nullptr;
}
bool NativeWeaponMenu::owns_grid(unsigned screen)const{
    const auto g=grid(screen);
    return g&&read<unsigned>(screen+0x48)==g->cells&&read<int>(screen+0x40)==g->rows&&read<int>(screen+0x44)==int(Columns);
}
bool NativeWeaponMenu::ready()const{
    return supported()&&owns_grid(grids_[0].address)&&owns_grid(grids_[1].address);
}
unsigned NativeWeaponMenu::visible_rows()const{
    return std::min(VisibleRows,unsigned((catalog_.size()+Columns-1)/Columns));
}
unsigned NativeWeaponMenu::maximum_start_row()const{
    const unsigned total=unsigned((catalog_.size()+Columns-1)/Columns),shown=visible_rows();
    return total>shown?total-shown:0;
}
void NativeWeaponMenu::fill(Grid& g){
    std::array<WeaponMenuCell,VisibleCells> cells{};
    const unsigned first=unsigned(g.start_row)*Columns;
    for(unsigned i=0;i<VisibleCells&&first+i<catalog_.size();++i)cells[i]=catalog_[first+i].cell;
    write(g.cells,cells);
}
void NativeWeaponMenu::frame(){
    const int current=api_->loaded_level();
    if(current!=level_){reset();level_=current;}
    if(!supported())return;
    if(ready()&&descriptor_count_==model_.registry().descriptors().size())return;
    // Never reinterpret another owner's replacement cell array or layout.
    if(grids_[0].active||grids_[1].active)return;
    const auto source=global(WeaponCells);
    if(!valid(source,sizeof(native_)))return;
    native_=read<decltype(native_)>(source);
    for(unsigned i=0;i<native_.size();++i)if(native_[i].id!=NativeWeapons[i]||native_[i].kind!=0)return;
    const unsigned addresses[]{screen_address(WeaponGrid),screen_address(QuickWeaponGrid)};
    for(const auto address:addresses){
        if(!valid(address,0x60)||read<int>(address+0x40)!=3||read<int>(address+0x44)!=5||
           read<unsigned>(address+0x48)!=source)return;
        if(api_->function_at(read<unsigned>(address))!=api_->function("FUN_0021b858")||
           api_->function_at(read<unsigned>(address+4))!=api_->function("FUN_0021d948"))return;
    }
    catalog_.clear();
    for(const auto& cell:native_)catalog_.push_back({cell,WeaponKey(cell.id)});
    for(const auto& descriptor:model_.registry().descriptors()){
        if(descriptor.carrier>arsenal::Registry::MaximumNativeKey||descriptor.carrier==0||descriptor.icon_bank>65535)return;
        WeaponMenuCell cell{std::uint16_t(descriptor.icon_bank),0,1,std::int16_t(descriptor.carrier),0};
        catalog_.push_back({cell,descriptor.key});
    }
    descriptor_count_=model_.registry().descriptors().size();
    if(descriptor_count_==0)return;
    constexpr unsigned buffer_bytes=unsigned(sizeof(WeaponMenuCell))*VisibleCells;
    const unsigned allocation=allocate_guest(2*buffer_bytes);
    if(!valid(allocation,2*buffer_bytes)){catalog_.clear();return;}
    for(unsigned i=0;i<grids_.size();++i){
        auto& g=grids_[i];g.address=addresses[i];g.original_cells=source;g.original_rows=3;g.original_cols=5;
        g.cells=allocation+i*buffer_bytes;g.rows=int(visible_rows());g.active=true;
        fill(g);write(g.address+0x48,g.cells);write(g.address+0x40,g.rows);
    }
}
NativeWeaponMenu::WeaponKey NativeWeaponMenu::key_at(const Grid& g,int cell)const{
    if(cell<0||cell>=g.rows*int(Columns))return 0;
    const unsigned index=unsigned(g.start_row)*Columns+unsigned(cell);
    return index<catalog_.size()?catalog_[index].key:0;
}
NativeWeaponMenu::WeaponKey NativeWeaponMenu::selected_key()const{
    const auto focus=focused();
    if(const auto g=grid(focus))return owns_grid(focus)?key_at(*g,read<int>(focus+0x3c)):0;
    // Gadget grids retain their own native identities and ownership rules.
    if(!valid(focus,0x60))return 0;
    const int rows=read<int>(focus+0x40),columns=read<int>(focus+0x44),selected=read<int>(focus+0x3c);
    const unsigned cells=read<unsigned>(focus+0x48);
    if(rows<=0||columns<=0||rows>8||columns>8||selected<0||selected>=rows*columns||!valid(cells,unsigned(rows*columns)*10))return 0;
    const auto cell=read<WeaponMenuCell>(cells+unsigned(selected)*10);
    return cell.kind==0&&cell.id>0&&cell.id<=int(arsenal::Registry::MaximumNativeKey)?WeaponKey(cell.id):0;
}
bool NativeWeaponMenu::owned(WeaponKey key)const{
    const auto& registry=model_.registry();
    if(key==arsenal::NativeWalloperKey||key==arsenal::NativeWrenchKey||registry.descriptor(key))return registry.owned(key);
    return key>0&&key<=arsenal::Registry::MaximumNativeKey&&read<std::uint8_t>(global(NativeOwned)+key)!=0;
}
void NativeWeaponMenu::follow_grid_transition(unsigned from,unsigned buttons){
    const auto next=pending_focus();auto g=grid(next);
    if(!g||from==next)return;
    // The native 3-column gadget panel links to both edges of the 5-column
    // weapon panel. Enter its real first/last row, not a stale scrolled window.
    if(buttons&Down)g->start_row=0;
    else if(buttons&Up)g->start_row=int(maximum_start_row());
    fill(*g);
}
int NativeWeaponMenu::grid_update(unsigned screen,Call original){
    if(!original)return 0;
    if(!supported())return original(screen);
    const auto buttons=read<unsigned>(pressed_at());
    auto g=grid(screen);
    if(!g||!owns_grid(screen)||focused()!=screen){
        const int result=original(screen);follow_grid_transition(screen,buttons);return result;
    }
    const auto saved_buttons=buttons;
    const int selected=read<int>(screen+0x3c);
    if(selected<0||selected>=g->rows*int(Columns))write<int>(screen+0x3c,0);
    unsigned navigation=buttons&~Cross;
    const int old=read<int>(screen+0x3c),row=old/int(Columns);
    bool scrolled=false;
    if((navigation&Up)&&row==0&&g->start_row>0){--g->start_row;navigation&=~Up;scrolled=true;}
    if((navigation&Down)&&row+1==g->rows&&unsigned(g->start_row)<maximum_start_row()){
        ++g->start_row;navigation&=~Down;scrolled=true;
    }
    if(scrolled){fill(*g);sound_(1,0x11,read<unsigned>(screen+0x14));}
    int result;
    {
        ScopedWrite<unsigned> masked(*this,pressed_at(),navigation);
        Projection view(model_,selected_key());result=original(screen);
    }
    // A native wrap on a standalone grid wraps the entire logical catalog.
    const int after=read<int>(screen+0x3c);
    if(!scrolled&&(saved_buttons&Down)&&row+1==g->rows&&after/int(Columns)==0&&
       read<unsigned>(screen+0x50)==0){g->start_row=0;fill(*g);}
    if(!scrolled&&(saved_buttons&Up)&&row==0&&after/int(Columns)+1==g->rows&&
       read<unsigned>(screen+0x4c)==0){g->start_row=int(maximum_start_row());fill(*g);}
    if(result!=0)return result;
    if(saved_buttons&Cross){
        const auto key=selected_key();
        if(key&&owned(key))model_.registry().equip(key);
        ScopedWrite<unsigned> confirm(*this,pressed_at(),Cross);
        Projection view(model_,key,true);result=original(screen);
    }
    follow_grid_transition(screen,saved_buttons);
    return result;
}
int NativeWeaponMenu::grid_draw(unsigned screen,Call original){
    if(!original)return 0;
    auto g=grid(screen);if(!g||!owns_grid(screen))return original(screen);
    fill(*g);
    const auto before=read<std::array<WeaponMenuCell,VisibleCells>>(g->cells);
    auto applied=before;
    const unsigned first=unsigned(g->start_row)*Columns;
    // A kind1 cell reads the alternate-owned table. Hide each unowned custom
    // cell independently even when several logical weapons share one carrier.
    std::array<std::uint8_t,37> alternate_before{};
    std::array<bool,37> alternate_changed{};
    const auto alternate=global(AlternateOwned);
    for(unsigned i=0;i<VisibleCells&&first+i<catalog_.size();++i){
        const auto& entry=catalog_[first+i];if(!model_.registry().descriptor(entry.key))continue;
        const unsigned carrier=unsigned(entry.cell.id);
        if(!alternate_changed[carrier]){alternate_before[carrier]=read<std::uint8_t>(alternate+carrier);alternate_changed[carrier]=true;write<std::uint8_t>(alternate+carrier,1);}
        if(!owned(entry.key))applied[i].id=0;
    }
    const auto zero_before=read<std::uint8_t>(alternate);write<std::uint8_t>(alternate,0);
    write(g->cells,applied);
    const auto width=global(IconWidth),height=global(IconHeight);
    const float w=read<float>(width)*icon_scale_,h=read<float>(height)*icon_scale_;
    int result;
    {
        ScopedWrite<float> scaled_width(*this,width,w),scaled_height(*this,height,h);
        Projection stock(model_,arsenal::NativeWalloperKey);
        // Native grids normally suppress equipped-frame offsets (flag0x20).
        // If another presentation enables them, don't mark Walloper equipped
        // while its carrier is displaying a different logical weapon.
        const auto equipped=global(MenuSystem)+0x30;
        const bool custom=model_.registry().descriptor(equipped_key())!=nullptr;
        ScopedWrite<int> marker(*this,equipped,custom?-1:read<int>(equipped));
        result=original(screen);
    }
    if(read<decltype(applied)>(g->cells)==applied)write(g->cells,before);
    for(unsigned i=1;i<alternate_changed.size();++i)if(alternate_changed[i]&&read<std::uint8_t>(alternate+i)==1)write(alternate+i,alternate_before[i]);
    if(read<std::uint8_t>(alternate)==0)write(alternate,zero_before);
    return result;
}
void NativeWeaponMenu::write_native_slots(unsigned screen){
    if(valid(screen+0x30,32))write(screen+0x30,model_.registry().native_slots());
}
int NativeWeaponMenu::slots_update(unsigned screen,Call original){
    if(!original)return 0;
    if(!supported()||screen!=screen_address(QuickRing))return original(screen);
    const auto key=selected_key();const auto buttons=read<unsigned>(pressed_at());
    int slot=read<int>(screen+0x50);
    if(slot<0||slot>=8)return original(screen);
    if(buttons&8)slot=(slot+1)%8;
    if(buttons&4)slot=(slot+7)%8;
    int result;
    {
        Projection view(model_,key,true);result=original(screen);
    }
    if((buttons&Cross)&&owned(key)){
        model_.registry().assign_quick(unsigned(slot),key);
        write_native_slots(screen);
    }
    return result;
}
int NativeWeaponMenu::quick_load(unsigned screen,Call original){
    if(!original)return 0;
    const int result=original(screen);
    if(supported()&&screen==screen_address(QuickRing)){
        write_native_slots(screen);
        int cursor=0;const auto& keys=model_.registry().quick();
        while(cursor<8&&keys[unsigned(cursor)]!=0)++cursor;
        write(screen+0x50,cursor%8);
    }
    return result;
}
int NativeWeaponMenu::quick_store(unsigned screen,Call original){
    if(!original)return 0;
    if(supported()&&screen==screen_address(QuickRing))write_native_slots(screen);
    return original(screen);
}
int NativeWeaponMenu::quick_draw(unsigned screen,Call original){
    if(!original)return 0;
    if(!supported()||screen!=screen_address(QuickRing)||!valid(screen,0x54))return original(screen);
    auto before=read<std::array<int,8>>(screen+0x30),safe=before;
    const auto& keys=model_.registry().quick();
    std::array<unsigned,8> icons{};
    for(unsigned i=0;i<8;++i){
        const auto descriptor=model_.registry().descriptor(keys[i]);
        if(descriptor&&safe[i]==int(descriptor->carrier)){
            safe[i]=0;if(owned(keys[i]))icons[i]=descriptor->icon_bank;
        }else if(safe[i]<0||safe[i]>int(arsenal::Registry::MaximumNativeKey))safe[i]=0;
    }
    write(screen+0x30,safe);
    int result;
    {
        // Native ring sprites are looked up through the carrier definition.
        // Keep the stock Walloper visible while custom cells are overlaid
        // separately below, even when Revolverator is the equipped identity.
        Projection stock(model_,arsenal::NativeWalloperKey);
        result=original(screen);
    }
    if(read<decltype(safe)>(screen+0x30)==safe)write(screen+0x30,before);
    const int width=read<int>(screen+0x20),height=read<int>(screen+0x24);
    if(width<32||height<32||width>4096||height>4096)return result;
    const float cx=float(width)*.5f,cy=float(height)*.5f;
    const float radius=float(std::min(width,height))*.5f-40.f;
    bool started=false;
    for(unsigned i=0;i<icons.size();++i){
        if(!icons[i])continue;
        if(!started){paging_(0);started=true;}
        const float angle=normalize_(float(i)*.7853982f-1.5707964f);
        const int x=int(cx+cosine_(angle)*radius),y=int(cy+sine_(angle)*radius);
        sprite_(find_(int(icons[i]),0),x-0x11,y-0x11,0x20,0x20,0x80);
    }
    if(started)finish_();
    return result;
}
int NativeWeaponMenu::selected_call(unsigned screen,Call original){
    if(!original)return 0;
    if(!supported())return original(screen);
    Projection view(model_,selected_key());return original(screen);
}
int NativeWeaponMenu::equipped_call(unsigned screen,Call original){
    if(!original)return 0;
    if(!supported())return original(screen);
    Projection view(model_,equipped_key());return original(screen);
}
unsigned NativeWeaponMenu::first_row(unsigned screen)const{
    const auto g=grid(screen);return g?unsigned(g->start_row):0;
}
void NativeWeaponMenu::reset(){
    if(api_->loaded_level()==level_){
        for(auto& g:grids_){
            if(!g.active||!owns_grid(g.address))continue;
            write(g.address+0x48,g.original_cells);write(g.address+0x40,g.original_rows);write(g.address+0x44,g.original_cols);
            const int selected=read<int>(g.address+0x3c);
            if(selected>=g.original_rows*g.original_cols)write(g.address+0x3c,g.original_rows*g.original_cols-1);
        }
    }
    grids_={};catalog_.clear();native_={};level_=-1;descriptor_count_=0;
}
}
