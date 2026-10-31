// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_icon_variants.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

using namespace revolverator;
namespace {
unsigned checks;
void require(bool condition,const char* message){
 ++checks;if(!condition){std::fprintf(stderr,"native icon variants: %s\n",message);std::exit(1);}
}
constexpr unsigned state=0x19a3e8,header=0x300000,defs=0x301000,refs=0x302000,images=0x303000,palettes=0x304000;
unsigned relocated_state=state;
unsigned physical(unsigned index){return (index&~24u)|((index&8u)<<1)|((index&16u)>>1);}
using Bytes=std::array<std::uint8_t,1024>;
using Rgba=std::array<std::uint8_t,4>;
Rgba color(const std::uint8_t* palette,unsigned index){Rgba result{};std::copy_n(palette+physical(index)*4,4,result.data());return result;}
void set_color(Bytes& palette,unsigned index,Rgba value){std::copy(value.begin(),value.end(),palette.begin()+physical(index)*4);}
Rgba pixel(HdIconData data,unsigned at){return color(data.palette,data.pixels[at]);}
struct Fixture {
 std::vector<std::uint8_t> ram=std::vector<std::uint8_t>(0x2000000);
 openrac_api api{};
 Memory memory;
 NativeIconVariants variants;
 std::vector<std::uint8_t> pixels=std::vector<std::uint8_t>(1024,8);
 Bytes palette{};
 template<class T>void put(unsigned at,T value){std::memcpy(ram.data()+at,&value,sizeof value);}
 template<class T>T get(unsigned at)const{T result{};std::memcpy(&result,ram.data()+at,sizeof result);return result;}
 static const openrac_api* prepare(Fixture* f){
  relocated_state=state;f->api.memory=f->ram.data();f->api.relocate_data=+[](unsigned at){return at==state?relocated_state:at;};return &f->api;
 }
 Fixture():memory(prepare(this)){
  put(state+0x18,header);put(state+0x1c,defs);put(state+0x20,refs);put(state+0x24,images);put(state+0x28,palettes);
  put(header+0x44,5u);put(header+0x24,5u);
  put<std::uint16_t>(defs,60015);put<std::uint16_t>(defs+2,5);put<std::uint16_t>(defs+4,0);put<std::uint16_t>(defs+8,0xffff);
  for(unsigned frame=0;frame<5;++frame){
   put(refs+frame*4,(frame<<16)|frame);put(images+frame*8,0x400000+frame*1024);
   put<std::uint8_t>(images+frame*8+6,5);put<std::uint8_t>(images+frame*8+7,5);
   put(palettes+frame*8,0x410000+frame*1024);
   Bytes source{},clut{};source.fill(8);
   set_color(clut,8,frame==4?Rgba{230,190,120,128}:Rgba{80,90,200,128});
   set_color(clut,17,frame==1?Rgba{235,235,235,128}:Rgba{25,25,24,128});
   set_color(clut,27,frame==4?Rgba{160,138,95,128}:Rgba{255,255,255,128});
   for(unsigned y=0;y<24;++y)for(unsigned x=14;x<19;++x)source[y*32+x]=17;
   if(frame==2)for(unsigned i=24*32;i<1024;++i)source[i]=std::uint8_t((i%3)==0?27:8);
   if(frame==4)source[700]=27;
   put(0x400000+frame*1024,source);put(0x410000+frame*1024,clut);
  }
  set_color(palette,8,{80,90,200,128});set_color(palette,17,{235,235,235,128});
  set_color(palette,27,{160,170,220,128});set_color(palette,44,{1,2,3,0});set_color(palette,99,{235,235,235,64});
  for(unsigned y=8;y<24;++y)for(unsigned x=12;x<20;++x)pixels[y*32+x]=17;
  pixels[0]=44;pixels[1]=27;pixels[2]=99;
  pixels[16*32+11]=99;
 }
 HdIconData normal()const{return {pixels.data(),pixels.size(),palette.data(),palette.size()};}
 bool build(){return variants.build(memory,normal());}
 void empty()const{
  for(const auto data:{variants.normal(),variants.ammo(),variants.golden()})require(!data.pixels&&!data.palette&&!data.pixel_bytes&&!data.palette_bytes,"unavailable getters contain no stale pointers");
 }
 void failed(){require(!build(),"malformed source rejected");empty();}
};
std::vector<std::uint8_t> read_file(const std::filesystem::path& path){
 std::ifstream stream(path,std::ios::binary);require(bool(stream),"private native input opens");
 return {std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
}
void write_file(const std::filesystem::path& path,const std::uint8_t* data,std::size_t size){
 std::ofstream stream(path,std::ios::binary);require(bool(stream),"private preview output opens");
 stream.write(reinterpret_cast<const char*>(data),static_cast<std::streamsize>(size));require(bool(stream),"private preview output completes");
}
void native_capture(int argc,char** argv){
 if(argc==1)return;
 require(argc==6,"native inspection expects RAM, HUD address, normal pixels, normal CLUT, private output directory");
 Fixture f;f.ram=read_file(argv[1]);require(f.ram.size()==0x2000000,"native capture has full32MiB RAM");
 f.api.memory=f.ram.data();relocated_state=static_cast<unsigned>(std::strtoul(argv[2],nullptr,0));
 f.pixels=read_file(argv[3]);const auto clut=read_file(argv[4]);require(clut.size()==1024,"native normal CLUT length");
 std::copy(clut.begin(),clut.end(),f.palette.begin());const auto before=f.ram;
 require(f.build(),"real native templates build both variants");
 const auto out=std::filesystem::path(argv[5]);std::filesystem::create_directories(out);
 const auto native_defs=f.get<unsigned>(relocated_state+0x1c),native_refs=f.get<unsigned>(relocated_state+0x20);
 const auto native_images=f.get<unsigned>(relocated_state+0x24),native_palettes=f.get<unsigned>(relocated_state+0x28);
 unsigned reference=0;
 for(unsigned i=0;i<1024;++i)if(f.get<std::uint16_t>(native_defs+i*8)==60015){
  reference=f.get<unsigned>(native_refs+(f.get<std::uint16_t>(native_defs+i*8+4)+2)*4);break;
 }
 const auto source_pixels=f.get<unsigned>(native_images+(reference>>16)*8),source_clut=f.get<unsigned>(native_palettes+(reference&65535)*8);
 const auto ammo=f.variants.ammo(),gold=f.variants.golden(),clean=f.variants.normal();
 require(std::memcmp(ammo.pixels+768,f.ram.data()+source_pixels+768,256)==0,"actual native AMMO lettering is byte-identical");
 require(std::memcmp(ammo.palette,f.ram.data()+source_clut,1024)==0,"actual native AMMO palette including CSM1 and alpha is byte-identical");
 require(gold.pixels!=f.pixels.data()&&gold.pixel_bytes==f.pixels.size(),"actual Gold owns a separate bake at input resolution");
 require(clean.pixels!=f.pixels.data()&&clean.pixels!=gold.pixels&&clean.pixel_bytes==gold.pixel_bytes,"actual normal owns a separate clean bake at matching resolution");
 const unsigned width=gold.pixel_bytes==16384?128:32;
 for(unsigned x=0;x<width;++x){
  require(gold.pixels[x]>=1&&gold.pixels[x]<=64,"actual Gold top edge has no inherited frame or noise");
  require(gold.pixels[(width-1)*width+x]>=1&&gold.pixels[(width-1)*width+x]<=64,"actual Gold bottom edge has no inherited frame or noise");
  require(clean.pixels[x]>=1&&clean.pixels[x]<=64,"actual normal top edge has no inherited frame or noise");
  require(clean.pixels[(width-1)*width+x]>=1&&clean.pixels[(width-1)*width+x]<=64,"actual normal bottom edge has no inherited frame or noise");
 }
 require(f.ram==before,"native inspection never modifies captured RAM");
 write_file(out/"ammo.psmt8",ammo.pixels,ammo.pixel_bytes);write_file(out/"ammo.clut",ammo.palette,ammo.palette_bytes);
 write_file(out/"golden.psmt8",gold.pixels,gold.pixel_bytes);write_file(out/"golden.clut",gold.palette,gold.palette_bytes);
 write_file(out/"normal.psmt8",clean.pixels,clean.pixel_bytes);write_file(out/"normal.clut",clean.palette,clean.palette_bytes);
 std::puts("NATIVE_FONT_SOURCE_MATCH_PASS:256letter-pixels+1024CLUT-bytes; guest RAM unchanged");
}
}
int main(int argc,char** argv){
 {
  Fixture f;f.empty();const auto before=f.ram;const auto normal_pixels=f.pixels;const auto normal_palette=f.palette;
  require(f.build(),"valid32x32 native variant build");const auto ammo=f.variants.ammo(),gold=f.variants.golden(),clean=f.variants.normal();
  require(ammo.pixel_bytes==1024&&ammo.palette_bytes==1024,"ammunition has its own32x32 texture and CLUT");
  require(ammo.pixels!=f.pixels.data()&&ammo.palette!=f.palette.data(),"ammo storage owned by builder");
  require(gold.pixels!=f.pixels.data()&&gold.pixels!=ammo.pixels&&gold.pixel_bytes==f.pixels.size(),"Gold owns its bake at the normal resolution");
  require(gold.palette!=f.palette.data()&&gold.palette!=ammo.palette&&gold.palette_bytes==1024,"Gold owns a distinct CLUT");
  require(clean.pixels!=f.pixels.data()&&clean.pixels!=gold.pixels&&clean.pixels!=ammo.pixels&&clean.pixel_bytes==1024,"normal owns its clean native32x32 texture");
  require(clean.palette!=f.palette.data()&&clean.palette!=gold.palette&&clean.palette!=ammo.palette&&clean.palette_bytes==1024,"normal owns a distinct CSM1 CLUT");
  const auto native_ammo=f.get<Bytes>(0x400000+2*1024),native_palette=f.get<Bytes>(0x410000+2*1024);
  require(std::memcmp(ammo.pixels+24*32,native_ammo.data()+24*32,256)==0,"native AMMO lettering copied byte for byte, never invented");
  require(std::memcmp(ammo.palette,native_palette.data(),1024)==0,"native AMMO CLUT copied without quantization or swizzle loss");
  require(pixel(gold,0)==Rgba{230,190,120,128},"Gold fills transparent blue corner with the native beige background");
  require(pixel(gold,12*32+16)==Rgba{25,25,24,128},"white drill core becomes solid native black");
  require(pixel(gold,16*32+11)==Rgba{128,108,72,128},"half-alpha white edge composites once into a clean native-color AA fringe");
  require(pixel(clean,0)==Rgba{80,90,200,128},"normal transparent source corner is replaced by clean native blue");
  require(pixel(clean,12*32+16)==Rgba{235,235,235,128},"normal white glyph keeps native foreground color");
  require(pixel(clean,16*32+11)==Rgba{158,163,218,128},"normal half-alpha edge composites cleanly into native blue");
  require(gold.palette[3]==0,"reserved index zero stays transparent");
  for(unsigned i=0;i<gold.pixel_bytes;++i)require(pixel(gold,i)[3]==128,"native Gold tile pixels use valid opaque PS2 alpha");
  for(unsigned i=0;i<clean.pixel_bytes;++i){
   const auto n=pixel(clean,i),g=pixel(gold,i);
   require(n[3]==128,"clean normal tile uses native opaque PS2 alpha");
   const double normal_coverage=double(int(n[0])-80)/(235-80),gold_coverage=double(230-int(g[0]))/(230-25);
   require(std::abs(normal_coverage-gold_coverage)<0.008,"normal and Gold use the same visible glyph and AA coverage");
  }
  require(color(ammo.palette,ammo.pixels[16])==Rgba{80,90,200,128},"native crosshair erased before composing drill");
  require(color(ammo.palette,ammo.pixels[12*32+16])==Rgba{25,25,24,128},"ammo tile shows drill silhouette instead of cartridge");
  require(f.ram==before&&f.pixels==normal_pixels&&f.palette==normal_palette,"builder does not mutate guest tables, templates, or normal data");
  const std::vector<std::uint8_t> baked(gold.pixels,gold.pixels+gold.pixel_bytes);
  const std::vector<std::uint8_t> baked_normal(clean.pixels,clean.pixels+clean.pixel_bytes);
  f.pixels.assign(1024,17);require(std::equal(baked.begin(),baked.end(),f.variants.golden().pixels),"Gold remains valid after input pixels change");
  require(std::equal(baked_normal.begin(),baked_normal.end(),f.variants.normal().pixels),"clean normal remains valid after input pixels change");
  f.variants.reset();f.empty();require(f.build(),"builder reusable after reset");
 }
 {
  Fixture f;f.pixels.assign(128*128,8);
  for(unsigned y=32;y<96;++y)for(unsigned x=48;x<80;++x)f.pixels[y*128+x]=17;
  require(f.build(),"HD128 input accepted");const auto gold=f.variants.golden();
  require(gold.pixels!=f.pixels.data()&&gold.pixel_bytes==16384,"HD Gold owns a full128x128 bake");
  require(f.variants.normal().pixels!=f.pixels.data()&&f.variants.normal().pixel_bytes==16384,"HD normal owns a full128x128 bake");
  require(f.variants.ammo().pixel_bytes==1024,"HD vendor ammo remains native32x32");
  require(color(f.variants.ammo().palette,f.variants.ammo().pixels[12*32+16])==Rgba{25,25,24,128},"HD drill is area-sampled into native ammo tile");
 }
 {
  Fixture f;require(f.build(),"baseline clean Gold build");
  const auto original=f.variants.golden();const std::vector<std::uint8_t> baseline(original.pixels,original.pixels+original.pixel_bytes);
  const auto normal=f.variants.normal();const std::vector<std::uint8_t> baseline_normal(normal.pixels,normal.pixels+normal.pixel_bytes);
  set_color(f.palette,55,{20,20,20,128});set_color(f.palette,56,{65,80,180,128});
  for(unsigned y=0;y<32;++y)for(unsigned x=0;x<32;++x){
   if(f.pixels[y*32+x]!=8)continue;
   f.pixels[y*32+x]=(x==0||x==31||y==0||y==31)?55:std::uint8_t(x>y?56:8);
  }
  require(f.build(),"blue reflection and dark border variant builds");
  require(std::equal(baseline.begin(),baseline.end(),f.variants.golden().pixels),"Gold background is independent of original triangular reflection and dark frame");
  require(std::equal(baseline_normal.begin(),baseline_normal.end(),f.variants.normal().pixels),"normal background is independent of original triangular reflection and dark frame");
  f.pixels[4*32+4]=17;require(f.build(),"isolated white glint input builds");
  require(std::equal(baseline.begin(),baseline.end(),f.variants.golden().pixels),"isolated source glint cannot become a dirty black speck");
  require(std::equal(baseline_normal.begin(),baseline_normal.end(),f.variants.normal().pixels),"normal rejects isolated source glints");
  set_color(f.palette,57,{220,220,225,128});
  for(unsigned x=0;x<32;++x)for(unsigned y=0;y<2;++y)f.pixels[y*32+x]=57;
  require(f.build(),"bright source frame input builds");
  require(std::equal(baseline.begin(),baseline.end(),f.variants.golden().pixels),"even a white thin frame is excluded from the Gold emblem");
  require(std::equal(baseline_normal.begin(),baseline_normal.end(),f.variants.normal().pixels),"normal rejects the same thin bright source frame");
 }
 {
  Fixture f;auto source=f.get<Bytes>(0x400000+1*1024),clut=f.get<Bytes>(0x410000+1*1024);
  const std::array<Rgba,4> corners{{{129,134,233,128},{120,125,224,128},{102,107,214,128},{82,87,173,128}}};
  for(unsigned corner=0;corner<4;++corner){
   set_color(clut,40+corner,corners[corner]);
   for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x)source[((corner&2)?31-y:y)*32+((corner&1)?31-x:x)]=std::uint8_t(40+corner);
  }
  f.put(0x400000+1*1024,source);f.put(0x410000+1*1024,clut);
  require(f.build(),"four native blue corners build a clean normal gradient");const auto normal=f.variants.normal();
  require(pixel(normal,0)==corners[0]&&pixel(normal,1023)==corners[3],"normal gradient uses native blue corner endpoints");
  for(unsigned y:{0u,31u})for(unsigned x=1;x<32;++x){
   const auto previous=pixel(normal,y*32+x-1),current=pixel(normal,y*32+x);
   for(unsigned channel=0;channel<3;++channel){
    require(current[channel]<=previous[channel],"normal gradient has no alternating dither or source reflection");
    require(previous[channel]-current[channel]<=3,"normal gradient has smooth adjacent steps");
   }
  }
 }
 {
  Fixture f;auto source=f.get<Bytes>(0x400000+4*1024),clut=f.get<Bytes>(0x410000+4*1024);
  const std::array<Rgba,4> corners{{{236,196,129,128},{220,180,112,128},{196,156,96,128},{168,135,81,128}}};
  for(unsigned corner=0;corner<4;++corner){
   set_color(clut,40+corner,corners[corner]);
   for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x)source[((corner&2)?31-y:y)*32+((corner&1)?31-x:x)]=std::uint8_t(40+corner);
  }
  f.put(0x400000+4*1024,source);f.put(0x410000+4*1024,clut);
  require(f.build(),"four native beige corners build a smooth Gold gradient");const auto gold=f.variants.golden();
  require(pixel(gold,0)==corners[0]&&pixel(gold,1023)==corners[3],"Gold background keeps the native lightest and darkest corner colors");
  for(unsigned y:{0u,31u})for(unsigned x=1;x<32;++x){
   const auto previous=pixel(gold,y*32+x-1),current=pixel(gold,y*32+x);
   for(unsigned channel=0;channel<3;++channel){
    require(current[channel]<=previous[channel],"background gradient never dithers back toward a brighter adjacent shade");
    require(previous[channel]-current[channel]<=3,"background adjacent gradient steps stay smooth");
   }
  }
  for(unsigned y=9;y<23;++y)for(unsigned x=13;x<19;++x)require(pixel(gold,y*32+x)==Rgba{25,25,24,128},"Gold white-core interior is uniform black without source noise");
 }
 {
  Fixture f;require(f.build(),"initial native source build");f.put<std::uint8_t>(0x400000+2*1024+1023,17);
  require(f.build()&&f.variants.ammo().pixels[1023]==17,"rebuild samples new native label instead of cached prior pixels");
  f.put<std::uint16_t>(images+4*8+4,1);f.failed();
 }
 {
  Fixture f;auto data=f.normal();data.pixels=nullptr;require(!f.variants.build(f.memory,data),"null input pixels rejected");f.empty();
  data=f.normal();data.palette=nullptr;require(!f.variants.build(f.memory,data),"null input palette rejected");
  data=f.normal();data.pixel_bytes=4096;require(!f.variants.build(f.memory,data),"unsupported64x64 rejected");
  data=f.normal();data.palette_bytes=1023;require(!f.variants.build(f.memory,data),"short CLUT rejected");
  f.palette[3]=255;f.failed();
 }
 {
  const std::array<std::pair<unsigned,unsigned>,10> bad{{
   {state+0x18,0},{header+0x44,0},{header+0x24,4097},{state+0x24,0x1fffffc},
   {state+0x28,0xfffffff8},{refs+2*4,0x00050002},{refs+4*4,0x00040005},
   {images+2*8,0x80040000},{palettes+4*8,0x80050000},{images+4*8,0x1fffffc}
  }};
  for(const auto& change:bad){Fixture f;require(f.build(),"valid state before malformed rebuild");f.put(change.first,change.second);f.failed();}
 }
 {
  Fixture f;f.put<std::uint16_t>(defs,60014);f.failed();
 }
 {
  Fixture f;f.put<std::uint16_t>(defs+2,4);f.failed();
 }
 {
  Fixture f;f.put<std::uint16_t>(defs+4,65535);f.failed();
 }
 {
  Fixture f;f.put<std::uint8_t>(images+2*8+6,7);f.failed();
 }
 {
  Fixture f;f.put<std::uint8_t>(images+4*8+7,4);f.failed();
 }
 {
  Fixture f;f.put<std::uint8_t>(0x410000+2*1024+3,255);f.failed();
 }
 {
  Fixture f;Bytes missing{};missing.fill(17);f.put(0x400000+1*1024,missing);f.failed();
 }
 {
  Fixture f;Bytes missing{};missing.fill(8);f.put(0x400000+4*1024,missing);f.failed();
 }
 {
  Fixture f;for(unsigned x=0;x<32;++x)f.put<std::uint8_t>(0x400000+2*1024+5*32+x,17);f.failed();
 }
 native_capture(argc,argv);
 std::printf("native icon variants: %u checks PASS\n",checks);return 0;
}
