// SPDX-License-Identifier: GPL-3.0-or-later
#include "ship_effects.hpp"
#include "render_math.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>
#include <vector>
using namespace spacevoyages;
namespace {bool watch_allocations=false;std::size_t frame_allocations{};unsigned checks{};}
void* operator new(std::size_t size){if(watch_allocations)++frame_allocations;if(auto p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
namespace {
void require(bool condition,const char* why){++checks;if(!condition){std::fprintf(stderr,"ship_effects: %s\n",why);std::exit(1);}}
bool close(float a,float b,float tolerance=.00002f){return std::abs(a-b)<tolerance;}
bool close(Vec3 a,Vec3 b,float tolerance=.00002f){return length(a-b)<tolerance;}
template<class T>T empty(){return T{};}template<>void empty<void>(){}
#define DEFAULT(result,name,args) result fallback_##name args{return empty<result>();}
SPACE_GL_FUNCTIONS(DEFAULT)
#undef DEFAULT
struct Attribute {unsigned buffer{},divisor{};int stride{};std::size_t offset{};};
struct Array {std::array<Attribute,5> attributes{};unsigned elements{};};
struct Recording {
 unsigned next=1,loads{},generations{},fail_generation{},uploads{},texture_uploads{},array_buffer{},vao{},program{};
 unsigned instanced_draws{},sprite_draws{},instances{};bool compile_fail{},device_fail{},depth{},write_depth=true,blend{},additive{};
 std::array<unsigned,4096> live{};
 std::array<std::vector<unsigned char>,4096> buffers;
 std::array<Array,4096> arrays{};
 std::array<float,16> model{};
 std::array<float,4> time_boost{};
 std::array<float,4> tint{},last_flame_tint{},last_sprite_tint{};
} recording;
unsigned create(unsigned kind){const auto id=recording.next++;require(id<recording.live.size(),"bounded recording names");recording.live[id]=kind;return id;}
unsigned shader(unsigned){return create(4);}unsigned program(){return create(5);}
void generated(int count,unsigned* names,unsigned kind){for(int i=0;i<count;++i){if(++recording.generations==recording.fail_generation)names[i]=0;else names[i]=create(kind);}}
void gen_buffers(int n,unsigned* names){generated(n,names,1);}void gen_arrays(int n,unsigned* names){generated(n,names,2);}void gen_textures(int n,unsigned* names){generated(n,names,3);}
void destroyed(int count,const unsigned* names,unsigned kind){for(int i=0;i<count;++i){require(names[i]&&recording.live[names[i]]==kind,"each owned handle destroyed once");recording.live[names[i]]=0;}}
void delete_buffers(int n,const unsigned* names){destroyed(n,names,1);}void delete_arrays(int n,const unsigned* names){destroyed(n,names,2);}void delete_textures(int n,const unsigned* names){destroyed(n,names,3);}
void delete_shader(unsigned name){destroyed(1,&name,4);}void delete_program(unsigned name){destroyed(1,&name,5);}
void shader_status(unsigned,unsigned,int* status){*status=recording.compile_fail?0:1;}void program_status(unsigned,unsigned,int* status){*status=1;}
void use(unsigned name){recording.program=name;}void bind_array(unsigned name){recording.vao=name;}
void bind_buffer(unsigned target,unsigned name){if(target==0x8892)recording.array_buffer=name;else{require(target==0x8893&&recording.vao,"element buffer belongs to a real VAO");recording.arrays[recording.vao].elements=name;}}
void buffer_data(unsigned target,std::ptrdiff_t size,const void* data,unsigned usage){require(size>0&&usage==0x88e4,"all effect geometry is static");const auto name=target==0x8892?recording.array_buffer:recording.arrays[recording.vao].elements;auto& bytes=recording.buffers[name];bytes.resize(std::size_t(size));std::memcpy(bytes.data(),data,std::size_t(size));++recording.uploads;}
void pointer(unsigned location,int,unsigned,unsigned char,int stride,const void* offset){require(recording.vao&&location<5,"effect VAO attribute range");auto& a=recording.arrays[recording.vao].attributes[location];a.buffer=recording.array_buffer;a.stride=stride;a.offset=reinterpret_cast<std::size_t>(offset);}
void divisor(unsigned location,unsigned value){require(location>=2&&location<=4&&value==1,"mount attributes advance per nozzle");recording.arrays[recording.vao].attributes[location].divisor=value;}
void texture_image(unsigned,int,int,int,int,int,unsigned,unsigned,const void*){++recording.texture_uploads;}
int location(unsigned,const char* name){if(!std::strcmp(name,"ship_model"))return 1;if(!std::strcmp(name,"time_boost"))return 2;if(!std::strcmp(name,"effect_tint"))return 4;return 3;}
void matrix(int id,int,unsigned char,const float* values){if(id==1)std::memcpy(recording.model.data(),values,16*sizeof(float));}
void vector(int id,int,const float* values){if(id==2)std::memcpy(recording.time_boost.data(),values,4*sizeof(float));if(id==4)std::memcpy(recording.tint.data(),values,4*sizeof(float));}
void enable(unsigned capability){if(capability==0xb71)recording.depth=true;if(capability==0xbe2)recording.blend=true;}
void disable(unsigned capability){if(capability==0xb71)recording.depth=false;if(capability==0xbe2)recording.blend=false;}
void depth_mask(unsigned char enabled){recording.write_depth=enabled!=0;}
void blend(unsigned a,unsigned b,unsigned c,unsigned d){recording.additive=a==0x302&&b==1&&c==1&&d==1;}
void draw_state(){require(recording.depth&&!recording.write_depth&&recording.blend&&recording.additive,"flames/effects test native hull depth, write no depth and use additive blending");require(recording.vao&&recording.program,"draw has live program/array");if(recording.device_fail)throw std::runtime_error("device failure");}
void instanced(unsigned mode,int count,unsigned type,const void*,int instances){draw_state();require(mode==4&&count==96&&type==0x1405&&instances>0,"static eight-sided cone draws the selected ship's nozzle count");const auto& vao=recording.arrays[recording.vao];for(unsigned i=2;i<5;++i){const auto& a=vao.attributes[i];require(a.divisor==1&&a.stride==48&&a.offset==(i-2)*16,"mount buffer48byte stride and fixed offsets");require(recording.buffers[a.buffer].size()>=std::size_t(instances)*48,"mount instance fetch stays inside its buffer");}++recording.instanced_draws;recording.instances=unsigned(instances);recording.last_flame_tint=recording.tint;}
void indexed(unsigned mode,int count,unsigned type,const void*){draw_state();require(mode==4&&count==6&&type==0x1405,"optional native sprite is one static quad");++recording.sprite_draws;recording.last_sprite_tint=recording.tint;}
void* loader(const char* name){++recording.loads;
#define ENTRY(symbol,function) if(!std::strcmp(name,"gl" #symbol))return reinterpret_cast<void*>(&function)
 ENTRY(CreateShader,shader);ENTRY(CreateProgram,program);ENTRY(GenBuffers,gen_buffers);ENTRY(GenVertexArrays,gen_arrays);ENTRY(GenTextures,gen_textures);
 ENTRY(DeleteBuffers,delete_buffers);ENTRY(DeleteVertexArrays,delete_arrays);ENTRY(DeleteTextures,delete_textures);ENTRY(DeleteShader,delete_shader);ENTRY(DeleteProgram,delete_program);
 ENTRY(GetShaderiv,shader_status);ENTRY(GetProgramiv,program_status);ENTRY(UseProgram,use);ENTRY(BindVertexArray,bind_array);ENTRY(BindBuffer,bind_buffer);ENTRY(BufferData,buffer_data);ENTRY(VertexAttribPointer,pointer);ENTRY(VertexAttribDivisor,divisor);
 ENTRY(TexImage2D,texture_image);ENTRY(GetUniformLocation,location);ENTRY(UniformMatrix4fv,matrix);ENTRY(Uniform4fv,vector);ENTRY(Enable,enable);ENTRY(Disable,disable);ENTRY(DepthMask,depth_mask);ENTRY(BlendFuncSeparate,blend);ENTRY(DrawElementsInstanced,instanced);ENTRY(DrawElements,indexed);
#undef ENTRY
#define FALLBACK(result,symbol,args) if(!std::strcmp(name,"gl" #symbol))return reinterpret_cast<void*>(&fallback_##symbol);
 SPACE_GL_FUNCTIONS(FALLBACK)
#undef FALLBACK
 return nullptr;
}
unsigned live(){unsigned n=0;for(auto kind:recording.live)if(kind)++n;return n;}
ShipEffectsAssets synthetic(){
 ShipEffectsAssets result;
 for(auto* sprite:{&result.flame,&result.hit,&result.explosion}){sprite->width=sprite->height=4;sprite->columns=sprite->rows=2;sprite->frames=4;sprite->fps=12;sprite->rgba.assign(64,255);}
 for(std::size_t ship=0;ship<3;++ship){auto& mounts=result.ships[ship];mounts.count=ship+2;mounts.color=ship==2?std::array<float,4>{0,1,.375f,1}:std::array<float,4>{1.375f,.6875f,.25f,1};for(std::size_t i=0;i<mounts.count;++i)mounts.mounts[i]={{-2.f-float(ship)*.4f,float(i)-.5f,.2f},{-1,.05f*float(ship),0},.12f,.9f};}
 return result;
}
constexpr ShipEffects::Matrix identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
Vec3 independent_euler(Vec3 p,Vec3 origin,float yaw,float pitch,float roll){
 const Vec3 x{p.x,p.y*std::cos(roll)-p.z*std::sin(roll),p.y*std::sin(roll)+p.z*std::cos(roll)};
 const Vec3 y{x.x*std::cos(pitch)+x.z*std::sin(pitch),x.y,-x.x*std::sin(pitch)+x.z*std::cos(pitch)};
 return origin+Vec3{y.x*std::cos(yaw)-y.y*std::sin(yaw),y.x*std::sin(yaw)+y.y*std::cos(yaw),y.z};
}
}
int main(){
 const auto assets=synthetic();
 {
  auto explicit_assets=assets;auto& original=explicit_assets.ships[3];original=assets.ships[0];original.color={.9f,.4f,.1f,1};
  original.mounts[0].position={-.75f,.33f,-.39f};original.mounts[1].position={-.75f,-.33f,-.39f};
  ShipEffects effects;effects.initialize(loader,explicit_assets);const auto model=render_math::model({3,4,5},.3f,-.6f,1.2f,1);
  const auto uploads=recording.uploads;watch_allocations=true;
  for(int i=0;i<100;++i)effects.draw(identity,model,{},ShipIdentity::Original,.8f,3.f);
  watch_allocations=false;require(effects.last_stats().thrusters==2&&recording.last_flame_tint==original.color&&recording.model==model,"Original selects appended slot3 with its own two nozzles and exact hull pose");
  require(recording.uploads==uploads&&frame_allocations==0,"Original draw reuses static instances without uploads or heap allocations");
  effects.draw(identity,model,{},531,.8f,3.f);require(recording.last_flame_tint==assets.ships[0].color,"legacy531 remains Green rather than inferring Original");
  effects.draw(identity,model,{},2047,.8f,3.f);require(effects.last_stats().draws==0,"visible class2047 is not a legacy native carrier alias");
 }
 for(unsigned ship=0;ship<3;++ship)for(float yaw:{0.f,.4f,1.7f,pi})for(float pitch:{-.7f,0.f,.8f})for(float roll:{-pi,-1.f,0.f,1.5f,pi}){
  const Vec3 origin{8,-7,4};const auto model=render_math::model(origin,yaw,pitch,roll,1);
  for(std::size_t nozzle=0;nozzle<assets.ships[ship].count;++nozzle){const auto& mount=assets.ships[ship].mounts[nozzle];
   const auto anchor=ship_effects_math::transform_point(model,mount.position);
   require(close(anchor,independent_euler(mount.position,origin,yaw,pitch,roll)),"all three ship classes retain exact nozzle position under independent RzRyRx rotations");
   require(close(length(anchor-origin),length(mount.position)),"body roll cannot detach a nozzle from its hull");
   const auto axis=ship_effects_math::transform_direction(model,mount.direction);
   require(close(axis,independent_euler(mount.direction,{},yaw,pitch,roll)),"exhaust direction rotates with the hull instead of the camera");
   for(float time:{0.f,.125f,3.f,40.f}){const auto idle=ship_effects_math::flame_scale(0,time),turbo=ship_effects_math::flame_scale(1,time);require(turbo.radius>idle.radius&&turbo.length>idle.length*2.5f&&turbo.intensity>idle.intensity,"turbo visibly increases plume dimensions/intensity at a fixed origin and visual instant");require(close(anchor,ship_effects_math::transform_point(model,mount.position)),"boost and time never translate the exhaust root");}
  }
 }
 {
  ShipEffects missing;const auto before=recording.loads;missing.initialize(nullptr,{});missing.draw(identity,identity,{},531,1,0);require(recording.loads==before&&!missing.initialized()&&live()==0,"legacy packs neither resolve GL nor create GPU resources");
 }
 {
  ShipEffects effects;effects.initialize(loader,assets);require(effects.initialized(),"new native-effect resources initialize");
  const auto uploads=recording.uploads,textures=recording.texture_uploads,generations=recording.generations,lookups=recording.loads;
  watch_allocations=true;
  for(int ship=531;ship<=533;++ship)for(float roll:{-2.f,0.f,1.5f}){const auto model=render_math::model({10,20,30},.7f,-.4f,roll,1);effects.draw(identity,model,{},ship,1,2);require(effects.last_stats().thrusters==unsigned(ship-531+2)&&recording.instances==unsigned(ship-531+2),"correct native ship selects its own static mounts");require(recording.model==model,"GPU receives the exact interpolated body matrix including roll");require(recording.last_flame_tint==assets.ships[unsigned(ship-531)].color,"native GS RGB128 modulation is passed un-clamped, including channels above1");}
  for(int frame=0;frame<1000;++frame)effects.draw(identity,identity,{},531,.65f,1.25f);
  const auto paused=recording.time_boost;effects.draw(identity,identity,{},531,.65f,1.25f);require(recording.time_boost==paused,"paused visual clock yields identical exhaust uniforms");
  watch_allocations=false;
  require(frame_allocations==0&&recording.uploads==uploads&&recording.texture_uploads==textures&&recording.generations==generations&&recording.loads==lookups,"drawing effects performs no component heap allocation, upload, lookup or resource generation");
  effects.draw(identity,identity,{},531,.5f,2,1,-1);require(effects.last_stats().draws==2&&effects.last_stats().thrusters==2&&effects.last_stats().sprites==1,"native hit feedback is optional alongside exhaust");require(recording.last_sprite_tint==std::array<float,4>{1,1,1,1},"hit sprite preserves its original colors rather than borrowing engine tint");
  effects.draw(identity,identity,{},531,1,2,1,.2f);require(effects.last_stats().thrusters==0&&effects.last_stats().sprites==1&&effects.last_stats().draws==1,"destruction immediately disables every flame and uses the native explosion only");
  require(recording.last_sprite_tint==std::array<float,4>{1,1,1,1},"explosion sprite also remains native-white modulated");
  effects.draw(identity,identity,{},531,1,2,1,1);require(effects.last_stats().draws==0,"completed destruction emits no residual flame or explosion");
  effects.draw(identity,identity,{},530,1,2);require(effects.last_stats().draws==0,"unsupported ship cannot borrow another class's nozzles");
  recording.device_fail=true;bool propagated=false;try{effects.draw(identity,identity,{},531,1,2);}catch(const std::runtime_error&){propagated=true;}recording.device_fail=false;require(propagated,"device errors reach the existing scene-owner fallback");
  effects.close();effects.close();require(!effects.initialized()&&live()==0,"repeated close releases every owned GPU handle exactly once");
 }
 for(int invalid=0;invalid<5;++invalid){auto bad=assets;if(invalid==0)bad.flame.columns=0;if(invalid==1)bad.flame.frames=5;if(invalid==2)bad.ships[1].mounts[0].direction={};if(invalid==3)bad.ships[2].count=17;if(invalid==4)bad.ships[0].color[0]=2.01f;ShipEffects effects;const auto lookups=recording.loads;bool rejected=false;try{effects.initialize(loader,bad);}catch(const std::runtime_error&){rejected=true;}require(rejected&&!effects.initialized()&&live()==0&&recording.loads==lookups,"invalid atlas, mounts or tint fail before any GL work");}
 recording.fail_generation=recording.generations+3;
 {ShipEffects effects;bool rejected=false;try{effects.initialize(loader,assets);}catch(const std::runtime_error&){rejected=true;}require(rejected&&live()==0,"partial allocation failure releases programs/textures/buffers");}
 recording.fail_generation=0;recording.compile_fail=true;
 {ShipEffects effects;bool rejected=false;try{effects.initialize(loader,assets);}catch(const std::runtime_error&){rejected=true;}require(rejected&&live()==0,"shader failure releases its shader object");}
 recording.compile_fail=false;
 {auto only_flame=assets;only_flame.hit={};only_flame.explosion={};ShipEffects effects;effects.initialize(loader,only_flame);effects.draw(identity,identity,{},531,1,2,1,.2f);require(effects.last_stats().draws==0,"missing optional native feedback never produces an invented substitute");}
 require(live()==0,"destructor releases the remaining live resources on its context thread");
 std::printf("PASS %u checks; frame_allocations=%zu; three-class transforms/roll, turbo-root stability, paused uniforms, depth/additive policy, static instancing, optional FX, failure/legacy cleanup\n",checks,frame_allocations);
}
