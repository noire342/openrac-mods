// SPDX-License-Identifier: GPL-3.0-or-later
// CPU integration of the production scene renderer, not a GLSL/GPU test.
#include "renderer.hpp"
#include "render_math.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include <stdexcept>
using namespace spacevoyages;
namespace {
unsigned checks{},calls{};
void require(bool condition,const char* why){++checks;if(!condition){std::fprintf(stderr,"renderer: %s\n",why);std::exit(1);}}
bool close(float a,float b){return std::abs(a-b)<.0001f;}
template<class T>T empty(){return T{};}template<>void empty<void>(){}
#define DEFAULT(result,name,args) result fallback_##name args{++calls;return empty<result>();}
SPACE_GL_FUNCTIONS(DEFAULT)
#undef DEFAULT
enum class Kind {None,Scene,Flame,Sprite};
struct Uniform {unsigned program{};std::string name;int integer{};std::array<float,16> value{};};
struct Texture {int width{},height{};std::vector<unsigned char> sample;};
struct Program {std::string source;Kind kind=Kind::None;};
struct Array {unsigned vertices{},indices{};};
struct Draw {
 Kind kind=Kind::None;bool ui{},blend_standard{},blend_additive{},depth{},depth_write{};
 int health_mode{},threshold{},image_unit{},palette_unit{},instances{},sphere_uv{};
 std::array<float,16> model{};
 unsigned texture0{},texture1{},active_unit{},vao{},depth_function{};
 std::array<float,4> tint{};float x0{},y0{},x1{},y1{};
};
struct Recording {
 unsigned next=1,program{},vao{},array_buffer{},active_unit{},texture_uploads{},generation_calls{};
 bool depth{},depth_write=true,blend{};unsigned blend_equation=0x8006,depth_function=0x201;std::array<unsigned,4> blend_factors{};
 std::array<unsigned,2> textures_bound{};
 std::array<unsigned,4096> live{};
 std::array<std::string,4096> shaders;
 std::array<Program,4096> programs;
 std::array<Texture,4096> textures;
 std::array<std::vector<unsigned char>,4096> buffers;
 std::array<Array,4096> arrays;
 std::vector<Uniform> uniforms{{}};
 std::vector<Draw> draws;
} recording;
unsigned create(unsigned kind){const auto id=recording.next++;require(id<recording.live.size(),"bounded fake handles");recording.live[id]=kind;return id;}
unsigned create_shader(unsigned){++calls;return create(4);}unsigned create_program(){++calls;return create(5);}
void gen(int n,unsigned* ids,unsigned kind){++calls;for(int i=0;i<n;++i){ids[i]=create(kind);++recording.generation_calls;}}
void buffers(int n,unsigned* ids){gen(n,ids,1);}void arrays(int n,unsigned* ids){gen(n,ids,2);}void textures(int n,unsigned* ids){gen(n,ids,3);}
void destroy(int n,const unsigned* ids,unsigned kind){++calls;for(int i=0;i<n;++i)if(ids[i]){require(recording.live[ids[i]]==kind,"every owned GL name released exactly once");recording.live[ids[i]]=0;}}
void delete_buffers(int n,const unsigned* ids){destroy(n,ids,1);}void delete_arrays(int n,const unsigned* ids){destroy(n,ids,2);}void delete_textures(int n,const unsigned* ids){destroy(n,ids,3);}
void delete_shader(unsigned id){destroy(1,&id,4);}void delete_program(unsigned id){destroy(1,&id,5);}
void shader_source(unsigned id,int count,const char*const* strings,const int* lengths){++calls;for(int i=0;i<count;++i)recording.shaders[id].append(strings[i],lengths&&lengths[i]>=0?std::size_t(lengths[i]):std::strlen(strings[i]));}
void attach(unsigned p,unsigned shader){++calls;recording.programs[p].source+=recording.shaders[shader];}
void link(unsigned id){++calls;auto& p=recording.programs[id];p.kind=p.source.find("health_texel")!=std::string::npos?Kind::Scene:p.source.find("mount_position_radius")!=std::string::npos?Kind::Flame:Kind::Sprite;}
void status(unsigned,unsigned,int* value){++calls;*value=1;}
void use(unsigned p){++calls;recording.program=p;}void bind_array(unsigned a){++calls;recording.vao=a;}
int location(unsigned p,const char* name){++calls;if(recording.programs[p].source.find(name)==std::string::npos)return -1;for(std::size_t i=1;i<recording.uniforms.size();++i)if(recording.uniforms[i].program==p&&recording.uniforms[i].name==name)return int(i);recording.uniforms.push_back({p,name,0,{}});return int(recording.uniforms.size()-1);}
void integer(int id,int value){++calls;if(id>=0)recording.uniforms[std::size_t(id)].integer=value;}
void vector(int id,int,const float* value){++calls;if(id>=0)std::copy(value,value+4,recording.uniforms[std::size_t(id)].value.begin());}
void matrix(int id,int,unsigned char,const float* value){++calls;if(id>=0)std::copy(value,value+16,recording.uniforms[std::size_t(id)].value.begin());}
const Uniform* uniform(unsigned program,const char* name){for(const auto& u:recording.uniforms)if(u.program==program&&u.name==name)return &u;return nullptr;}
int integer_value(const char* name){const auto* u=uniform(recording.program,name);return u?u->integer:0;}
void active_texture(unsigned unit){++calls;require(unit==0x84c0||unit==0x84c1,"scene uses only base texture0 and native palette1");recording.active_unit=unit-0x84c0;}
void bind_texture(unsigned target,unsigned texture){++calls;require(target==0xde1,"only2D textures expected");recording.textures_bound[recording.active_unit]=texture;}
void texture_image(unsigned,int,int,int width,int height,int,unsigned,unsigned,const void* data){++calls;++recording.texture_uploads;auto& dst=recording.textures[recording.textures_bound[recording.active_unit]];dst.width=width;dst.height=height;const auto size=std::min(std::size_t(width)*std::size_t(height)*4,std::size_t(1024));dst.sample.assign(static_cast<const unsigned char*>(data),static_cast<const unsigned char*>(data)+size);}
void bind_buffer(unsigned target,unsigned id){++calls;if(target==0x8892)recording.array_buffer=id;else{require(target==0x8893&&recording.vao,"index buffer binding has real VAO");recording.arrays[recording.vao].indices=id;}}
void buffer_data(unsigned target,std::ptrdiff_t size,const void* data,unsigned){++calls;const auto id=target==0x8892?recording.array_buffer:recording.arrays[recording.vao].indices;auto& dst=recording.buffers[id];dst.resize(std::size_t(size));if(data)std::memcpy(dst.data(),data,std::size_t(size));}
void attribute(unsigned,int,unsigned,unsigned char,int,const void*){++calls;recording.arrays[recording.vao].vertices=recording.array_buffer;}
void enable(unsigned capability){++calls;if(capability==0xb71)recording.depth=true;if(capability==0xbe2)recording.blend=true;}
void disable(unsigned capability){++calls;if(capability==0xb71)recording.depth=false;if(capability==0xbe2)recording.blend=false;}
void depth_mask(unsigned char value){++calls;recording.depth_write=value!=0;}
void depth_function(unsigned value){++calls;recording.depth_function=value;}
void blend(unsigned a,unsigned b,unsigned c,unsigned d){++calls;recording.blend_factors={a,b,c,d};}
void blend_equation(unsigned value){++calls;recording.blend_equation=value;}
void capture(bool ui,int instances){
 ++calls;Draw d;d.kind=recording.programs[recording.program].kind;d.ui=ui;d.instances=instances;d.depth=recording.depth;d.depth_write=recording.depth_write;
 d.blend_standard=recording.blend&&recording.blend_equation==0x8006&&recording.blend_factors==std::array<unsigned,4>{0x302,0x303,1,0x303};d.blend_additive=recording.blend&&recording.blend_equation==0x8006&&recording.blend_factors==std::array<unsigned,4>{0x302,1,1,1};
 d.health_mode=integer_value("health_mode");d.threshold=integer_value("health_threshold");d.image_unit=integer_value("image");d.palette_unit=integer_value("health_palette");d.depth_function=recording.depth_function;
 d.sphere_uv=integer_value("sphere_uv");if(const auto* m=uniform(recording.program,"model"))d.model=m->value;
 d.texture0=recording.textures_bound[0];d.texture1=recording.textures_bound[1];d.active_unit=recording.active_unit;d.vao=recording.vao;
 if(const auto* tint=uniform(recording.program,"tint"))std::copy(tint->value.begin(),tint->value.begin()+4,d.tint.begin());
 if(ui){const auto& bytes=recording.buffers[recording.arrays[recording.vao].vertices];require(bytes.size()%sizeof(Vertex)==0,"UI buffer contains the real scene Vertex layout");d.x0=d.y0=1e20f;d.x1=d.y1=-1e20f;for(std::size_t i=0;i<bytes.size();i+=sizeof(Vertex)){Vertex v{};std::memcpy(&v,bytes.data()+i,sizeof v);d.x0=std::min(d.x0,v.position[0]);d.x1=std::max(d.x1,v.position[0]);d.y0=std::min(d.y0,v.position[1]);d.y1=std::max(d.y1,v.position[1]);}}
 recording.draws.push_back(d);
}
void elements(unsigned,int,unsigned,const void*){capture(false,1);}void instanced(unsigned,int,unsigned,const void*,int count){capture(false,count);}void array_draw(unsigned,int,int){capture(true,1);}
void* loader(const char* name){
#define ENTRY(symbol,function) if(!std::strcmp(name,"gl" #symbol))return reinterpret_cast<void*>(&function)
 ENTRY(CreateShader,create_shader);ENTRY(CreateProgram,create_program);ENTRY(GenBuffers,buffers);ENTRY(GenVertexArrays,arrays);ENTRY(GenTextures,textures);ENTRY(DeleteBuffers,delete_buffers);ENTRY(DeleteVertexArrays,delete_arrays);ENTRY(DeleteTextures,delete_textures);ENTRY(DeleteShader,delete_shader);ENTRY(DeleteProgram,delete_program);
 ENTRY(ShaderSource,shader_source);ENTRY(AttachShader,attach);ENTRY(LinkProgram,link);ENTRY(GetShaderiv,status);ENTRY(GetProgramiv,status);ENTRY(UseProgram,use);ENTRY(BindVertexArray,bind_array);ENTRY(GetUniformLocation,location);ENTRY(Uniform1i,integer);ENTRY(Uniform4fv,vector);ENTRY(UniformMatrix4fv,matrix);ENTRY(ActiveTexture,active_texture);ENTRY(BindTexture,bind_texture);ENTRY(TexImage2D,texture_image);ENTRY(BindBuffer,bind_buffer);ENTRY(BufferData,buffer_data);ENTRY(VertexAttribPointer,attribute);ENTRY(Enable,enable);ENTRY(Disable,disable);ENTRY(DepthMask,depth_mask);ENTRY(DepthFunc,depth_function);ENTRY(BlendFuncSeparate,blend);ENTRY(BlendEquation,blend_equation);ENTRY(DrawElements,elements);ENTRY(DrawElementsInstanced,instanced);ENTRY(DrawArrays,array_draw);
#undef ENTRY
#define FALLBACK(result,symbol,args) if(!std::strcmp(name,"gl" #symbol))return reinterpret_cast<void*>(&fallback_##symbol);
 SPACE_GL_FUNCTIONS(FALLBACK)
#undef FALLBACK
 return nullptr;
}
unsigned live(){unsigned n=0;for(auto item:recording.live)if(item)++n;return n;}
SceneAssets assets(bool health=true,bool effects=true){
 SceneAssets result;result.font_width=result.font_height=16;result.font_rgba.assign(16*16*4,255);result.sky_rgba.assign(2048*1024*4,255);result.radar_rgba.assign(128*128*4,255);
 for(auto& glyph:result.glyphs){glyph.u=glyph.v=glyph.top=0;glyph.advance=8;}
 for(const char* id:{"planet_novalis","planet_kerwan","planet_eudora","ship_531","ship_532","ship_533"}){MeshAsset mesh;mesh.id=id;mesh.width=mesh.height=1;mesh.rgba={73,88,99,255};mesh.texture_key="shared-test-body";mesh.vertices={{{-.5f,0,0},{0,0,1},{0,0}},{{.5f,0,0},{0,0,1},{1,0}},{{0,.5f,0},{0,0,1},{.5f,1}}};mesh.indices={0,1,2};result.meshes.push_back(std::move(mesh));}
 if(health){auto& meter=result.health_meter;meter.width=4;meter.height=2;meter.indices={0,1,2,127,128,253,254,255};meter.palette_rgba.resize(256*4);for(unsigned i=0;i<256;++i){meter.palette_rgba[i*4]=static_cast<unsigned char>(i);meter.palette_rgba[i*4+1]=static_cast<unsigned char>(255-i);meter.palette_rgba[i*4+2]=static_cast<unsigned char>(i^0xa5);meter.palette_rgba[i*4+3]=i?255:0;}}
 if(effects){for(auto* sprite:{&result.ship_effects.flame,&result.ship_effects.hit,&result.ship_effects.explosion}){sprite->width=sprite->height=2;sprite->rgba.assign(16,255);}for(unsigned ship=0;ship<3;++ship){auto& mounted=result.ship_effects.ships[ship];mounted.count=ship==0?8:2;for(std::size_t i=0;i<mounted.count;++i)mounted.mounts[i]={{-2.f,float(i)*.2f,0},{-1,0,0},.1f,.8f};}}
 return result;
}
VoyageController journey(){const auto& catalog=prototype_catalog();return VoyageController(catalog.data(),catalog.size());}
void departure(VoyageController& voyage){std::array<bool,19> unlocked{};unlocked[1]=unlocked[3]=unlocked[4]=true;require(voyage.begin(1,unlocked),"synthetic native-catalog journey begins");}
void check_hud(bool expect_health,bool expect_effects){
 bool saw_effect=false,saw_ui=false,saw_health=false,saw_radar=false;std::size_t first_ui=recording.draws.size(),last_effect=0;const Draw* health=nullptr;const Draw* radar=nullptr;
 for(std::size_t i=0;i<recording.draws.size();++i){const auto& d=recording.draws[i];if(d.kind==Kind::Flame||d.kind==Kind::Sprite){saw_effect=true;last_effect=i;require(d.blend_additive&&d.depth&&!d.depth_write,"real ShipEffects pass uses additive depth-tested/no-write state");}
  if(!d.ui)continue;saw_ui=true;first_ui=std::min(first_ui,i);require(d.kind==Kind::Scene&&d.blend_standard&&!d.depth&&!d.depth_write,"every existing HUD/fade/Pause draw restores standard alpha blend after FX");require(d.active_unit==0&&d.image_unit==0,"HUD returns to base texture unit0");
  const auto& tex=recording.textures[d.texture0];if(d.health_mode){saw_health=true;health=&d;require(tex.width==4&&tex.height==2&&d.palette_unit==1,"health indices are sampled on0 and native CLUT on1");const auto& pal=recording.textures[d.texture1];require(pal.width==256&&pal.height==1,"health palette texture is actually bound on unit1");}else if(tex.width==128&&tex.height==128){saw_radar=true;radar=&d;require(d.health_mode==0,"radar never inherits palette sampling mode");}}
 require(saw_ui&&saw_radar,"real native-style radar survives the effects pass");require(saw_health==expect_health,"health draw is conditional on available native indexed artwork");require(saw_effect==expect_effects,"optional FX integration matches supplied asset availability");if(expect_effects)require(last_effect<first_ui,"all FX finish before health/radar/fade/Pause");if(health&&radar)require(health->x0==radar->x0&&health->y0==radar->y0&&health->x1==radar->x1&&health->y1==radar->y1,"native health arc shares the existing radar quad without another panel");
}
}
int main(){
 {
  auto data=assets();auto body=data.meshes.front();body.id="ship_original_part0";body.texture_key="original-body";body.rgba={242,33,71,255};data.meshes.push_back(body);
  auto glass=body;glass.id="ship_original_part1";glass.texture_key="original-glass";glass.alpha=AlphaMode::Blend;glass.sphere_uv=true;glass.rgba={71,222,163,96};data.meshes.push_back(glass);
  auto crew=body;crew.id="ship_original_part2";crew.texture_key="clank-base";crew.rgba={222,111,33,255};data.meshes.push_back(crew);
  auto chrome=crew;chrome.id="ship_original_part3";chrome.texture_key="clank-chrome";chrome.rgba={223,111,33,128};chrome.alpha=AlphaMode::Blend;chrome.sphere_uv=chrome.metal_overlay=true;data.meshes.push_back(chrome);
  data.original_ship.available=true;data.original_ship.radius=1.45f;data.original_ship.length=1.8022f;
  auto& plume=data.ship_effects.ships[3];plume.count=2;plume.mounts[0]={{-1,.3f,-.4f},{-1,0,0},.22f,.88f};plume.mounts[1]={{-1,-.3f,-.4f},{-1,0,0},.22f,.88f};
  auto voyage=journey();std::array<bool,19> available{};available[1]=available[3]=true;require(voyage.begin(1,available,ShipIdentity::Original)&&voyage.departure_complete(),"explicit Original pure voyage");
  SceneRenderer renderer(data);require(renderer.supports_ship(ShipIdentity::Original),"Original profile is detectable before any GL or menu bypass");renderer.initialize(loader);recording.draws.clear();
  const auto& f=voyage.presentation_flight();renderer.draw(voyage,f,1280,800,ShipIdentity::Original,false,1,0,2);
  const auto model=render_math::model(f.position,f.model_yaw,f.model_pitch,f.model_roll,1);unsigned bodies=0,glasses=0,overlays=0;std::size_t overlay_at=0,glass_at=0,draw_at=0;
  for(const auto& draw:recording.draws){++draw_at;const auto& texture=recording.textures[draw.texture0];if(draw.ui){require(draw.depth_function==0x201,"HUD depth comparison is restored after chrome");require(draw.sphere_uv==0,"HUD/font never inherit Original glass mapping");continue;}
   if(draw.kind!=Kind::Scene||texture.sample.empty())continue;
   if(texture.sample[0]==242){++bodies;require(draw.model==model&&draw.sphere_uv==0,"Original body uses the one interpolated ship matrix and normal UVs");}
   if(texture.sample[0]==71){++glasses;glass_at=draw_at;require(draw.depth_function==0x201,"canopy glass retains ordinary LESS depth");require(draw.model==model&&draw.sphere_uv==1&&draw.blend_standard&&!draw.depth_write,"only explicit glass uses native spherical UV with standard alpha and no depth writes");}
   if(texture.sample[0]==223){++overlays;overlay_at=draw_at;require(draw.depth_function==0x203&&draw.blend_standard&&!draw.depth_write,"coplanar chrome uses LEQUAL without writing depth");}
  }
  require(overlays==1&&overlay_at<glass_at&&recording.depth_function==0x201,"chrome overlays complete before canopy glass and restore LESS");
  require(bodies==1&&glasses==1&&renderer.last_stats().thrusters==2,"Original draws one own hull/glass and its two own exhausts, without replacing it with Green");
  const auto before=calls;renderer.draw(voyage,f,1280,800,2047,false,1,0);require(calls==before,"legacy int2047 never clamps to Green or fakes an Original identity");
  data.original_ship.available=false;require(!renderer.supports_ship(ShipIdentity::Original),"incomplete pack cannot enable Original");bool refused=false;try{renderer.draw(voyage,f,1280,800,ShipIdentity::Original,false,1,0);}catch(const std::runtime_error&){refused=true;}
  require(refused&&calls==before,"missing Original availability rejects before drawing any fallback mesh");renderer.close();require(live()==0,"Original materials and exhaust resources close exactly once");
 }
 {
  auto data=assets();SceneRenderer renderer(data);auto voyage=journey();
  for(int trip=0;trip<2;++trip){
   departure(voyage);require(voyage.departure_complete()&&voyage.candidate(),"context prompt fixture has an actual eligible destination");renderer.initialize(loader);
   for(const auto size:{std::array<int,2>{1024,768},std::array<int,2>{1280,720},std::array<int,2>{1280,800}}){
    recording.blend_equation=0x800b; // Native subtractive effects may leave this behind.
    recording.draws.clear();renderer.draw(voyage,voyage.flight(),size[0],size[1],532,false,1,0);
    const auto& shown=renderer.last_stats();unsigned submitted=0;
    for(const auto& draw:recording.draws)if(draw.ui&&draw.texture0==shown.font_texture){++submitted;require(draw.blend_standard&&!draw.depth&&draw.health_mode==0,"context glyphs use standard visible HUD state after native health palette");}
    require(shown.prompt_candidate==voyage.candidate()->level&&shown.prompt_glyphs>5&&submitted==shown.prompt_glyphs*2,"prompt diagnostics count actual foreground and shadow glyph draws");
    require(shown.prompt_alpha==1&&shown.prompt_x>=0&&shown.prompt_x+shown.prompt_width<=416.f*size[0]/size[1]&&shown.prompt_y+16<416,"eligible prompt fits all supported aspect ratios at full opacity");
   }
   for(int frame=0;frame<600;++frame)voyage.tick(1./60,{},false);
   require(!voyage.candidate(),"fixture flies beyond the source context range");recording.draws.clear();renderer.draw(voyage,voyage.flight(),1280,720,532,false,1,0);
   require(renderer.last_stats().prompt_candidate==-1&&renderer.last_stats().prompt_glyphs==0,"old prompt diagnostics clear when the candidate leaves range");
   renderer.close();require(live()==0,"consecutive prompt scene releases every GPU handle");voyage.cancel();
  }
 }
 {
  auto data=assets();SceneRenderer renderer(data);auto voyage=journey();departure(voyage);
  const auto before=calls;renderer.draw(voyage,voyage.flight(),1280,720,531,false,0,0);require(calls==before,"uninitialized renderer returns without GL");
  renderer.initialize(loader);const auto texture_uploads=recording.texture_uploads;
  for(const auto size:{std::array<int,2>{0,720},std::array<int,2>{1280,0},std::array<int,2>{-1,720}}){const auto prior=calls;renderer.draw(voyage,voyage.flight(),size[0],size[1],531,false,0,0);require(calls==prior&&renderer.last_stats().draws==0,"invalid picture sizes perform no GL work");}
  ShipDamageState hull;hull.max_hull=256;hull.hull=17;
  for(const auto sample:{std::array<int,2>{256,253},std::array<int,2>{128,127},std::array<int,2>{0,2}}){
   hull.hud_hull=float(sample[0]);recording.draws.clear();renderer.draw(voyage,voyage.flight(),1280,720,531,false,0,0,2,&hull);check_hud(true,true);
   unsigned health_draws=0;for(const auto& d:recording.draws)if(d.ui&&d.health_mode){++health_draws;require(d.threshold==sample[1],"actual shader receives native CLUT threshold from eased HUD health, not physical hull");const auto& index=recording.textures[d.texture0];for(std::size_t i=0;i<data.health_meter.indices.size();++i)require(index.sample[i*4]==data.health_meter.indices[i],"index0/1/254/255 and every source index survive upload unchanged");const auto& palette=recording.textures[d.texture1];require(palette.sample==data.health_meter.palette_rgba,"full original CLUT survives every health level without CPU recoloring");}
   require(health_draws==1&&recording.texture_uploads==texture_uploads,"health updates use one quad/uniform and never re-upload palette or image");
   const auto& black=recording.draws.back();require(black.ui&&black.tint==std::array<float,4>{0,0,0,1}&&black.x0==0&&black.y0==0&&close(black.x1,416.f*1280/720)&&black.y1==416,"entry black remains the final full-screen alpha1 draw after active exhaust");
  }
  require(voyage.departure_complete(),"free-flight presentation available");
  hull.hud_hull=128;hull.hit_flash=1;recording.draws.clear();renderer.draw(voyage,voyage.flight(),1280,720,532,true,1,0,3,&hull);check_hud(true,true);
  bool pause_overlay=false;for(const auto& d:recording.draws)if(d.ui&&d.tint==std::array<float,4>{0,0,0,.48f}&&d.x0==0&&d.y0==0)pause_overlay=true;require(pause_overlay,"pause darkening uses standard alpha after additive hit and exhaust");
  hull.destroyed=true;hull.destruction_progress=.75f;recording.draws.clear();renderer.draw(voyage,voyage.flight(),1280,720,533,false,1,0,3,&hull);check_hud(true,true);require(renderer.last_stats().thrusters==0&&renderer.last_stats().effect_sprites==1,"destruction hides exhaust and presents optional native explosion before HUD/fade");
  hull.destruction_progress=1;recording.draws.clear();renderer.draw(voyage,voyage.flight(),1280,720,533,false,1,0,3,&hull);check_hud(true,false);const auto& black=recording.draws.back();require(black.tint==std::array<float,4>{0,0,0,1}&&black.blend_standard,"final destruction black is opaque even after previous additive effects");
  renderer.close();const auto ended=calls;renderer.close();require(live()==0&&calls==ended,"full scene/effects close releases every handle exactly once");
 }
 for(bool flame:{false,true}){
  auto data=assets(false,flame);SceneRenderer renderer(data);auto voyage=journey();departure(voyage);voyage.departure_complete();renderer.initialize(loader);recording.draws.clear();renderer.draw(voyage,voyage.flight(),1024,768,531,false,1,0,0);check_hud(false,flame);renderer.close();require(live()==0,"legacy absent health artwork does not leak zero/real texture handles");
 }
 {
  auto data=assets(true,true);SceneRenderer renderer(data);auto voyage=journey();voyage.set_asteroid_damage(false);departure(voyage);voyage.departure_complete();renderer.initialize(loader,nullptr,false);recording.draws.clear();renderer.draw(voyage,voyage.flight(),1280,800,531,false,1,0,0);check_hud(false,false);require(renderer.last_stats().thrusters==0,"disabled engine flames stay disabled while other legacy drawing continues");renderer.close();require(live()==0,"configuration-off path cleans up all optional GPU resources");
 }
 std::printf("PASS %u checks; actual SceneRenderer integration: additive-to-HUD blend restore, texture units0/1 and native CLUT, opaque final black/Pause, optional assets, invalid sizes and cleanup. Recording facade only; no GLSL/GPU proof and no whole-renderer allocation claim.\n",checks);
}
