// SPDX-License-Identifier: GPL-3.0-or-later
// CPU recording facade: validates the production renderer's contract, not GLSL
// compilation or device pixels. Gameplay inputs drive the real combat pool.
#include "projectile_renderer.hpp"
#include "assets.hpp"
#include "space_combat.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wunused-parameter"
#endif
using namespace spacevoyages;
namespace {unsigned checks{};bool count_allocations{};unsigned allocations{};}
void* operator new(std::size_t size){if(count_allocations)++allocations;if(auto* p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"CHECK line%d: %s\n",__LINE__,#x);std::exit(2);}}while(false)
namespace {
template<class T>T empty(){return T{};}template<>void empty<void>(){}
#define DEFAULT(result,name,args) result fallback_##name args{return empty<result>();}
SPACE_GL_FUNCTIONS(DEFAULT)
#undef DEFAULT
struct Buffer {std::array<unsigned char,8192> data{};std::size_t size{};unsigned usage{};};
struct Attribute {unsigned buffer{},divisor{};int stride{};std::size_t offset{};};
struct Vao {std::array<Attribute,8> attributes{};unsigned elements{};};
struct Recorder {
    unsigned next=1,program{},vao{},array{},active=0x84c0,texture{},draws{},uploads{},tex_uploads{},generations{},fail_generation{},calls{};
    bool compile_fail{},link_fail{},draw_fail{},depth{},depth_write=true,additive{},blend_enabled{};
    std::array<unsigned,1024> live{};
    std::array<Buffer,1024> buffers{};
    std::array<Vao,1024> vaos{};
    std::array<std::array<float,4>,16> uniforms{};
    float time{};
    std::array<char,8192> shader_text{};
    std::array<char,8192> bolt_vertex_text{};
} rec;
unsigned create(unsigned kind){CHECK(rec.next<rec.live.size());rec.live[rec.next]=kind;return rec.next++;}
unsigned shader(unsigned){return create(4);}unsigned program(){return create(5);}
void gen(int n,unsigned* out,unsigned kind){for(int i=0;i<n;++i)out[i]=++rec.generations==rec.fail_generation?0:create(kind);}
void gen_buffer(int n,unsigned* out){gen(n,out,1);}void gen_vao(int n,unsigned* out){gen(n,out,2);}void gen_texture(int n,unsigned* out){gen(n,out,3);}
void release(int n,const unsigned* out,unsigned kind){for(int i=0;i<n;++i){CHECK(out[i]&&rec.live[out[i]]==kind);rec.live[out[i]]=0;}}
void del_buffer(int n,const unsigned* out){release(n,out,1);}void del_vao(int n,const unsigned* out){release(n,out,2);}void del_texture(int n,const unsigned* out){release(n,out,3);}
void del_shader(unsigned id){release(1,&id,4);}void del_program(unsigned id){release(1,&id,5);}
void shader_source(unsigned,int count,const char*const* sources,const int*){CHECK(count==1);std::snprintf(rec.shader_text.data(),rec.shader_text.size(),"%s",*sources);
    if(std::strstr(*sources,"velocity_birth")&&std::strstr(*sources,"scale_tip"))std::snprintf(rec.bolt_vertex_text.data(),rec.bolt_vertex_text.size(),"%s",*sources);}
void shader_status(unsigned,unsigned,int* status){*status=rec.compile_fail?0:1;}void program_status(unsigned,unsigned,int* status){*status=rec.link_fail?0:1;}
void bind_vao(unsigned id){rec.vao=id;}
void bind_buffer(unsigned target,unsigned id){if(target==0x8892)rec.array=id;else{CHECK(target==0x8893&&rec.vao);rec.vaos[rec.vao].elements=id;}}
void buffer_data(unsigned target,std::ptrdiff_t size,const void* bytes,unsigned usage){CHECK(target==0x8892&&rec.array&&size>0&&std::size_t(size)<=8192&&bytes);CHECK(usage==0x88e4||usage==0x88e8);
    auto& b=rec.buffers[rec.array];b.size=std::size_t(size);b.usage=usage;std::memcpy(b.data.data(),bytes,b.size);++rec.uploads;}
void attrib(unsigned index,int,unsigned,unsigned char,int stride,const void* offset){CHECK(rec.vao&&index<8);rec.vaos[rec.vao].attributes[index]={rec.array,0,stride,reinterpret_cast<std::size_t>(offset)};}
void divisor(unsigned index,unsigned value){CHECK(rec.vao&&index<8&&value==1);rec.vaos[rec.vao].attributes[index].divisor=value;}
void active(unsigned unit){CHECK(unit==0x84c0);rec.active=unit;}
void bind_texture(unsigned target,unsigned id){CHECK(target==0xde1);rec.texture=id;}
void texture(unsigned,int,int,int width,int height,int,unsigned format,unsigned type,const void* pixels){CHECK(width>0&&height>0&&format==0x1908&&type==0x1401&&pixels&&rec.texture);++rec.tex_uploads;}
void use(unsigned id){rec.program=id;}
int location(unsigned,const char* name){const char* names[]={"image","view_projection","presentation_time","scale_tip","camera_right","camera_up","atlas","atlas_texel","effect_kind","texture_wrap"};
    for(int i=0;i<10;++i)if(std::strcmp(name,names[i])==0)return i;CHECK(false);return 0;}
void uniform1f(int where,float v){CHECK(where==2&&std::isfinite(v));rec.time=v;}
void uniform4(int where,int count,const float* value){CHECK(where>=0&&where<int(rec.uniforms.size())&&count==1);for(int i=0;i<4;++i){CHECK(std::isfinite(value[i]));rec.uniforms[std::size_t(where)][std::size_t(i)]=value[i];}}
void enable(unsigned which){if(which==0xb71)rec.depth=true;if(which==0xbe2)rec.blend_enabled=true;}
void disable(unsigned which){if(which==0xbe2)rec.blend_enabled=false;}
void depth_mask(unsigned char write){rec.depth_write=write!=0;}
void blend(unsigned a,unsigned b,unsigned c,unsigned d){CHECK(a==0x302&&b==1&&c==1&&d==1);rec.additive=true;}
void draw(unsigned mode,int count,unsigned type,const void*,int instances){
    if(rec.draw_fail)throw std::runtime_error("Device failure");
    CHECK(mode==4&&(count==3||count==6)&&type==0x1405&&instances>0&&instances<=128&&rec.vao);
    const auto& vao=rec.vaos[rec.vao];CHECK(vao.elements&&rec.texture);
    const bool fragment=vao.attributes[7].divisor==1;
    if(fragment){CHECK(rec.depth&&rec.depth_write&&!rec.blend_enabled&&rec.active==0x84c0);
        for(unsigned i=5;i<8;++i){const auto& a=vao.attributes[i];CHECK(a.divisor==1&&a.stride==48&&a.offset==std::size_t(i-5)*16);CHECK(rec.buffers[a.buffer].size>=std::size_t(instances)*48);}
        ++rec.draws;return;}
    CHECK(rec.depth&&!rec.depth_write&&rec.additive&&rec.blend_enabled&&rec.active==0x84c0);
    const unsigned first=count==3?5:2;
    for(unsigned i=first;i<first+2;++i){const auto& a=vao.attributes[i];CHECK(a.divisor==1&&a.stride==32&&a.offset==std::size_t(i-first)*16);CHECK(rec.buffers[a.buffer].size>=std::size_t(instances)*32);}
    ++rec.draws;
}
void* loader(const char* name){++rec.calls;
#define ENTRY(symbol,fn) if(std::strcmp(name,"gl" #symbol)==0)return reinterpret_cast<void*>(&fn)
    ENTRY(CreateShader,shader);ENTRY(CreateProgram,program);ENTRY(GenBuffers,gen_buffer);ENTRY(GenVertexArrays,gen_vao);ENTRY(GenTextures,gen_texture);
    ENTRY(DeleteBuffers,del_buffer);ENTRY(DeleteVertexArrays,del_vao);ENTRY(DeleteTextures,del_texture);ENTRY(DeleteShader,del_shader);ENTRY(DeleteProgram,del_program);
    ENTRY(ShaderSource,shader_source);ENTRY(GetShaderiv,shader_status);ENTRY(GetProgramiv,program_status);ENTRY(BindVertexArray,bind_vao);ENTRY(BindBuffer,bind_buffer);
    ENTRY(BufferData,buffer_data);ENTRY(VertexAttribPointer,attrib);ENTRY(VertexAttribDivisor,divisor);ENTRY(ActiveTexture,active);ENTRY(BindTexture,bind_texture);ENTRY(TexImage2D,texture);
    ENTRY(UseProgram,use);ENTRY(GetUniformLocation,location);ENTRY(Uniform1f,uniform1f);ENTRY(Uniform4fv,uniform4);ENTRY(Enable,enable);ENTRY(Disable,disable);ENTRY(DepthMask,depth_mask);ENTRY(BlendFuncSeparate,blend);ENTRY(DrawElementsInstanced,draw);
#undef ENTRY
#define FALLBACK(result,symbol,args) if(std::strcmp(name,"gl" #symbol)==0)return reinterpret_cast<void*>(&fallback_##symbol);
    SPACE_GL_FUNCTIONS(FALLBACK)
#undef FALLBACK
    return nullptr;
}
unsigned live(){unsigned result{};for(auto id:rec.live)if(id)++result;return result;}
MeshAsset mesh(){MeshAsset m;m.id="projectile_laser";m.width=m.height=1;m.rgba={255,224,32,255};
    m.vertices={{{-9.53f,-.18f,0},{0,0,1},{0,0}},{{1.04f,0,0},{0,0,1},{1,.5f}},{{-9.53f,.18f,0},{0,0,1},{0,1}}};m.indices={0,1,2};return m;}
ProjectileEffectsAssets effects(bool sprites=true){ProjectileEffectsAssets a;a.bolt_scales={1,.25738127f,.11293804f};a.bolt_growth={1,3,6};
    if(sprites){a.impact.width=a.impact.height=1;a.impact.rgba={255,255,255,255};a.explosion=a.impact;}return a;}
SpaceCombatAssets guns(){SpaceCombatAssets a;for(std::size_t i=1;i<3;++i){a.ships[i].armed=true;a.ships[i].count=1;a.ships[i].muzzles[0]={{2,0,0},{1,0,0}};}return a;}
constexpr ProjectileRenderer::Matrix identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
float scalar(const Buffer& b,std::size_t offset){float value{};CHECK(offset+4<=b.size);std::memcpy(&value,b.data.data()+offset,4);return value;}
unsigned pool_buffer(){for(unsigned id=1;id<rec.next;++id)if(rec.live[id]==2){const auto& a=rec.vaos[id].attributes[5];if(a.divisor==1)return a.buffer;}CHECK(false);return 0;}
MeshAsset fragment_mesh(){auto m=mesh();m.id="asteroid_1";m.vertices[0].position[0]=-.5f;m.vertices[1].position[0]=.5f;m.vertices[2].position[0]=0;return m;}
unsigned fragment_buffer(){for(unsigned id=1;id<rec.next;++id)if(rec.live[id]==2&&rec.vaos[id].attributes[7].divisor==1)return rec.vaos[id].attributes[7].buffer;CHECK(false);return 0;}
void test_pool(){
    SpaceCombat combat;CHECK(combat.set_assets(guns()));combat.set_world({},nullptr,0);combat.reset(532);
    FlightState ship{};ship.speed=0;combat.step(ship,true,10.);
    ProjectileRenderer renderer;const auto original=mesh();renderer.initialize(loader,original,effects());CHECK(renderer.initialized());
    CHECK(rec.draws==3&&rec.tex_uploads==3);const auto generation=rec.generations,texture_uploads=rec.tex_uploads,proc_calls=rec.calls;
    renderer.draw(identity,{},combat,532,10.);CHECK(renderer.last_stats().projectiles==1&&renderer.last_stats().instance_uploads==1);
    const auto& b=rec.buffers[pool_buffer()];CHECK(b.size==32&&scalar(b,0)==2&&scalar(b,4)==0&&scalar(b,8)==0);
    CHECK(std::abs(scalar(b,12)-.5f)<1e-6&&scalar(b,16)==400&&scalar(b,20)==0&&scalar(b,24)==0&&scalar(b,28)==0);
    CHECK(std::abs(rec.uniforms[3][0]-.25738127f)<1e-7&&std::abs(rec.uniforms[3][1]-1.04f)<1e-6);
    CHECK(rec.uniforms[3][2]==3&&rec.uniforms[3][3]==6);
    CHECK(std::strstr(rec.bolt_vertex_text.data(),"length(velocity_birth.xyz)*max(age,0.0)"));
    CHECK(std::strstr(rec.bolt_vertex_text.data(),"smoothstep(0.0,max(scale_tip.w,.000001),distance)"));
    CHECK(std::strstr(rec.bolt_vertex_text.data(),"vec3 base=(position-vec3(scale_tip.y,0,0))*scale_tip.x"));
    CHECK(std::strstr(rec.bolt_vertex_text.data(),"p.x=max(base.x*expansion,-distance)"));
    CHECK(std::strstr(rec.bolt_vertex_text.data(),"radial_distance=max(0.0,distance+p.x)"));
    CHECK(rec.uniforms[9][3]==24.f/128.f); // Native runtime alpha, not GLB OPAQUE.
    const auto uploads=rec.uploads;count_allocations=true;
    combat.step(ship,false,10.+1./60.);const auto& p=combat.projectiles()[0];
    CHECK(std::abs(p.position.x-(p.birth_position.x+p.velocity.x*float(1./60.)))<1e-5);
    for(int i=0;i<1000;++i){renderer.draw(identity,{},combat,532,10.+double(i%3)/180.);CHECK(renderer.last_stats().instance_uploads==0);}
    count_allocations=false;CHECK(!allocations&&rec.uploads==uploads&&rec.generations==generation&&rec.tex_uploads==texture_uploads&&rec.calls==proc_calls);
    renderer.draw(identity,{},combat,533,10.01);CHECK(std::abs(rec.uniforms[3][0]-.11293804f)<1e-7&&rec.uniforms[3][2]==6&&rec.uniforms[3][3]==6);
    const auto paused_scale=rec.uniforms[3],paused_tint=rec.uniforms[9];const auto paused_time=rec.time;renderer.draw(identity,{},combat,533,10.01);
    CHECK(rec.uniforms[3]==paused_scale&&rec.uniforms[9]==paused_tint&&rec.time==paused_time&&rec.uploads==uploads);
    renderer.draw(identity,{},combat,531,10.01);CHECK(renderer.last_stats().projectiles==0&&renderer.last_stats().draws==0);
    renderer.draw(identity,{},combat,ShipIdentity::Original,10.01);CHECK(renderer.last_stats().projectiles==0&&renderer.last_stats().draws==0);
    const auto unknown_calls=rec.calls;renderer.draw(identity,{},combat,534,10.01);CHECK(rec.calls==unknown_calls&&renderer.last_stats().draws==0);
    const auto before_invalid=rec.draws;auto invalid=identity;invalid[0]=NAN;renderer.draw(invalid,{},combat,532,10.);CHECK(rec.draws==before_invalid);
    renderer.draw(identity,{},combat,532,NAN);CHECK(rec.draws==before_invalid);
    combat.clear_projectiles();renderer.draw(identity,{},combat,532,10.02);CHECK(renderer.last_stats().projectiles==0&&renderer.last_stats().draws==0);
    combat.step(ship,true,10.1);renderer.draw(identity,{},combat,532,10.1); // Cooldown may still be armed; drive to a real shot.
    for(int i=0;i<5;++i)combat.step(ship,true,10.2+double(i)/60.);
    renderer.draw(identity,{},combat,532,10.3);CHECK(renderer.last_stats().projectiles>0);
    rec.draw_fail=true;bool failed=false;try{renderer.draw(identity,{},combat,532,10.3);}catch(const std::runtime_error&){failed=true;}rec.draw_fail=false;CHECK(failed);
    renderer.close();renderer.close();CHECK(!renderer.initialized()&&live()==0);
}
void test_persistent_events(){
    SpaceCombat combat;CHECK(combat.set_assets(guns()));Asteroid rock;rock.position={20,0,0};rock.radius=3;combat.set_world({rock},nullptr,0);combat.reset(532);
    FlightState ship{};ship.speed=0;for(int i=0;i<17;++i)combat.step(ship,true,double(i)/60.);
    CHECK(combat.state().destroyed==1&&combat.alive_mask()[0]==0&&combat.event_count()>=3);
    bool destroyed=false;for(std::size_t i=0;i<combat.event_count();++i)if(combat.events()[i].kind==SpaceCombatEventKind::Destroyed){destroyed=true;CHECK(combat.events()[i].radius==3);}
    CHECK(destroyed); // Several native ticks were consumed without any render.
    ProjectileRenderer renderer;renderer.initialize(loader,mesh(),effects());CameraState camera{};camera.yaw=.3f;camera.pitch=-.2f;camera.roll=.6f;
    renderer.draw(identity,camera,combat,532,16./60.);CHECK(renderer.last_stats().effects>=1&&renderer.last_stats().instance_uploads>=1);
    CHECK(std::abs(rec.uniforms[4][0])+std::abs(rec.uniforms[4][1])+std::abs(rec.uniforms[4][2])>1);
    const auto uploads=rec.uploads;renderer.draw(identity,camera,combat,532,16./60.);CHECK(renderer.last_stats().instance_uploads==0&&rec.uploads==uploads);
    renderer.draw(identity,camera,combat,532,2.);CHECK(renderer.last_stats().effects==0&&rec.uploads==uploads);
    combat.reset(533);renderer.draw(identity,camera,combat,533,2.);CHECK(renderer.last_stats().effects==0&&renderer.last_stats().projectiles==0);
    renderer.close();CHECK(live()==0);
}
void test_continuous_hit(){
    SpaceCombat combat;CHECK(combat.set_assets(guns()));Asteroid rock;rock.position={20,0,0};rock.radius=3;
    combat.set_world({rock},nullptr,0);combat.reset(532);FlightState ship{};ship.speed=0;
    combat.step(ship,true,0);for(int i=1;i<=3;++i)combat.step(ship,false,double(i)/60.);
    const auto& p=combat.projectiles()[0];CHECK(!p.active&&p.id&&std::abs(p.lifetime-.0375f)<1e-5);
    CHECK(combat.event_count()==1&&std::abs(combat.events()[0].scene_seconds-.0375)<1e-5);
    ProjectileRenderer renderer;renderer.initialize(loader,mesh(),effects(false));
    renderer.draw(identity,{},combat,532,.03);CHECK(renderer.last_stats().projectiles==1); // Retired CCD slot still belongs to this interpolated frame.
    const auto& b=rec.buffers[pool_buffer()];CHECK(std::abs(scalar(b,12)-.0375f)<1e-5&&std::abs(scalar(b,28)+.03f)<1e-5);
    const auto uploads=rec.uploads;renderer.draw(identity,{},combat,532,.0374);CHECK(renderer.last_stats().projectiles==1&&rec.uploads==uploads);
    renderer.draw(identity,{},combat,532,.0376);CHECK(renderer.last_stats().projectiles==0&&rec.uploads==uploads);
    combat.clear_projectiles();renderer.draw(identity,{},combat,532,.03);CHECK(renderer.last_stats().projectiles==0); // Destruction/transition never revives a retired tail.
    renderer.close();CHECK(live()==0);
}
void test_native_fragments(){
    SpaceCombat combat;CHECK(combat.set_assets(guns()));Asteroid rock;rock.position={20,0,0};rock.radius=3;
    combat.set_world({rock},nullptr,0);combat.reset(532);FlightState ship{};ship.speed=0;
    for(int i=0;i<17;++i)combat.step(ship,true,double(i)/60.);
    const SpaceCombatEvent* destroyed=nullptr;for(std::size_t i=0;i<combat.event_count();++i)if(combat.events()[i].kind==SpaceCombatEventKind::Destroyed)destroyed=&combat.events()[i];
    CHECK(destroyed);const double hit=destroyed->scene_seconds;
    ProjectileRenderer renderer;const auto fragment=fragment_mesh();renderer.initialize(loader,mesh(),effects(false),&fragment);
    renderer.draw(identity,{},combat,532,hit-.0001);CHECK(renderer.last_stats().fragments==0);
    renderer.draw(identity,{},combat,532,hit+.0001);CHECK(renderer.last_stats().fragments==2&&renderer.last_stats().effects==0);
    const auto& b=rec.buffers[fragment_buffer()];CHECK(b.size==96&&b.usage==0x88e8);
    const Vec3 first{scalar(b,0),scalar(b,4),scalar(b,8)},second{scalar(b,48),scalar(b,52),scalar(b,56)};
    const auto center=destroyed->position-unit(destroyed->normal)*destroyed->radius;
    CHECK(length((first+second)*.5f-center)<1e-4); // Hit surface converted back to the parent center.
    for(std::size_t offset:{std::size_t(0),std::size_t(48)}){
        const Vec3 velocity{scalar(b,offset+16),scalar(b,offset+20),scalar(b,offset+24)},axis{scalar(b,offset+32),scalar(b,offset+36),scalar(b,offset+40)};
        CHECK(length(velocity)>=5&&length(velocity)<=10&&std::abs(length(axis)-1)<1e-5);
        CHECK(scalar(b,offset+28)>=rock.radius*.26f&&scalar(b,offset+28)<=rock.radius*.34f);
        CHECK(scalar(b,offset+44)>=.5235f&&scalar(b,offset+44)<=3.1416f);
    }
    const auto uploads=rec.uploads,resources=rec.generations,lookups=rec.calls;
    count_allocations=true;for(int i=0;i<1000;++i){renderer.draw(identity,{},combat,532,hit+.2);CHECK(renderer.last_stats().fragments==2&&renderer.last_stats().instance_uploads==0);}count_allocations=false;
    CHECK(!allocations&&rec.uploads==uploads&&rec.generations==resources&&rec.calls==lookups);
    renderer.draw(identity,{},combat,532,hit+.499);CHECK(renderer.last_stats().fragments==2);renderer.draw(identity,{},combat,532,hit+.501);CHECK(renderer.last_stats().fragments==0);
    combat.reset(533);renderer.draw(identity,{},combat,533,hit+.1);CHECK(renderer.last_stats().fragments==0);
    renderer.close();renderer.close();CHECK(live()==0);
    for(unsigned failure=12;failure<=16;++failure){rec.fail_generation=rec.generations+failure;bool failed=false;
        {ProjectileRenderer r;try{r.initialize(loader,mesh(),effects(),&fragment);}catch(const std::runtime_error&){failed=true;}CHECK(failed&&!r.initialized());}
        CHECK(live()==0);rec.fail_generation=0;}
    for(int failure=0;failure<3;++failure){auto bad=fragment;if(failure==0)bad.vertices[0].position[0]=2;if(failure==1)bad.alpha=AlphaMode::Blend;if(failure==2)bad.indices[0]=100;
        bool failed=false;{ProjectileRenderer r;try{r.initialize(loader,mesh(),effects(),&bad);}catch(const std::runtime_error&){failed=true;}CHECK(failed);}CHECK(live()==0);}
}
void test_visual_growth(){
    using projectile_effects_math::bolt_scale;
    using projectile_effects_math::bolt_offset;
    const ProjectileEffectsAssets legacy{};CHECK((legacy.bolt_growth==std::array<float,3>({1,1,1})&&legacy.bolt_growth_distance==6));
    for(const auto pair:{std::array<float,2>{.25738127f,3},std::array<float,2>{.11293804f,6}}){
        const auto bore=pair[0],growth=pair[1];
        CHECK(bolt_scale(bore,growth,6,0)==bore&&bolt_scale(bore,growth,6,-1)==bore);
        CHECK(std::abs(bolt_scale(bore,growth,6,3)-bore*(1+growth)*.5f)<1e-7);
        CHECK(std::abs(bolt_scale(bore,growth,6,6)-bore*growth)<1e-7&&bolt_scale(bore,growth,6,100)==bolt_scale(bore,growth,6,6));
        float previous=bore;
        for(int i=0;i<=6000;++i){const auto scale=bolt_scale(bore,growth,6,float(i)/1000);CHECK(scale>=previous-1e-7f&&scale>=bore&&scale<=bore*growth+1e-7f);previous=scale;}
        CHECK(bolt_scale(bore,1,6,3)==bore&&bolt_scale(bore,1,6,200)==bore); // Metadata absent keeps legacy size.
        CHECK(bolt_scale(bore,growth,6,NAN)==bore&&bolt_scale(bore,growth,0,3)==bore);
        for(int i=0;i<=2000;++i){const float travelled=float(i)/100;const auto head=bolt_offset({1.04f,0,0},1.04f,bore,growth,6,travelled);
            CHECK(length_squared(head)==0); // The emitted tip always remains on the physical projectile path.
            for(float x:{-9.53f,-7.f,-4.f,-1.f,1.04f})for(float yz:{-.188878f,0.f,.188878f}){
                const auto offset=bolt_offset({x,yz,yz},1.04f,bore,growth,6,travelled);
                CHECK(offset.x>=-travelled-1e-6f&&offset.x<=1e-6f);
                const float radial=std::sqrt(offset.y*offset.y+offset.z*offset.z),base=std::abs(yz)*bore*std::sqrt(2.f);
                CHECK(radial>=base-1e-6f&&radial<=base*growth+1e-6f);
                if(std::abs(offset.x+travelled)<1e-5f)CHECK(std::abs(radial-base)<1e-6f); // No enlarged plasma at the muzzle/birth plane.
                if(travelled+offset.x>=6)CHECK(std::abs(radial-base*growth)<1e-6f);
            }
        }
        const auto birth=bolt_offset({-9.53f,.188878f,0},1.04f,bore,growth,6,0);
        CHECK(birth.x==0&&std::abs(birth.y-.188878f*bore)<1e-7f);
        const auto escaped=bolt_offset({-9.53f,.188878f,0},1.04f,bore,growth,6,20);
        CHECK(std::abs(escaped.y-.188878f*bore*growth)<1e-7f); // Fully clear of the mouth: art growth is preserved.
    }
    SpaceCombat combat;CHECK(combat.set_assets(guns()));combat.set_world({},nullptr,0);combat.reset(532);FlightState ship{};ship.speed=0;combat.step(ship,true,0);
    {ProjectileRenderer renderer;auto old=effects(false);old.bolt_growth=legacy.bolt_growth;renderer.initialize(loader,mesh(),old);renderer.draw(identity,{},combat,532,0);
        CHECK(rec.uniforms[3][2]==1&&rec.uniforms[3][3]==6&&rec.uniforms[9][3]==24.f/128.f);}
    CHECK(live()==0);
    for(int invalid=0;invalid<6;++invalid){auto a=effects(false);if(invalid==0)a.bolt_growth[1]=.9f;if(invalid==1)a.bolt_growth[2]=17;
        if(invalid==2)a.bolt_growth[1]=NAN;if(invalid==3)a.bolt_growth_distance=0;if(invalid==4)a.bolt_growth_distance=1025;if(invalid==5)a.bolt_growth_distance=NAN;
        const auto lookups=rec.calls;bool rejected=false;{ProjectileRenderer r;try{r.initialize(loader,mesh(),a);}catch(const std::runtime_error&){rejected=true;}CHECK(rejected&&!r.initialized());}
        CHECK(rec.calls==lookups&&live()==0);
    }
}
void test_legacy_and_failures(){
    const auto calls=rec.calls;{ProjectileRenderer r;r.initialize(nullptr,{},effects());CHECK(!r.initialized()&&rec.calls==calls);}
    {ProjectileRenderer r;r.initialize(loader,mesh(),effects(false));CHECK(r.initialized());}CHECK(live()==0);
    for(unsigned failure=1;failure<=11;++failure){rec.fail_generation=rec.generations+failure;bool failed=false;
        {ProjectileRenderer r;try{r.initialize(loader,mesh(),effects());}catch(const std::runtime_error&){failed=true;}CHECK(failed&&!r.initialized());}
        CHECK(live()==0);rec.fail_generation=0;}
    for(int failure=0;failure<2;++failure){rec.compile_fail=failure==0;rec.link_fail=failure==1;bool failed=false;
        {ProjectileRenderer r;try{r.initialize(loader,mesh(),effects());}catch(const std::runtime_error&){failed=true;}CHECK(failed);}
        CHECK(live()==0);rec.compile_fail=rec.link_fail=false;}
    for(int failure=0;failure<4;++failure){auto m=mesh();auto e=effects();if(failure==0)m.indices[0]=99;if(failure==1)m.vertices[0].position[0]=NAN;
        if(failure==2)e.bolt_scales[1]=0;if(failure==3)e.impact.frames=2;bool failed=false;
        {ProjectileRenderer r;try{r.initialize(loader,m,e);}catch(const std::runtime_error&){failed=true;}CHECK(failed);}CHECK(live()==0);}
}
}
int main(){try{test_pool();test_persistent_events();test_continuous_hit();test_native_fragments();test_visual_growth();test_legacy_and_failures();CHECK(!allocations);
    std::printf("PASS %u checks: original laser/native alpha, data-driven3D growth/birth fit/legacy,532/533 scale and531 guard, CCD pool/revision uploads,2native1412 debris/event, opaque/additive order, lifetime/spin/center, pause zeroheap, persistent reset,16partial-init faults. FakeGL only; no device/GLSL proof.\n",checks);
}catch(const std::exception& e){std::fprintf(stderr,"Unexpected: %s\n",e.what());return 3;}}
