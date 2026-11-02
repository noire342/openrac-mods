// SPDX-License-Identifier: GPL-3.0-or-later
#include "projectile_renderer.hpp"
#include "assets.hpp"
#include "render_math.hpp"
#include "space_combat.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace spacevoyages {
namespace {
constexpr unsigned array_buffer=0x8892,element_buffer=0x8893,static_draw=0x88e4,dynamic_draw=0x88e8;
struct BoltInstance {std::array<float,4> position_lifetime,velocity_birth;};
struct EventInstance {std::array<float,4> position_radius,birth_duration_kind;};
struct FragmentInstance {std::array<float,4> position_birth,velocity_radius,axis_speed;};
struct QuadVertex {float x,y,u,v;};
static_assert(sizeof(BoltInstance)==32&&sizeof(EventInstance)==32,"Bounded combat instance format");
static_assert(sizeof(FragmentInstance)==48,"Bounded visual debris instance format");
constexpr ProjectileRenderer::Matrix outside_clip{0,0,0,0,0,0,0,0,0,0,0,0,2,2,2,1};
bool finite_matrix(const ProjectileRenderer::Matrix& m){for(float v:m)if(!std::isfinite(v))return false;return true;}
// A stable event-based visual seed, independent of rendering frequency.
float fragment_seed(std::uint64_t id,unsigned ordinal){
    auto x=id+std::uint64_t(ordinal+1)*0x9e3779b97f4a7c15ull;
    x=(x^(x>>30))*0xbf58476d1ce4e5b9ull;x=(x^(x>>27))*0x94d049bb133111ebull;x^=x>>31;
    return float(x>>40)/16777215.f;
}
void validate_sprite(const EffectSprite& s){
    if(s.rgba.empty())return;
    if(s.width<1||s.height<1||s.width>4096||s.height>4096||s.rgba.size()!=std::size_t(s.width)*std::size_t(s.height)*4||
       !s.columns||!s.rows||s.columns>std::uint32_t(s.width)||s.rows>std::uint32_t(s.height)||
       s.width%int(s.columns)||s.height%int(s.rows)||!s.frames||s.frames>s.columns*s.rows||
       !std::isfinite(s.fps)||s.fps<0||s.fps>1000)
        throw std::runtime_error("Invalid original projectile-effect sprite");
}
constexpr const char* bolt_vertex=R"GLSL(#version 330 core
layout(location=0)in vec3 position;
layout(location=1)in vec3 normal;
layout(location=2)in vec2 uv;
layout(location=3)in vec4 vertex_color;
layout(location=4)in vec4 tile_rect;
layout(location=5)in vec4 birth_lifetime;
layout(location=6)in vec4 velocity_birth;
uniform mat4 view_projection;
uniform float presentation_time;
uniform vec4 scale_tip;
out vec2 t;
out vec4 vcolor;
out vec4 rect;
void main(){
    float age=presentation_time-velocity_birth.w;
    vec3 axis=normalize(velocity_birth.xyz);
    vec3 reference=abs(axis.z)<.9?vec3(0,0,1):vec3(0,1,0);
    vec3 left=normalize(cross(reference,axis)),up=cross(axis,left);
    float distance=length(velocity_birth.xyz)*max(age,0.0);
    float expansion=mix(1.0,scale_tip.z,smoothstep(0.0,max(scale_tip.w,.000001),distance));
    vec3 base=(position-vec3(scale_tip.y,0,0))*scale_tip.x;
    // The original mesh grows only into space already traversed by the tip.
    // At the birth plane its radius remains exactly fitted to the barrel.
    vec3 p=base;
    p.x=max(base.x*expansion,-distance);
    float radial_distance=max(0.0,distance+p.x);
    float radial_expansion=mix(1.0,scale_tip.z,smoothstep(0.0,max(scale_tip.w,.000001),radial_distance));
    p.yz=base.yz*radial_expansion;
    vec3 world=birth_lifetime.xyz+velocity_birth.xyz*max(age,0.0)+axis*p.x+left*p.y+up*p.z;
    gl_Position=view_projection*vec4(world,1.0);
    if(age<0.0||age>=birth_lifetime.w)gl_Position=vec4(2,2,2,1);
    t=uv;vcolor=vertex_color;rect=tile_rect;
})GLSL";
constexpr const char* bolt_fragment=R"GLSL(#version 330 core
in vec2 t;
in vec4 vcolor;
in vec4 rect;
uniform sampler2D image;
uniform vec4 texture_wrap;
out vec4 color;
float tile(float u,float mode){
    if(mode==10497.0)return fract(u);
    if(mode==33648.0)return 1.0-abs(mod(u,2.0)-1.0);
    return clamp(u,0.0,1.0);
}
void main(){
    vec2 st=texture_wrap.z>0.0?rect.zw+vec2(tile(t.x,texture_wrap.x),tile(t.y,texture_wrap.y))*rect.xy:t;
    color=texture(image,st)*vcolor;
    // Original object tint255 on the GS128 neutral scale, then native clamp.
    color.rgb=min(color.rgb*(255.0/128.0),vec3(1.0));
    color.a*=texture_wrap.w;
})GLSL";
constexpr const char* effect_vertex=R"GLSL(#version 330 core
layout(location=0)in vec2 position;
layout(location=1)in vec2 uv;
layout(location=2)in vec4 center_radius;
layout(location=3)in vec4 birth_duration_kind;
uniform mat4 view_projection;
uniform float presentation_time;
uniform vec4 camera_right;
uniform vec4 camera_up;
uniform int effect_kind;
out vec2 t;
out float progress;
void main(){
    float age=presentation_time-birth_duration_kind.x;
    progress=clamp(age/max(birth_duration_kind.y,.000001),0.0,1.0);
    vec3 p=center_radius.xyz+(camera_right.xyz*position.x+camera_up.xyz*position.y)*center_radius.w;
    gl_Position=view_projection*vec4(p,1.0);
    if(age<0.0||age>=birth_duration_kind.y||int(birth_duration_kind.z+.5)!=effect_kind)
        gl_Position=vec4(2,2,2,1);
    t=uv;
})GLSL";
constexpr const char* effect_fragment=R"GLSL(#version 330 core
in vec2 t;
in float progress;
uniform sampler2D image;
uniform vec4 atlas;
uniform vec4 atlas_texel;
out vec4 color;
void main(){
    float frame=min(floor(progress*atlas.z),atlas.z-1.0);
    vec2 cell=vec2(mod(frame,atlas.x),floor(frame/atlas.x));
    vec2 half_texel=atlas_texel.xy*atlas.xy*.5;
    vec2 st=(cell+clamp(t,half_texel,vec2(1.0)-half_texel))/atlas.xy;
    vec4 p=texture(image,st);
    color=vec4(p.rgb,p.a*(1.0-progress));
})GLSL";
constexpr const char* fragment_vertex=R"GLSL(#version 330 core
layout(location=0)in vec3 position;
layout(location=1)in vec3 normal;
layout(location=2)in vec2 uv;
layout(location=3)in vec4 vertex_color;
layout(location=4)in vec4 tile_rect;
layout(location=5)in vec4 center_birth;
layout(location=6)in vec4 velocity_radius;
layout(location=7)in vec4 axis_speed;
uniform mat4 view_projection;
uniform float presentation_time;
out vec2 t;
out vec4 vcolor;
out vec4 rect;
out vec3 n;
vec3 spin(vec3 p,vec3 axis,float c,float s){return p*c+cross(axis,p)*s+axis*dot(axis,p)*(1.0-c);}
void main(){
    float age=presentation_time-center_birth.w;
    float angle=axis_speed.w*max(age,0.0),c=cos(angle),s=sin(angle);
    float shrink=clamp((.5-age)/.1,0.0,1.0);
    vec3 p=center_birth.xyz+velocity_radius.xyz*max(age,0.0)
        +spin(position,axis_speed.xyz,c,s)*velocity_radius.w*shrink;
    gl_Position=view_projection*vec4(p,1.0);
    if(age<0.0||age>=.5)gl_Position=vec4(2,2,2,1);
    n=spin(normal,axis_speed.xyz,c,s);t=uv;vcolor=vertex_color;rect=tile_rect;
})GLSL";
constexpr const char* fragment_fragment=R"GLSL(#version 330 core
in vec2 t;
in vec4 vcolor;
in vec4 rect;
in vec3 n;
uniform sampler2D image;
uniform vec4 texture_wrap;
out vec4 color;
float tile(float u,float mode){
    if(mode==10497.0)return fract(u);
    if(mode==33648.0)return 1.0-abs(mod(u,2.0)-1.0);
    return clamp(u,0.0,1.0);
}
void main(){
    vec2 st=texture_wrap.z>0.0?rect.zw+vec2(tile(t.x,texture_wrap.x),tile(t.y,texture_wrap.y))*rect.xy:t;
    vec4 p=texture(image,st)*vcolor;
    if(texture_wrap.w>=0.0&&p.a<texture_wrap.w)discard;
    float light=max(dot(normalize(n),normalize(vec3(-.4,-.3,.85))),0.0);
    color=vec4(p.rgb*(.86+.14*light),1.0);
})GLSL";
}

