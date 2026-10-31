// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "native_icon_variants.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace revolverator {
namespace {
constexpr unsigned side=32,label_row=24;
using Bytes=std::array<std::uint8_t,1024>;
struct Color {int r{},g{},b{},a{};};
struct NativeIcon {Bytes pixels{},palette{};};
unsigned physical(unsigned logical){return (logical&~24u)|((logical&8u)<<1)|((logical&16u)>>1);}
Color color(const std::uint8_t* palette,unsigned logical){
 const auto at=physical(logical)*4;return {palette[at],palette[at+1],palette[at+2],palette[at+3]};
}
int luma(Color c){return c.r*3+c.g*6+c.b;}
bool blue(Color c){return c.a&&c.b-c.r>28&&c.b-c.g>28&&c.b>100;}
bool beige(Color c){return c.a&&c.r-c.b>30&&c.g-c.b>20;}
bool dark(Color c){return c.a&&luma(c)<640;}
bool white(Color c){return c.a&&c.r>180&&c.g>180&&c.b>180&&std::max({c.r,c.g,c.b})-std::min({c.r,c.g,c.b})<20;}
std::array<unsigned,256> frequencies(const NativeIcon& icon,unsigned limit=1024){
 std::array<unsigned,256> result{};for(unsigned i=0;i<limit;++i)++result[icon.pixels[i]];return result;
}
bool load_icon(const Memory& memory,unsigned frame,NativeIcon& icon){
 const auto state=memory.global(0x19a3e8);
 if(!Memory::valid(state,0x2c))return false;
 const auto header=memory.read<unsigned>(state+0x18),defs=memory.read<unsigned>(state+0x1c);
 const auto refs=memory.read<unsigned>(state+0x20),images=memory.read<unsigned>(state+0x24),palettes=memory.read<unsigned>(state+0x28);
 if(!Memory::valid(header,0x48)||!Memory::valid(defs,8)||!Memory::valid(refs,4))return false;
 const auto image_count=memory.read<unsigned>(header+0x44),palette_count=memory.read<unsigned>(header+0x24);
 if(!image_count||image_count>4096||!palette_count||palette_count>4096||
    !Memory::valid(images,image_count*8)||!Memory::valid(palettes,palette_count*8))return false;
 for(unsigned i=0;i<1024;++i){
  const auto at=defs+i*8;if(!Memory::valid(at,8))return false;
  const auto bank=memory.read<std::uint16_t>(at);if(bank==0xffff)return false;
  if(bank!=60015)continue;
  const auto count=memory.read<std::uint16_t>(at+2),start=memory.read<std::uint16_t>(at+4);
  if(count<5||frame>=count||unsigned(start)+count>65536)return false;
  const auto ref_at=refs+(unsigned(start)+frame)*4;if(!Memory::valid(ref_at,4))return false;
  const auto ref=memory.read<unsigned>(ref_at),image=ref>>16,palette=ref&0xffff;
  if(image>=image_count||palette>=palette_count)return false;
  const auto page=images+image*8;
  // Cached GS pages are not CPU source pixels. Only the known source frames
  // 60015:1/:2/:4, still backed by their original32x32 bytes, are accepted.
  if(memory.read<std::uint16_t>(page+4)||memory.read<std::uint8_t>(page+6)!=5||
     memory.read<std::uint8_t>(page+7)!=5)return false;
  const auto pixels=memory.read<unsigned>(page),clut=memory.read<unsigned>(palettes+palette*8);
  if((pixels|clut)&0x80000000u||!Memory::valid(pixels,1024)||!Memory::valid(clut,1024))return false;
  icon.pixels=memory.read<Bytes>(pixels);icon.palette=memory.read<Bytes>(clut);
  for(unsigned j=3;j<1024;j+=4)if(icon.palette[j]>128)return false;
  return true;
 }
 return false;
}
template<class Predicate>
bool representative(const NativeIcon& icon,const std::array<unsigned,256>& frequency,Predicate predicate,Color& result){
 unsigned best=0;
 for(unsigned i=0;i<256;++i){const auto c=color(icon.palette.data(),i);if(frequency[i]>best&&predicate(c)){best=frequency[i];result=c;}}
 return best!=0;
}
unsigned nearest(const NativeIcon& icon,const std::array<unsigned,256>& frequency,double r,double g,double b){
 double best=std::numeric_limits<double>::max();unsigned result=0;
 for(unsigned i=0;i<256;++i){
  const auto c=color(icon.palette.data(),i);if(!frequency[i]||c.a!=128)continue;
  const auto distance=(c.r-r)*(c.r-r)+(c.g-g)*(c.g-g)+(c.b-b)*(c.b-b);
  if(distance<best){best=distance;result=i;}
 }
 return result;
}
double coverage(Color c,double blue_ratio,double white_ratio){
 if(!c.a)return 0;
 if(c.b<64)return 1;
 const double ratio=double(std::max(c.r,c.g))/c.b;
 return std::clamp((ratio-blue_ratio)/(white_ratio-blue_ratio),0.0,1.0);
}
void set_color(Bytes& palette,unsigned index,double r,double g,double b,unsigned alpha=128){
 const auto at=physical(index)*4;
 palette[at]=std::uint8_t(std::clamp(std::lround(r),0l,255l));
 palette[at+1]=std::uint8_t(std::clamp(std::lround(g),0l,255l));
 palette[at+2]=std::uint8_t(std::clamp(std::lround(b),0l,255l));palette[at+3]=std::uint8_t(alpha);
}
bool bake_icon(const NativeIcon& source,const HdIconData& normal,Color foreground,bool golden,double blue_ratio,double white_ratio,
               std::array<std::uint8_t,16384>& pixels,Bytes& palette){
 // Sample only the native background in each corner. The original tile's
 // illumination, border, and reflection never contribute to this background.
 std::array<Color,4> corners{};
 for(unsigned corner=0;corner<4;++corner){
  unsigned count=0;
  for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x){
   const auto sx=(corner&1u)?31-x:x,sy=(corner&2u)?31-y:y;
   const auto c=color(source.palette.data(),source.pixels[sy*32+sx]);if(golden?!beige(c):!blue(c))continue;
   corners[corner].r+=c.r;corners[corner].g+=c.g;corners[corner].b+=c.b;++count;
  }
  if(!count)return false;
  corners[corner].r/=int(count);corners[corner].g/=int(count);corners[corner].b/=int(count);
 }
 auto darkest=corners[0],lightest=corners[0];
 for(const auto c:corners){if(luma(c)<luma(darkest))darkest=c;if(luma(c)>luma(lightest))lightest=c;}
 palette.fill(0);
 const auto background=[&](double t){return std::array<double,3>{
  darkest.r+(lightest.r-darkest.r)*t,darkest.g+(lightest.g-darkest.g)*t,darkest.b+(lightest.b-darkest.b)*t};};
 //64 smooth background tones and7x27 coverage/shade pairs. The spatially
 // stable palette has no dithering or source-color-dependent quantization.
 for(unsigned i=0;i<64;++i){const auto c=background(double(i)/63);set_color(palette,1+i,c[0],c[1],c[2]);}
 set_color(palette,65,foreground.r,foreground.g,foreground.b);
 for(unsigned coverage_level=1;coverage_level<=7;++coverage_level)for(unsigned shade=0;shade<27;++shade){
  const auto c=background(double(shade)/26);const double t=double(coverage_level)/8;
  set_color(palette,66+(coverage_level-1)*27+shade,c[0]*(1-t)+foreground.r*t,c[1]*(1-t)+foreground.g*t,c[2]*(1-t)+foreground.b*t);
 }
 std::array<double,256> glyph{};
 for(unsigned i=0;i<256;++i){
  const auto c=color(normal.palette,i);
  // Dark/neutral edging belongs to the old blue frame, not to the white
  // symbol. Snap the white core to a solid glyph and keep a clean AA fringe.
  if(c.b<128||std::min(c.r,c.g)<96||!c.a)continue;
  const auto t=std::clamp((coverage(c,blue_ratio,white_ratio)-0.15)/0.7,0.0,1.0);
  glyph[i]=t*t*(3-2*t)*c.a/128.0;
 }
 const unsigned width=normal.pixel_bytes==16384?128:32;
 std::array<std::uint8_t,16384> mask{},visited{};
 std::array<std::uint16_t,16384> queue{};
 for(unsigned i=0;i<normal.pixel_bytes;++i)mask[i]=std::uint8_t(std::lround(glyph[normal.pixels[i]]*255));
 for(unsigned origin=0;origin<normal.pixel_bytes;++origin){
  if(mask[origin]<20||visited[origin])continue;
  unsigned head=0,count=1,min_x=width,min_y=width,max_x=0,max_y=0;bool white_core=false;
  queue[0]=std::uint16_t(origin);visited[origin]=1;
  while(head<count){
   const unsigned index=queue[head++],x=index%width,y=index/width;
   min_x=std::min(min_x,x);max_x=std::max(max_x,x);min_y=std::min(min_y,y);max_y=std::max(max_y,y);
   const auto c=color(normal.palette,normal.pixels[index]);
   white_core=white_core||(mask[index]>=224&&std::min({c.r,c.g,c.b})>=180);
   for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){
    const int nx=int(x)+dx,ny=int(y)+dy;
    if(nx<0||ny<0||nx>=int(width)||ny>=int(width))continue;
    const unsigned next=unsigned(ny)*width+unsigned(nx);
    if(mask[next]>=20&&!visited[next]){visited[next]=1;queue[count++]=std::uint16_t(next);}
   }
  }
  const unsigned box_width=max_x-min_x+1,box_height=max_y-min_y+1;
  // White emblem components have an area and a real body. Isolated glints
  // and the thin source tile frame are excluded from every clean variant.
  const bool frame_bar=((min_y<width/8||max_y>=width-width/8)&&box_width>width/2&&box_height<=width/8)||
   ((min_x<width/8||max_x>=width-width/8)&&box_height>width/2&&box_width<=width/8);
  const bool keep=!frame_bar&&white_core&&count>=std::max(3u,width*width/1024)&&
   box_width>=std::max(2u,width/32)&&box_height>=std::max(2u,width/32)&&count*6>=box_width*box_height;
  for(unsigned i=0;i<count;++i){visited[queue[i]]=keep?2:1;if(!keep)mask[queue[i]]=0;}
 }
 for(unsigned index=0;index<normal.pixel_bytes;++index){
  if(mask[index]>=20)continue;
  const unsigned x=index%width,y=index/width;bool adjacent=false;
  for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){
   const int nx=int(x)+dx,ny=int(y)+dy;
   if(nx>=0&&ny>=0&&nx<int(width)&&ny<int(width)&&visited[unsigned(ny)*width+unsigned(nx)]==2)adjacent=true;
  }
  if(!adjacent)mask[index]=0;
 }
 for(unsigned y=0;y<width;++y)for(unsigned x=0;x<width;++x){
  const double fx=double(x)/(width-1),fy=double(y)/(width-1);
  const double top=luma(corners[0])*(1-fx)+luma(corners[1])*fx;
  const double bottom=luma(corners[2])*(1-fx)+luma(corners[3])*fx;
  const double value=top*(1-fy)+bottom*fy;
  const auto shade=luma(lightest)==luma(darkest)?0.5:std::clamp((value-luma(darkest))/(luma(lightest)-luma(darkest)),0.0,1.0);
  const auto t=mask[y*width+x]/255.0;
  const unsigned level=unsigned(std::lround(t*8));
  pixels[y*width+x]=std::uint8_t(!level?1+unsigned(std::lround(shade*63)):
   level>=8?65:66+(level-1)*27+unsigned(std::lround(shade*26)));
 }
 return true;
}
}

