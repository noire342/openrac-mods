// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_vendor_icons.hpp"
#include "native_vendor.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace revolverator;
namespace {
unsigned checks;
void require(bool condition,const char* message){
    ++checks;if(!condition){std::fprintf(stderr,"native vendor icons: %s\n",message);std::exit(1);}
}
constexpr unsigned menu=0x1ca4c0,resident=0x19a3e8,overlay=0x17e4d8;
constexpr int marker=0x52564f31;
struct Row {int item{},ammo{},state{},unused{},sound{};};
struct Rectangle {int x0,y0,x1,y1,ox,oy;unsigned color;};
struct Sprite {int frame,x,y,width,height,alpha;};
struct Lookup {int bank,frame;};
struct Fixture;
Fixture* live;
struct Fixture {
    std::vector<unsigned char> ram=std::vector<unsigned char>(0x2000000);
    openrac_api api{};
    NativeVendor vendor;
    NativeVendorIcons icons;
    std::vector<Rectangle> rectangles;
    std::vector<Sprite> sprites;
    std::vector<Lookup> lookups;
    std::array<unsigned,5> native_tables{0x220000,0x221000,0x222000,0x223000,0x224000};
    std::array<unsigned,5> previous_tables{0x320000,0x321000,0x322000,0x323000,0x324000};
    int loaded_level=1;
    bool foreign_edit{};
    template<class T>void put(unsigned at,T value){std::memcpy(ram.data()+at,&value,sizeof value);}
    template<class T>T get(unsigned at)const{T value{};std::memcpy(&value,ram.data()+at,sizeof value);return value;}
    static unsigned relocate(unsigned at){return at;}
    static unsigned relocate_level(int,unsigned at){return at;}
    static int loaded(){return live->loaded_level;}
    static void check_tables(){require(live->get<std::array<unsigned,5>>(overlay+0x18)==live->native_tables,"all five resident HUD words visible during native drawing");}
    static void rectangle(int x0,int y0,int x1,int y1,int ox,int oy,unsigned color){
        check_tables();live->rectangles.push_back({x0,y0,x1,y1,ox,oy,color});
    }
    static int frame(int bank,int index){check_tables();live->lookups.push_back({bank,index});return bank*4+index;}
    static void sprite(int frame_index,int x,int y,int width,int height,int alpha){
        check_tables();live->sprites.push_back({frame_index,x,y,width,height,alpha});
        if(live->foreign_edit)live->put(overlay+0x18+12,0x12345678u);
    }
    static void* function(const char* name){
        if(!std::strcmp(name,"FUN_001fb8f0"))return reinterpret_cast<void*>(rectangle);
        if(!std::strcmp(name,"FUN_001ff960"))return reinterpret_cast<void*>(frame);
        if(!std::strcmp(name,"FUN_001ffc30"))return reinterpret_cast<void*>(sprite);
        return nullptr;
    }
    static const openrac_api* prepare(Fixture* fixture){
        live=fixture;auto& api=fixture->api;api.memory=fixture->ram.data();api.game="rac1";api.version="ntsc";
        api.loaded_level=loaded;api.relocate_data=relocate;api.relocate_level_data=relocate_level;api.function=function;
        return &api;
    }
    Fixture():vendor(prepare(this)),icons(&api,vendor){
        put(resident+0x18,native_tables);put(overlay+0x18,previous_tables);put(0x15f5c4,5);
        for(unsigned i=0;i<37;++i)put<unsigned short>(0x1863d0+i*76+0x38,static_cast<unsigned short>(60000+i));
        put(0x1c40b0+18*24,7500);put<unsigned short>(0x1c40b0+18*24+8,0);
        vendor.frame();
    }
    void rows(const std::vector<Row>& rows,int selection=0,int scroll=0){
        std::array<Row,16> data{};for(unsigned i=0;i<rows.size()&&i<data.size();++i)data[i]=rows[i];
        put(menu+0xd0,data);put(menu+0x210,int(rows.size()));put(menu+0x58,selection);put(menu+0x4c,scroll);
    }
    void strip(unsigned count,int selected,int scroll=0){
        std::vector<Row> rows_data;for(unsigned i=0;i<count;++i)rows_data.push_back({int(i%37),int(i&1u),0,0,0});
        rows(rows_data,selected,scroll);
    }
    void tables_restored()const{require(get<std::array<unsigned,5>>(overlay+0x18)==previous_tables,"overlay HUD words restored after pass");require(get<std::array<unsigned,5>>(resident+0x18)==native_tables,"resident HUD words unchanged");}
    void no_packets()const{require(rectangles.empty()&&sprites.empty()&&lookups.empty(),"refused pass emits no partial native packets");}
};
}
int main(){
    {
        Fixture f;f.rows({{18,0,0,0,0},{18,1,0,marker,0},{15,1,0,0,0}},1);
        f.put<unsigned short>(0x1863d0+18*76+0x38,60037); // The selected Revo projects the shared native18 definition.
        const auto before=f.ram;
        require(f.icons.draw(),"small mixed logical strip handled");
        require(f.rectangles.size()==3&&f.sprites.size()==3,"native clear and selection plus one sprite per row");
        require(f.lookups[0].bank==60018&&f.lookups[0].frame==0&&f.lookups[1].bank==60037&&f.lookups[1].frame==2&&f.lookups[2].bank==60015,"native Walloper, custom ammo and other weapon retain distinct banks");
        const auto& selection=f.rectangles[1];require(selection.x0==64&&selection.x1==120&&selection.y0==2&&selection.y1==58,"selection geometry matches native small strip");
        const auto& inner=f.rectangles[2];require(inner.x0==66&&inner.x1==118&&inner.y0==4&&inner.y1==56&&inner.color==0x80000000u,"native selection interior preserved");
        for(unsigned i=0;i<f.sprites.size();++i){const auto& s=f.sprites[i];require(s.x==12+int(i)*56&&s.y==6&&s.width==48&&s.height==48&&s.alpha==128,"small-strip sprite positions, size and alpha preserved");}
        f.tables_restored();require(f.ram==before,"small draw does not change selection, items, inventory, prices or any guest bytes");
    }
    for(unsigned count=1;count<=7;++count){
        Fixture f;f.strip(count,int(count)-1);require(f.icons.draw()&&f.sprites.size()==count,"all one-through-seven layouts keep native row count");f.tables_restored();
    }
    for(unsigned count:{8u,9u,12u,16u})for(int selected:{0,int(count)-1}){
        Fixture f;f.strip(count,selected);require(f.icons.draw()&&f.sprites.size()==11&&f.rectangles.size()==3,"large strip keeps eleven wrapped native cells and highlight");
        for(unsigned i=0;i<11;++i){
            const unsigned row=unsigned((int(count)*2+selected-5+int(i))%int(count));
            require(f.lookups[i].bank==60000+int(row)&&f.lookups[i].frame==int(row&1u)*2,"wrapped cell resolves the corresponding logical row");
            const auto& s=f.sprites[i];require(s.x==-100+int(i)*56&&s.y==6&&s.width==48&&s.height==48&&static_cast<unsigned>(s.alpha)==0x80808080u,"large-strip geometry and native alpha bit pattern preserved");
        }
        require(f.rectangles[1].x0==176&&f.rectangles[1].x1==232&&f.rectangles[2].x0==178&&f.rectangles[2].x1==230,"large-strip central highlight exact");f.tables_restored();
    }
    for(int scroll:{-56,-4,4,56}){
        Fixture f;f.strip(8,0,scroll);const int after=scroll+(scroll<0?4:-4);auto expected=f.ram;std::memcpy(expected.data()+menu+0x4c,&after,4);
        require(f.icons.draw()&&f.rectangles.size()==1&&f.sprites.size()==11,"moving strip omits highlight even on the frame reaching zero");
        require(f.get<int>(menu+0x4c)==after&&f.sprites[0].x==after-100,"native scroll advances exactly four pixels before placing icons");
        f.tables_restored();require(f.ram==expected,"scroll offset is the only persistent guest mutation");
    }
    for(unsigned timer:{0u,1u,7u,8u,15u,16u,0xffffffffu}){
        Fixture f;f.strip(2,0);f.put(menu+4,timer);require(f.icons.draw(),"native timer pattern handled");
        const int phase=int((timer&15u)*4u)-32;
        const unsigned color=(unsigned(phase<0?-phase:phase)+64u)*0x10202u|0x80000000u;
        require(f.rectangles[1].color==color,"pulse color reproduces integer absolute-value formula without signed overflow");
    }
    for(int count:{-1,0,17,0x7fffffff}){
        Fixture f;f.strip(2,0);f.put(menu+0x210,count);require(!f.icons.draw(),"invalid count refused");f.no_packets();f.tables_restored();
    }
    for(int selection:{-1,2,100}){
        Fixture f;f.strip(2,0);f.put(menu+0x58,selection);require(!f.icons.draw(),"invalid selection refused");f.no_packets();
    }
    for(int mode:{0,3,4,7}){Fixture f;f.strip(2,0);f.put(0x15f5c4,mode);require(!f.icons.draw(),"nonvendor mode refused");f.no_packets();}
    for(int level:{-1,19}){Fixture f;f.strip(2,0);f.loaded_level=level;require(!f.icons.draw(),"invalid loaded level refused");f.no_packets();}
    {Fixture f;f.strip(2,0);f.api.version="pal";require(!f.icons.draw(),"PAL refused without native changes");f.no_packets();}
    {Fixture f;f.strip(2,0);f.api.game="other";require(!f.icons.draw(),"other game refused without native changes");f.no_packets();}
    {Fixture f;f.strip(8,0,0x7fffffff);require(!f.icons.draw(),"corrupt scroll cannot overflow native coordinates");f.no_packets();}
    {Fixture f;f.rows({{999,0,0,0,0}});require(!f.icons.draw(),"invalid native row bank fails before native clear");f.no_packets();}
    {Fixture f;f.strip(1,0);f.foreign_edit=true;require(f.icons.draw(),"single-cell pass permits foreign late HUD-table edit");
     auto expected=f.previous_tables;expected[3]=0x12345678u;require(f.get<std::array<unsigned,5>>(overlay+0x18)==expected,"scope restores its four unchanged words and preserves another owner's fifth word");}
    std::printf("native vendor icons: %u checks passed\n",checks);
}