namespace projectile_effects_math {
float bolt_scale(float muzzle_scale,float growth,float growth_distance,float travelled)noexcept{
    if(!std::isfinite(muzzle_scale)||muzzle_scale<=0)return 0;
    if(!std::isfinite(growth)||growth<1||!std::isfinite(growth_distance)||growth_distance<=0||!std::isfinite(travelled))return muzzle_scale;
    const float t=std::clamp(travelled/growth_distance,0.f,1.f),smooth=t*t*(3-2*t);
    return muzzle_scale*(1+(growth-1)*smooth);
}
Vec3 bolt_offset(Vec3 vertex,float tip_x,float muzzle_scale,float growth,float growth_distance,float travelled)noexcept{
    if(!finite(vertex)||!std::isfinite(tip_x)||!std::isfinite(muzzle_scale)||muzzle_scale<=0||!std::isfinite(travelled))return {};
    const float distance=std::max(0.f,travelled);
    const auto base=(vertex-Vec3{tip_x,0,0})*muzzle_scale;
    const float x=std::max(base.x*bolt_scale(muzzle_scale,growth,growth_distance,distance)/muzzle_scale,-distance);
    const float radial=bolt_scale(muzzle_scale,growth,growth_distance,std::max(0.f,distance+x))/muzzle_scale;
    return {x,base.y*radial,base.z*radial};
}
}