bool NativeIconVariants::build(const Memory& memory,const HdIconData& normal){
 reset();
 if(!normal.pixels||!normal.palette||normal.palette_bytes!=1024||
    (normal.pixel_bytes!=1024&&normal.pixel_bytes!=16384))return false;
 for(unsigned i=3;i<1024;i+=4)if(normal.palette[i]>128)return false;
 NativeIcon native_normal,native_ammo,native_gold;
 if(!load_icon(memory,1,native_normal)||!load_icon(memory,2,native_ammo)||!load_icon(memory,4,native_gold))return false;
 const auto normal_frequency=frequencies(native_normal),ammo_frequency=frequencies(native_ammo),gold_frequency=frequencies(native_gold);
 Color ammo_black,gold_black,normal_white;
 if(!representative(native_ammo,frequencies(native_ammo,label_row*side),dark,ammo_black)||
    !representative(native_gold,gold_frequency,dark,gold_black)||
    !representative(native_normal,normal_frequency,white,normal_white))return false;
 double blue_ratio=0,white_ratio=0;unsigned blues=0,whites=0;
 for(unsigned i=0;i<256;++i){
  const auto c=color(native_normal.palette.data(),i);const auto weight=normal_frequency[i];
  if(blue(c)){blue_ratio+=double(std::max(c.r,c.g))/c.b*weight;blues+=weight;}
  if(white(c)){
   white_ratio+=double(std::max(c.r,c.g))/c.b*weight;whites+=weight;
  }
 }
 if(!blues||!whites)return false;
 blue_ratio=std::clamp(blue_ratio/blues+0.1,0.4,0.85);white_ratio/=whites;
 if(white_ratio<=blue_ratio+0.05)return false;
 std::array<double,256> mask{};
 for(unsigned i=0;i<256;++i)mask[i]=coverage(color(normal.palette,i),blue_ratio,white_ratio);
 if(!bake_icon(native_gold,normal,gold_black,true,blue_ratio,white_ratio,gold_pixels_,gold_palette_)||
    !bake_icon(native_normal,normal,normal_white,false,blue_ratio,white_ratio,normal_pixels_,normal_palette_))return false;
 ammo_palette_=native_ammo.palette;ammo_pixels_=native_ammo.pixels;
 const unsigned width=normal.pixel_bytes==16384?128:32,edge=width/16,span=width-edge*2;
 for(unsigned y=0;y<label_row;++y){
  // Remove the template crosshair, retaining its native blue gradient.
  // The letters occupy the last8 rows and are never sampled or redrawn.
  int left=-1,right=-1;
  for(unsigned x=0;x<side;++x)if(blue(color(native_ammo.palette.data(),native_ammo.pixels[y*side+x]))){
   if(left<0)left=int(x);
   right=int(x);
  }
  if(left<0)return false;
  const auto left_color=color(native_ammo.palette.data(),native_ammo.pixels[y*side+unsigned(left)]);
  const auto right_color=color(native_ammo.palette.data(),native_ammo.pixels[y*side+unsigned(right)]);
  for(unsigned x=0;x<side;++x){
   const double fraction=right==left?0.0:std::clamp(double(int(x)-left)/(right-left),0.0,1.0);
   const double r=left_color.r*(1-fraction)+right_color.r*fraction;
   const double g=left_color.g*(1-fraction)+right_color.g*fraction;
   const double b=left_color.b*(1-fraction)+right_color.b*fraction;
   double t=0;
   if(x>=4&&x<28){
    // Area sampling preserves the small native silhouette at both32 and128
    // source resolutions. Only its outer2/32 frame is cropped for the label.
    const double x0=edge+double((x-4)*span)/24,x1=edge+double((x-3)*span)/24;
    const double y0=edge+double(y*span)/24,y1=edge+double((y+1)*span)/24;
    for(unsigned sy=unsigned(std::floor(y0));sy<unsigned(std::ceil(y1));++sy)
     for(unsigned sx=unsigned(std::floor(x0));sx<unsigned(std::ceil(x1));++sx){
      const double weight=(std::min(x1,double(sx+1))-std::max(x0,double(sx)))*
                          (std::min(y1,double(sy+1))-std::max(y0,double(sy)));
      const auto index=normal.pixels[sy*width+sx];
      t+=mask[index]*(color(normal.palette,index).a/128.0)*weight;
     }
    t=std::clamp(t/((x1-x0)*(y1-y0)),0.0,1.0);
   }
   ammo_pixels_[y*side+x]=std::uint8_t(nearest(native_ammo,ammo_frequency,
       r*(1-t)+ammo_black.r*t,g*(1-t)+ammo_black.g*t,b*(1-t)+ammo_black.b*t));
  }
 }
 normal_bytes_=normal.pixel_bytes;ready_=true;return true;
}
HdIconData NativeIconVariants::normal()const{
 return ready_?HdIconData{normal_pixels_.data(),normal_bytes_,normal_palette_.data(),normal_palette_.size()}:HdIconData{};
}
HdIconData NativeIconVariants::ammo()const{
 return ready_?HdIconData{ammo_pixels_.data(),ammo_pixels_.size(),ammo_palette_.data(),ammo_palette_.size()}:HdIconData{};
}
HdIconData NativeIconVariants::golden()const{
 return ready_?HdIconData{gold_pixels_.data(),normal_bytes_,gold_palette_.data(),gold_palette_.size()}:HdIconData{};
}
void NativeIconVariants::reset(){ready_=false;normal_bytes_=0;}
}
