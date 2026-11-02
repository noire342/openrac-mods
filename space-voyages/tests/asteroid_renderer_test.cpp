// SPDX-License-Identifier: GPL-3.0-or-later
// Local CPU harness: exercises the production component through a recording GL facade.
#include "asteroid_renderer.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>
#include <vector>
#undef assert
#define assert(condition) do{if(!(condition)){std::fprintf(stderr,"CHECK failed at line %d: %s\n",__LINE__,#condition);std::exit(2);}}while(false)
using namespace spacevoyages;
static bool count_allocations=false;
static std::size_t frame_allocations=0;
void* operator new(std::size_t size){if(count_allocations)++frame_allocations;if(auto p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
namespace {
template<class T>T empty(){return T{};}
template<>void empty<void>(){}
#define DEFAULT(result,name,args) result fallback_##name args{return empty<result>();}
SPACE_GL_FUNCTIONS(DEFAULT)
#undef DEFAULT
struct Buffer {std::vector<unsigned char> data;};
struct Attribute {unsigned buffer{},divisor{};int stride{};std::size_t offset{};};
struct Vao {std::array<Attribute,8> attributes{};unsigned elements{};};
struct Recording {
    unsigned next=1,bound_buffer{},bound_vao{},draws{},uploads{},tex_uploads{},sub_uploads{},generation_calls{},fail_generation{};
    unsigned active_unit{},bound_texture{};
    std::array<float,1024> mask{};
    bool float_mask{};
    std::array<bool,176> rock_ids{};
    bool compile_fail=false,draw_fail=false;
    std::array<unsigned,4096> live{};
    std::array<Buffer,4096> buffers;
    std::array<Vao,4096> vaos;
    float time{};unsigned shader_count{};
} recording;
unsigned create(unsigned kind){const auto id=recording.next++;assert(id<recording.live.size());recording.live[id]=kind;return id;}
unsigned shader(unsigned){++recording.shader_count;return create(4);}
unsigned program(){return create(5);}
void generated(int n,unsigned* out,unsigned kind){for(int i=0;i<n;++i){if(++recording.generation_calls==recording.fail_generation)out[i]=0;else out[i]=create(kind);}}
void gen_buffers(int n,unsigned* out){generated(n,out,1);}
void gen_vaos(int n,unsigned* out){generated(n,out,2);}
void gen_textures(int n,unsigned* out){generated(n,out,3);}
void destroy(int n,const unsigned* ids,unsigned kind){for(int i=0;i<n;++i){assert(ids[i]&&recording.live[ids[i]]==kind);recording.live[ids[i]]=0;}}
void delete_buffers(int n,const unsigned* ids){destroy(n,ids,1);}
void delete_vaos(int n,const unsigned* ids){destroy(n,ids,2);}
void delete_textures(int n,const unsigned* ids){destroy(n,ids,3);}
void delete_shader(unsigned id){destroy(1,&id,4);}
void delete_program(unsigned id){destroy(1,&id,5);}
void get_shader(unsigned,unsigned,int* result){*result=recording.compile_fail?0:1;}
void get_program(unsigned,unsigned,int* result){*result=1;}
void bind_vao(unsigned id){recording.bound_vao=id;}
void bind_buffer(unsigned target,unsigned id){if(target==0x8892)recording.bound_buffer=id;else{assert(target==0x8893&&recording.bound_vao);recording.vaos[recording.bound_vao].elements=id;}}
void buffer_data(unsigned target,std::ptrdiff_t size,const void* data,unsigned usage){assert(target==0x8892&&usage==0x88e4&&size>0);++recording.uploads;auto& bytes=recording.buffers[recording.bound_buffer].data;bytes.resize(std::size_t(size));std::memcpy(bytes.data(),data,std::size_t(size));}
void attribute(unsigned index,int,unsigned,unsigned char,int stride,const void* offset){assert(recording.bound_vao&&index<8);auto& a=recording.vaos[recording.bound_vao].attributes[index];a.buffer=recording.bound_buffer;a.stride=stride;a.offset=reinterpret_cast<std::size_t>(offset);}
void divisor(unsigned index,unsigned value){assert(index>=5&&index<=7&&value==1);recording.vaos[recording.bound_vao].attributes[index].divisor=value;}
void texture(unsigned,int,int internal,int width,int height,int,unsigned,unsigned type,const void*){
    ++recording.tex_uploads;if(internal==0x8814){assert(width==256&&height==1&&type==0x1406);recording.float_mask=true;}}
void active_texture(unsigned unit){assert(unit==0x84c0||unit==0x84c1);recording.active_unit=unit;}
void bind_texture(unsigned target,unsigned id){assert(target==0xde1);recording.bound_texture=id;}
void texture_sub(unsigned target,int level,int x,int y,int width,int height,unsigned format,unsigned type,const void* data){
    assert(target==0xde1&&level==0&&x==0&&y==0&&width==256&&height==1&&format==0x1908&&type==0x1406);
    assert(recording.active_unit==0x84c1&&recording.bound_texture&&data);++recording.sub_uploads;
    std::memcpy(recording.mask.data(),data,std::size_t(width*height*4)*sizeof(float));
}
int uniform_location(unsigned,const char* name){return std::strcmp(name,"presentation_time")==0?1:2;}
void uniform_float(int location,float value){if(location==1)recording.time=value;}
void draw(unsigned mode,int count,unsigned type,const void*,int instances){
    if(recording.draw_fail)throw std::runtime_error("device failure");
    assert(mode==4&&count==3&&type==0x1405&&instances>0&&recording.bound_vao);
    const auto& vao=recording.vaos[recording.bound_vao];assert(vao.elements);
    for(unsigned i=5;i<8;++i){const auto& a=vao.attributes[i];assert(a.divisor==1&&a.stride==48&&a.offset==(i-5)*16);assert(recording.buffers[a.buffer].data.size()>=std::size_t(instances)*48);}
    const auto& data=recording.buffers[vao.attributes[7].buffer].data;
    for(int i=0;i<instances;++i){float index{};std::memcpy(&index,data.data()+std::size_t(i)*48+44,4);
        assert(index>=0&&index<176&&index==std::floor(index));recording.rock_ids[std::size_t(index)]=true;}
    ++recording.draws;
}
void* loader(const char* name){
#define ENTRY(symbol,function) if(std::strcmp(name,"gl" #symbol)==0)return reinterpret_cast<void*>(&function)
    ENTRY(CreateShader,shader);ENTRY(CreateProgram,program);ENTRY(GenBuffers,gen_buffers);ENTRY(GenVertexArrays,gen_vaos);ENTRY(GenTextures,gen_textures);
    ENTRY(DeleteBuffers,delete_buffers);ENTRY(DeleteVertexArrays,delete_vaos);ENTRY(DeleteTextures,delete_textures);ENTRY(DeleteShader,delete_shader);ENTRY(DeleteProgram,delete_program);
    ENTRY(GetShaderiv,get_shader);ENTRY(GetProgramiv,get_program);ENTRY(BindVertexArray,bind_vao);ENTRY(BindBuffer,bind_buffer);ENTRY(BufferData,buffer_data);
    ENTRY(VertexAttribPointer,attribute);ENTRY(VertexAttribDivisor,divisor);ENTRY(TexImage2D,texture);ENTRY(GetUniformLocation,uniform_location);ENTRY(Uniform1f,uniform_float);ENTRY(DrawElementsInstanced,draw);
    ENTRY(TexSubImage2D,texture_sub);ENTRY(ActiveTexture,active_texture);ENTRY(BindTexture,bind_texture);
#undef ENTRY
#define FALLBACK(result,symbol,args) if(std::strcmp(name,"gl" #symbol)==0)return reinterpret_cast<void*>(&fallback_##symbol);
    SPACE_GL_FUNCTIONS(FALLBACK)
#undef FALLBACK
    return nullptr;
}
unsigned live(){unsigned count=0;for(auto kind:recording.live)if(kind)++count;return count;}
SceneAssets assets(){SceneAssets result;for(unsigned i=0;i<2;++i){MeshAsset mesh;mesh.id="asteroid_"+std::to_string(i);mesh.width=mesh.height=1;mesh.rgba={255,255,255,255};mesh.texture_key="shared-rock";mesh.vertices={{{-.5f,0,0},{0,0,1},{0,0}},{{.5f,0,0},{0,0,1},{1,0}},{{0,.5f,0},{0,0,1},{.5f,1}}};mesh.indices={0,1,2};result.meshes.push_back(mesh);}return result;}
AsteroidLayout small_layout(){AsteroidFieldDefinition field{1,123,{0,0,0},{.65f,.65f,.65f},{1,0,0},0,12};AsteroidLayoutPolicy policy;policy.minimum_radius=.04f;policy.maximum_radius=.05f;policy.separation=.01f;policy.variants=2;policy.variant_radius_count=0;return build_asteroid_layout(nullptr,0,&field,1,policy);}
constexpr AsteroidRenderer::Matrix identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
}
int main(){
    try{
    const auto scene=assets();const auto layout=small_layout();assert(layout.complete()&&layout.asteroids().size()==12);
    {
        AsteroidRenderer renderer;renderer.initialize(loader,scene,layout);
        assert(renderer.initialized()&&recording.draws==2&&recording.uploads==6&&recording.tex_uploads==2&&recording.float_mask);
        for(std::size_t i=0;i<12;++i)assert(recording.rock_ids[i]);
        const auto uploads=recording.uploads,generations=recording.generation_calls;
        count_allocations=true;
        renderer.draw(identity,2.5f);const auto visible=renderer.last_stats();
        auto outside=identity;outside[12]=-100;renderer.draw(outside,2.5f);const auto hidden=renderer.last_stats();
        auto intersecting=identity;intersecting[12]=-1.3f;renderer.draw(intersecting,2.5f);const auto intersection=renderer.last_stats();
        for(int i=0;i<1000;++i)renderer.draw(identity,float(i)/144);
        count_allocations=false;
        assert(frame_allocations==0&&recording.uploads==uploads&&recording.generation_calls==generations);
        assert(visible.instances==12&&visible.draws==2&&visible.triangles==12&&visible.culled_batches==0);
        assert(hidden.instances==0&&hidden.draws==0&&hidden.culled_batches==2);
        assert(intersection.draws>0&&intersection.instances>0); // intersecting sphere is retained
        std::array<std::uint8_t,12> alive{};alive.fill(1);alive[3]=0;
        auto sub_uploads=recording.sub_uploads;
        count_allocations=true;renderer.draw(identity,3,alive.data(),alive.size(),7);
        assert(recording.sub_uploads==sub_uploads+1&&recording.mask[12]==0&&recording.mask[8]==1);
        for(int i=0;i<100;++i)renderer.draw(identity,3,alive.data(),alive.size(),7);
        assert(recording.sub_uploads==sub_uploads+1);
        alive[3]=1;alive[9]=0;renderer.draw(identity,3,alive.data(),alive.size(),8);
        assert(recording.sub_uploads==sub_uploads+2&&recording.mask[12]==1&&recording.mask[36]==0);
        renderer.draw(identity,3);assert(recording.sub_uploads==sub_uploads+3&&recording.mask[36]==1);
        renderer.draw(identity,3);assert(recording.sub_uploads==sub_uploads+3);
        count_allocations=false;assert(frame_allocations==0&&recording.uploads==uploads);
        bool count_rejected=false;try{renderer.draw(identity,3,alive.data(),11,9);}catch(const std::invalid_argument&){count_rejected=true;}assert(count_rejected);
        assert(recording.sub_uploads==sub_uploads+3);
        std::array<double,12> death{};death.fill(-1);death[9]=.0375;
        renderer.draw(identity,.03f,alive.data(),alive.size(),9,death.data());
        assert(recording.mask[36]==0&&std::abs(recording.mask[37]-.0375f)<1e-8);
        const auto timed_uploads=recording.sub_uploads;
        const auto mask_visible=[](std::size_t id){return !(recording.mask[id*4]<.5f&&recording.time>=recording.mask[id*4+1]);};
        assert(mask_visible(9));renderer.draw(identity,.0374f,alive.data(),alive.size(),9,death.data());assert(mask_visible(9));
        renderer.draw(identity,.0376f,alive.data(),alive.size(),9,death.data());assert(!mask_visible(9));
        assert(recording.sub_uploads==timed_uploads); // Presentation crosses death without uploads.
        renderer.draw(identity,.03f,alive.data(),alive.size(),9);assert(!mask_visible(9)&&recording.sub_uploads==timed_uploads+1); // Legacy immediate mode.
        death[9]=NAN;bool death_rejected=false;try{renderer.draw(identity,.03f,alive.data(),alive.size(),10,death.data());}catch(const std::invalid_argument&){death_rejected=true;}assert(death_rejected);
        renderer.draw(identity,.03f); // Reset timed/legacy state for later cases.
        renderer.draw(identity,3);assert(recording.time==3);renderer.draw(identity,3);assert(recording.time==3); // paused visual clock
        recording.draw_fail=true;bool propagated=false;try{renderer.draw(identity,4);}catch(const std::runtime_error&){propagated=true;}recording.draw_fail=false;assert(propagated);
        renderer.close();renderer.close();assert(!renderer.initialized()&&live()==0);
    }
    recording.fail_generation=recording.generation_calls+6;
    {AsteroidRenderer renderer;bool rejected=false;try{renderer.initialize(loader,scene,layout);}catch(const std::runtime_error&){rejected=true;}assert(rejected&&!renderer.initialized()&&live()==0);}
    recording.fail_generation=0;recording.compile_fail=true;
    {AsteroidRenderer renderer;bool rejected=false;try{renderer.initialize(loader,scene,layout);}catch(const std::runtime_error&){rejected=true;}assert(rejected&&live()==0);}
    recording.compile_fail=false;
    for(int invalid=0;invalid<3;++invalid){auto bad=scene;if(invalid==0)bad.meshes[0].alpha=AlphaMode::Blend;if(invalid==1)bad.meshes[0].vertices[0].position[0]=1.01f;if(invalid==2)bad.meshes[0].id="asteroid_0_invalid";AsteroidRenderer renderer;bool rejected=false;try{renderer.initialize(loader,bad,layout);}catch(const std::runtime_error&){rejected=true;}assert(rejected&&live()==0);}
    {
        AsteroidLayoutPolicy policy;policy.variants=2;const auto& catalog=galaxy_catalog();const auto full=build_asteroid_layout(catalog.data(),catalog.size(),policy);assert(full.complete()&&full.asteroids().size()==176);
        AsteroidRenderer renderer;renderer.initialize(loader,scene,full);auto full_view=identity;full_view[0]=full_view[5]=full_view[10]=.0001f;
        count_allocations=true;renderer.draw(full_view,123.25f);count_allocations=false;
        unsigned batches{};for(const auto& field:full.fields())for(unsigned variant=0;variant<2;++variant){bool exists=false;
            for(std::size_t i=field.begin;i<field.begin+field.count;++i)exists=exists||full.asteroids()[i].variant==variant;if(exists)++batches;}
        assert(frame_allocations==0&&renderer.last_stats().instances==176&&renderer.last_stats().draws==batches&&renderer.last_stats().triangles==176);
    }
    assert(live()==0);
    std::puts("PASS: immutable176 IDs, revision-only RGBA32F alive/death mask, continuous.0375 hit gate, legacy/reset/count checks, culling, zero frame heap/geometry uploads, paused clock, partial-init/destructor cleanup; FakeGL only");
    }catch(const std::exception& e){std::fprintf(stderr,"Unexpected exception: %s\n",e.what());return 3;}
}