unsigned ProjectileRenderer::shader(unsigned kind,const char* source){
    const auto id=gl_.CreateShader(kind);if(!id)throw std::runtime_error("Could not create projectile shader");
    try{gl_.ShaderSource(id,1,&source,nullptr);gl_.CompileShader(id);int ok{};gl_.GetShaderiv(id,0x8b81,&ok);
        if(!ok){char error[2048]{};gl_.GetShaderInfoLog(id,sizeof error,nullptr,error);throw std::runtime_error(std::string("Projectile shader: ")+error);}
    }catch(...){gl_.DeleteShader(id);throw;}return id;
}
unsigned ProjectileRenderer::program(const char* vertex,const char* fragment){
    unsigned a{},b{},id{};
    try{a=shader(0x8b31,vertex);b=shader(0x8b30,fragment);id=gl_.CreateProgram();if(!id)throw std::runtime_error("Could not create projectile program");
        gl_.AttachShader(id,a);gl_.AttachShader(id,b);gl_.LinkProgram(id);int ok{};gl_.GetProgramiv(id,0x8b82,&ok);
        if(!ok){char error[2048]{};gl_.GetProgramInfoLog(id,sizeof error,nullptr,error);throw std::runtime_error(std::string("Projectile program: ")+error);}
        gl_.DeleteShader(a);a=0;gl_.DeleteShader(b);return id;
    }catch(...){if(a)gl_.DeleteShader(a);if(b)gl_.DeleteShader(b);if(id)gl_.DeleteProgram(id);throw;}
}
ProjectileRenderer::SpriteGpu ProjectileRenderer::texture(const EffectSprite& s){
    SpriteGpu gpu{};if(s.rgba.empty())return gpu;
    try{gl_.GenTextures(1,&gpu.texture);if(!gpu.texture)throw std::runtime_error("Could not allocate projectile-effect texture");
        gl_.BindTexture(0xde1,gpu.texture);gl_.TexParameteri(0xde1,0x2801,0x2601);gl_.TexParameteri(0xde1,0x2800,0x2601);
        gl_.TexParameteri(0xde1,0x2802,0x812f);gl_.TexParameteri(0xde1,0x2803,0x812f);
        gl_.TexImage2D(0xde1,0,0x8058,s.width,s.height,0,0x1908,0x1401,s.rgba.data());
    }catch(...){if(gpu.texture)gl_.DeleteTextures(1,&gpu.texture);throw;}
    gpu.width=s.width;gpu.height=s.height;gpu.columns=int(s.columns);gpu.rows=int(s.rows);gpu.frames=int(s.frames);gpu.fps=s.fps;return gpu;
}
void ProjectileRenderer::initialize(GlApi::GetProc loader,const MeshAsset& mesh,const ProjectileEffectsAssets& effects,const MeshAsset* fragment){
    if(initialized())return;
    if(mesh.vertices.empty()&&mesh.indices.empty())return; // Legacy pack: no GL resources or invented beam.
    validate_sprite(effects.impact);validate_sprite(effects.explosion);
    if(mesh.vertices.empty()||mesh.indices.empty()||mesh.indices.size()%3||mesh.indices.size()>std::size_t(std::numeric_limits<int>::max())||
       mesh.width<1||mesh.height<1||mesh.width>4096||mesh.height>4096||mesh.rgba.size()!=std::size_t(mesh.width)*std::size_t(mesh.height)*4)
        throw std::runtime_error("Invalid original projectile mesh/texture");
    tip_=-std::numeric_limits<float>::infinity();
    for(const auto& v:mesh.vertices){if(!finite({v.position[0],v.position[1],v.position[2]}))throw std::runtime_error("Invalid projectile vertex");tip_=std::max(tip_,v.position[0]);}
    for(auto index:mesh.indices)if(index>=mesh.vertices.size())throw std::runtime_error("Invalid projectile index");
    for(float scale:effects.bolt_scales)if(!std::isfinite(scale)||scale<=0||scale>100)throw std::runtime_error("Invalid projectile visual scale");
    for(float growth:effects.bolt_growth)if(!std::isfinite(growth)||growth<1||growth>16)throw std::runtime_error("Invalid projectile visual growth");
    if(!std::isfinite(effects.bolt_growth_distance)||effects.bolt_growth_distance<=0||effects.bolt_growth_distance>1024)
        throw std::runtime_error("Invalid projectile growth distance");
    if(fragment){
        if(fragment->vertices.empty()||fragment->indices.empty()||fragment->indices.size()%3||
           fragment->indices.size()>std::size_t(std::numeric_limits<int>::max())||fragment->alpha==AlphaMode::Blend||
           fragment->width<1||fragment->height<1||fragment->width>4096||fragment->height>4096||
           fragment->rgba.size()!=std::size_t(fragment->width)*std::size_t(fragment->height)*4)
            throw std::runtime_error("Invalid original asteroid-fragment mesh/texture");
        for(const auto& v:fragment->vertices){const Vec3 p{v.position[0],v.position[1],v.position[2]};
            if(!finite(p)||length_squared(p)>1.0002f)throw std::runtime_error("Asteroid-fragment asset exceeds unit sphere");}
        for(auto index:fragment->indices)if(index>=fragment->vertices.size())throw std::runtime_error("Invalid asteroid-fragment index");
    }
    scales_=effects.bolt_scales;growth_=effects.bolt_growth;growth_distance_=effects.bolt_growth_distance;gl_.load(loader);
    try{
        const auto make_buffer=[&](unsigned& id,const void* data,std::size_t bytes,unsigned usage){
            gl_.GenBuffers(1,&id);if(!id)throw std::runtime_error("Could not allocate projectile buffer");
            gl_.BindBuffer(array_buffer,id);gl_.BufferData(array_buffer,std::ptrdiff_t(bytes),data,usage);
        };
        const auto uniforms=[&](unsigned id){Uniforms u{};u.vp=gl_.GetUniformLocation(id,"view_projection");u.time=gl_.GetUniformLocation(id,"presentation_time");
            u.scale_tip=gl_.GetUniformLocation(id,"scale_tip");u.camera_right=gl_.GetUniformLocation(id,"camera_right");u.camera_up=gl_.GetUniformLocation(id,"camera_up");
            u.atlas=gl_.GetUniformLocation(id,"atlas");u.atlas_texel=gl_.GetUniformLocation(id,"atlas_texel");u.effect_kind=gl_.GetUniformLocation(id,"effect_kind");
            u.wrap=gl_.GetUniformLocation(id,"texture_wrap");gl_.UseProgram(id);gl_.Uniform1i(gl_.GetUniformLocation(id,"image"),0);return u;};
        bolt_program_=program(bolt_vertex,bolt_fragment);bolt_u_=uniforms(bolt_program_);
        make_buffer(bolt_vbo_,mesh.vertices.data(),mesh.vertices.size()*sizeof(Vertex),static_draw);
        make_buffer(bolt_ebo_,mesh.indices.data(),mesh.indices.size()*sizeof(unsigned),static_draw);
        std::array<BoltInstance,SpaceCombat::projectile_capacity> empty{};
        make_buffer(bolt_instances_,empty.data(),sizeof empty,dynamic_draw);
        gl_.GenVertexArrays(1,&bolt_vao_);if(!bolt_vao_)throw std::runtime_error("Could not allocate projectile VAO");
        gl_.BindVertexArray(bolt_vao_);gl_.BindBuffer(array_buffer,bolt_vbo_);gl_.BindBuffer(element_buffer,bolt_ebo_);
        for(unsigned a=0;a<5;++a){const int components=a==2?2:a>=3?4:3;const std::size_t offset=a>=3?32+(a-3)*16:a*12;
            gl_.EnableVertexAttribArray(a);gl_.VertexAttribPointer(a,components,0x1406,0,sizeof(Vertex),reinterpret_cast<const void*>(offset));}
        gl_.BindBuffer(array_buffer,bolt_instances_);
        for(unsigned a=5;a<7;++a){gl_.EnableVertexAttribArray(a);gl_.VertexAttribPointer(a,4,0x1406,0,sizeof(BoltInstance),reinterpret_cast<const void*>(std::size_t(a-5)*16));gl_.VertexAttribDivisor(a,1);}
        gl_.ActiveTexture(0x84c0);gl_.GenTextures(1,&bolt_texture_);if(!bolt_texture_)throw std::runtime_error("Could not allocate original laser texture");
        gl_.BindTexture(0xde1,bolt_texture_);gl_.TexParameteri(0xde1,0x2801,0x2601);gl_.TexParameteri(0xde1,0x2800,0x2601);
        gl_.TexParameteri(0xde1,0x2802,mesh.atlas?33071:int(mesh.wrap_s));gl_.TexParameteri(0xde1,0x2803,mesh.atlas?33071:int(mesh.wrap_t));
        gl_.TexImage2D(0xde1,0,0x8058,mesh.width,mesh.height,0,0x1908,0x1401,mesh.rgba.data());
        // Native Gemlik1009: blend flags0x200, alpha24 on the GS128 scale.
        bolt_indices_=int(mesh.indices.size());texture_wrap_={float(mesh.wrap_s),float(mesh.wrap_t),float(mesh.atlas),24.f/128.f};
        if(!effects.impact.rgba.empty()||!effects.explosion.rgba.empty()){
            effect_program_=program(effect_vertex,effect_fragment);effect_u_=uniforms(effect_program_);
            constexpr QuadVertex quad[]={{-1,-1,0,1},{1,-1,1,1},{1,1,1,0},{-1,1,0,0}};constexpr unsigned indices[]={0,1,2,0,2,3};
            make_buffer(effect_vbo_,quad,sizeof quad,static_draw);make_buffer(effect_ebo_,indices,sizeof indices,static_draw);
            std::array<EventInstance,SpaceCombat::event_capacity> event_empty{};make_buffer(effect_instances_,event_empty.data(),sizeof event_empty,dynamic_draw);
            gl_.GenVertexArrays(1,&effect_vao_);if(!effect_vao_)throw std::runtime_error("Could not allocate projectile-effect VAO");
            gl_.BindVertexArray(effect_vao_);gl_.BindBuffer(array_buffer,effect_vbo_);gl_.BindBuffer(element_buffer,effect_ebo_);
            for(unsigned a=0;a<2;++a){gl_.EnableVertexAttribArray(a);gl_.VertexAttribPointer(a,2,0x1406,0,sizeof(QuadVertex),reinterpret_cast<const void*>(std::size_t(a)*8));}
            gl_.BindBuffer(array_buffer,effect_instances_);
            for(unsigned a=2;a<4;++a){gl_.EnableVertexAttribArray(a);gl_.VertexAttribPointer(a,4,0x1406,0,sizeof(EventInstance),reinterpret_cast<const void*>(std::size_t(a-2)*16));gl_.VertexAttribDivisor(a,1);}
            impact_=texture(effects.impact);explosion_=texture(effects.explosion);
        }
        if(fragment){
            fragment_program_=program(fragment_vertex,fragment_fragment);fragment_u_=uniforms(fragment_program_);
            make_buffer(fragment_vbo_,fragment->vertices.data(),fragment->vertices.size()*sizeof(Vertex),static_draw);
            make_buffer(fragment_ebo_,fragment->indices.data(),fragment->indices.size()*sizeof(unsigned),static_draw);
            std::array<FragmentInstance,SpaceCombat::event_capacity*2> empty_fragments{};
            make_buffer(fragment_instances_,empty_fragments.data(),sizeof empty_fragments,dynamic_draw);
            gl_.GenVertexArrays(1,&fragment_vao_);if(!fragment_vao_)throw std::runtime_error("Could not allocate native-fragment VAO");
            gl_.BindVertexArray(fragment_vao_);gl_.BindBuffer(array_buffer,fragment_vbo_);gl_.BindBuffer(element_buffer,fragment_ebo_);
            for(unsigned a=0;a<5;++a){const int components=a==2?2:a>=3?4:3;const std::size_t offset=a>=3?32+(a-3)*16:a*12;
                gl_.EnableVertexAttribArray(a);gl_.VertexAttribPointer(a,components,0x1406,0,sizeof(Vertex),reinterpret_cast<const void*>(offset));}
            gl_.BindBuffer(array_buffer,fragment_instances_);
            for(unsigned a=5;a<8;++a){gl_.EnableVertexAttribArray(a);gl_.VertexAttribPointer(a,4,0x1406,0,sizeof(FragmentInstance),reinterpret_cast<const void*>(std::size_t(a-5)*16));gl_.VertexAttribDivisor(a,1);}
            gl_.GenTextures(1,&fragment_texture_);if(!fragment_texture_)throw std::runtime_error("Could not allocate native-fragment texture");
            gl_.BindTexture(0xde1,fragment_texture_);gl_.TexParameteri(0xde1,0x2801,0x2601);gl_.TexParameteri(0xde1,0x2800,0x2601);
            gl_.TexParameteri(0xde1,0x2802,fragment->atlas?33071:int(fragment->wrap_s));gl_.TexParameteri(0xde1,0x2803,fragment->atlas?33071:int(fragment->wrap_t));
            gl_.TexImage2D(0xde1,0,0x8058,fragment->width,fragment->height,0,0x1908,0x1401,fragment->rgba.data());
            fragment_indices_=int(fragment->indices.size());fragment_wrap_={float(fragment->wrap_s),float(fragment->wrap_t),float(fragment->atlas),fragment->alpha==AlphaMode::Mask?fragment->alpha_cutoff:-1.f};
        }
        // Warm both exact instanced pipelines while the owner's scene is black.
        gl_.Enable(0xb71);gl_.DepthMask(0);gl_.DepthFunc(0x201);gl_.Enable(0xbe2);gl_.BlendFuncSeparate(0x302,1,1,1);
        gl_.Disable(0xb44);gl_.Disable(0xb90);gl_.Disable(0xc11);gl_.UseProgram(bolt_program_);
        gl_.UniformMatrix4fv(bolt_u_.vp,1,0,outside_clip.data());gl_.Uniform1f(bolt_u_.time,1);const float scale_tip[]={1,tip_,growth_[1],growth_distance_};
        gl_.Uniform4fv(bolt_u_.scale_tip,1,scale_tip);gl_.Uniform4fv(bolt_u_.wrap,1,texture_wrap_.data());gl_.BindTexture(0xde1,bolt_texture_);gl_.BindVertexArray(bolt_vao_);
        gl_.DrawElementsInstanced(4,bolt_indices_,0x1405,nullptr,int(SpaceCombat::projectile_capacity));
        if(effect_program_){gl_.UseProgram(effect_program_);gl_.UniformMatrix4fv(effect_u_.vp,1,0,outside_clip.data());gl_.Uniform1f(effect_u_.time,1);
            const float axis[]={0,1,0,0};gl_.Uniform4fv(effect_u_.camera_right,1,axis);gl_.Uniform4fv(effect_u_.camera_up,1,axis);
            gl_.BindVertexArray(effect_vao_);for(const auto& s:{impact_,explosion_})if(s.texture){const float atlas[]={float(s.columns),float(s.rows),float(s.frames),s.fps};
                const float texel[]={1.f/s.width,1.f/s.height,0,0};gl_.Uniform4fv(effect_u_.atlas,1,atlas);gl_.Uniform4fv(effect_u_.atlas_texel,1,texel);
                gl_.BindTexture(0xde1,s.texture);gl_.DrawElementsInstanced(4,6,0x1405,nullptr,int(SpaceCombat::event_capacity));}}
        if(fragment_program_){gl_.DepthMask(1);gl_.Disable(0xbe2);gl_.UseProgram(fragment_program_);
            gl_.UniformMatrix4fv(fragment_u_.vp,1,0,outside_clip.data());gl_.Uniform1f(fragment_u_.time,1);gl_.Uniform4fv(fragment_u_.wrap,1,fragment_wrap_.data());
            gl_.BindTexture(0xde1,fragment_texture_);gl_.BindVertexArray(fragment_vao_);
            gl_.DrawElementsInstanced(4,fragment_indices_,0x1405,nullptr,int(SpaceCombat::event_capacity*2));}
        gl_.BindVertexArray(0);gl_.UseProgram(0);stats_={};
    }catch(...){close();throw;}
}
void ProjectileRenderer::draw(const Matrix& vp,const CameraState& camera,const SpaceCombat& combat,int ship_class,double seconds){
    const auto identity=ship_identity_from_native_class(ship_class);
    if(!identity){stats_={};return;}
    draw(vp,camera,combat,*identity,seconds);
}
void ProjectileRenderer::draw(const Matrix& vp,const CameraState& camera,const SpaceCombat& combat,ShipIdentity identity,double seconds){
    if(!ship_profile(identity)){stats_={};return;}
    stats_={};if(!initialized()||!finite_matrix(vp)||!std::isfinite(seconds)||!finite(camera.position)||
        !std::isfinite(camera.yaw)||!std::isfinite(camera.pitch)||!std::isfinite(camera.roll))return;
    if(!pool_uploaded_)origin_seconds_=seconds;
    const float time=float(seconds-origin_seconds_);
    if(!std::isfinite(time))return;
    if(!pool_uploaded_||projectile_revision_!=combat.projectile_revision()){
        std::array<BoltInstance,SpaceCombat::projectile_capacity> instances{};bolt_count_=0;
        // CCD-retired slots retain their clipped lifetime for the previous /
        // current interpolation interval; the shader hides them at exact hit.
        for(const auto& p:combat.projectiles())if(p.id&&finite(p.birth_position)&&finite(p.velocity)&&length_squared(p.velocity)>1e-10f&&
            std::isfinite(p.birth_seconds)&&std::isfinite(p.lifetime)&&p.lifetime>0){
            instances[std::size_t(bolt_count_++)]={{p.birth_position.x,p.birth_position.y,p.birth_position.z,p.lifetime},
                {p.velocity.x,p.velocity.y,p.velocity.z,float(p.birth_seconds-origin_seconds_)}};}
        if(bolt_count_){gl_.BindBuffer(array_buffer,bolt_instances_);gl_.BufferData(array_buffer,std::ptrdiff_t(std::size_t(bolt_count_)*sizeof(BoltInstance)),instances.data(),dynamic_draw);++stats_.instance_uploads;}
        projectile_revision_=combat.projectile_revision();pool_uploaded_=true;
    }
    if(effect_program_&&(!events_uploaded_||event_revision_!=combat.event_revision())){
        std::array<EventInstance,SpaceCombat::event_capacity> instances{};effect_count_=0;
        for(std::size_t i=0;i<combat.event_count();++i){const auto& e=combat.events()[i];const bool exploded=e.kind==SpaceCombatEventKind::Destroyed;
            const auto& sprite=exploded?explosion_:impact_;if(!sprite.texture||!finite(e.position)||!std::isfinite(e.scene_seconds))continue;
            const float life=sprite.fps>0?float(sprite.frames)/sprite.fps:(exploded?.65f:.18f);
            const float radius=exploded?std::max(e.radius,1.f):.8f;
            instances[std::size_t(effect_count_++)]={{e.position.x,e.position.y,e.position.z,radius},
                {float(e.scene_seconds-origin_seconds_),life,exploded?1.f:0.f,0}};}
        if(effect_count_){gl_.BindBuffer(array_buffer,effect_instances_);gl_.BufferData(array_buffer,std::ptrdiff_t(std::size_t(effect_count_)*sizeof(EventInstance)),instances.data(),dynamic_draw);++stats_.instance_uploads;}
        event_revision_=combat.event_revision();events_uploaded_=true;
    }
    if(fragment_program_&&(!fragments_uploaded_||fragment_revision_!=combat.event_revision())){
        std::array<FragmentInstance,SpaceCombat::event_capacity*2> instances{};fragment_count_=0;
        for(std::size_t i=0;i<combat.event_count();++i){const auto& e=combat.events()[i];
            if(e.kind!=SpaceCombatEventKind::Destroyed||!finite(e.position)||!finite(e.normal)||
                !std::isfinite(e.radius)||e.radius<=0||!std::isfinite(e.scene_seconds))continue;
            const auto normal=unit(e.normal),center=e.position-normal*e.radius;
            const auto tangent=unit(cross(normal,std::abs(normal.z)<.9f?Vec3{0,0,1}:Vec3{0,1,0}));
            const auto up=cross(normal,tangent);
            for(unsigned part=0;part<2;++part){const float sign=part?1.f:-1.f,a=fragment_seed(e.id,part*3),b=fragment_seed(e.id,part*3+1),c=fragment_seed(e.id,part*3+2);
                const auto position=center+tangent*(sign*e.radius*.18f);
                const auto velocity=unit(normal*.45f+tangent*(sign*.9f)+up*(b-.5f))*(5+5*a);
                const auto axis=unit(normal*(a-.5f)+tangent*sign+up*(c-.5f));
                const float spin=(30+150*c)*.01745329252f;
                instances[std::size_t(fragment_count_++)]={{position.x,position.y,position.z,float(e.scene_seconds-origin_seconds_)},
                    {velocity.x,velocity.y,velocity.z,e.radius*(.26f+.08f*b)},{axis.x,axis.y,axis.z,spin}};
            }
        }
        if(fragment_count_){gl_.BindBuffer(array_buffer,fragment_instances_);gl_.BufferData(array_buffer,std::ptrdiff_t(std::size_t(fragment_count_)*sizeof(FragmentInstance)),instances.data(),dynamic_draw);++stats_.instance_uploads;}
        fragment_revision_=combat.event_revision();fragments_uploaded_=true;
    }
    gl_.ActiveTexture(0x84c0);gl_.Enable(0xb71);gl_.DepthFunc(0x201);gl_.Disable(0xb44);gl_.Disable(0xb90);gl_.Disable(0xc11);
    if(fragment_count_){unsigned visible{};for(std::size_t i=0;i<combat.event_count();++i){const auto& e=combat.events()[i];const double age=seconds-e.scene_seconds;
            if(e.kind==SpaceCombatEventKind::Destroyed&&age>=0&&age<.5)visible+=2;}
        if(visible){gl_.DepthMask(1);gl_.Disable(0xbe2);gl_.UseProgram(fragment_program_);gl_.UniformMatrix4fv(fragment_u_.vp,1,0,vp.data());gl_.Uniform1f(fragment_u_.time,time);
            gl_.Uniform4fv(fragment_u_.wrap,1,fragment_wrap_.data());gl_.BindTexture(0xde1,fragment_texture_);gl_.BindVertexArray(fragment_vao_);
            gl_.DrawElementsInstanced(4,fragment_indices_,0x1405,nullptr,fragment_count_);++stats_.draws;stats_.fragments=visible;stats_.triangles+=unsigned(fragment_indices_/3)*visible;}}
    gl_.DepthMask(0);gl_.Enable(0xbe2);gl_.BlendFuncSeparate(0x302,1,1,1);
    unsigned visible_bolts{};
    for(const auto& p:combat.projectiles())if(p.id&&p.lifetime>0){const double age=seconds-p.birth_seconds;if(age>=0&&age<p.lifetime)++visible_bolts;}
    const auto* profile=ship_profile(identity);
    if(bolt_count_&&visible_bolts&&profile&&profile->weapons_allowed){
        gl_.UseProgram(bolt_program_);gl_.UniformMatrix4fv(bolt_u_.vp,1,0,vp.data());gl_.Uniform1f(bolt_u_.time,time);
        const auto ship=profile->slot;const float scale_tip[]={scales_[ship],tip_,growth_[ship],growth_distance_};
        gl_.Uniform4fv(bolt_u_.scale_tip,1,scale_tip);gl_.Uniform4fv(bolt_u_.wrap,1,texture_wrap_.data());
        gl_.BindTexture(0xde1,bolt_texture_);gl_.BindVertexArray(bolt_vao_);gl_.DrawElementsInstanced(4,bolt_indices_,0x1405,nullptr,bolt_count_);
        ++stats_.draws;stats_.projectiles=visible_bolts;stats_.triangles+=unsigned(bolt_indices_/3)*visible_bolts;
    }
    if(effect_count_){
        const auto basis=render_math::model({},camera.yaw,camera.pitch,camera.roll,1);const float right[]={-basis[4],-basis[5],-basis[6],0},up[]={basis[8],basis[9],basis[10],0};
        gl_.UseProgram(effect_program_);gl_.UniformMatrix4fv(effect_u_.vp,1,0,vp.data());gl_.Uniform1f(effect_u_.time,time);
        gl_.Uniform4fv(effect_u_.camera_right,1,right);gl_.Uniform4fv(effect_u_.camera_up,1,up);gl_.BindVertexArray(effect_vao_);
        for(int kind=0;kind<2;++kind){const auto& sprite=kind?explosion_:impact_;if(!sprite.texture)continue;unsigned alive{};
            const float life=sprite.fps>0?float(sprite.frames)/sprite.fps:(kind?.65f:.18f);
            for(std::size_t i=0;i<combat.event_count();++i){const auto& e=combat.events()[i];const double age=seconds-e.scene_seconds;
                if(int(e.kind==SpaceCombatEventKind::Destroyed)==kind&&age>=0&&age<life)++alive;}
            if(!alive)continue;
            const float atlas[]={float(sprite.columns),float(sprite.rows),float(sprite.frames),sprite.fps},texel[]={1.f/sprite.width,1.f/sprite.height,0,0};
            gl_.Uniform1i(effect_u_.effect_kind,kind);gl_.Uniform4fv(effect_u_.atlas,1,atlas);gl_.Uniform4fv(effect_u_.atlas_texel,1,texel);
            gl_.BindTexture(0xde1,sprite.texture);gl_.DrawElementsInstanced(4,6,0x1405,nullptr,effect_count_);
            ++stats_.draws;stats_.effects+=alive;stats_.triangles+=alive*2;
        }
    }
    gl_.BindVertexArray(0);gl_.UseProgram(0);
}
void ProjectileRenderer::close()noexcept{
    const auto release=[](auto&& operation)noexcept{try{operation();}catch(...){}};
    for(auto id:{bolt_vao_,effect_vao_,fragment_vao_})if(id)release([&]{gl_.DeleteVertexArrays(1,&id);});
    for(auto id:{bolt_vbo_,bolt_ebo_,bolt_instances_,effect_vbo_,effect_ebo_,effect_instances_,fragment_vbo_,fragment_ebo_,fragment_instances_})if(id)release([&]{gl_.DeleteBuffers(1,&id);});
    for(auto id:{bolt_texture_,impact_.texture,explosion_.texture,fragment_texture_})if(id)release([&]{gl_.DeleteTextures(1,&id);});
    for(auto id:{bolt_program_,effect_program_,fragment_program_})if(id)release([&]{gl_.DeleteProgram(id);});
    bolt_program_=effect_program_=bolt_vao_=bolt_vbo_=bolt_ebo_=bolt_instances_=bolt_texture_=0;
    effect_vao_=effect_vbo_=effect_ebo_=effect_instances_=0;bolt_count_=effect_count_=bolt_indices_=0;
    fragment_program_=fragment_vao_=fragment_vbo_=fragment_ebo_=fragment_instances_=fragment_texture_=0;fragment_indices_=fragment_count_=0;
    pool_uploaded_=events_uploaded_=fragments_uploaded_=false;projectile_revision_=event_revision_=fragment_revision_=0;impact_={};explosion_={};stats_={};
}
}
