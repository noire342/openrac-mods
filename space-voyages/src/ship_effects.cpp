// SPDX-License-Identifier: GPL-3.0-or-later
#include "ship_effects.hpp"
#include "render_math.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace spacevoyages {
namespace {
constexpr float radius_boost=.25f,length_boost=1.6f,pulse_amount=.06f,intensity_boost=.55f,pulse_frequency=27.f;
constexpr unsigned array_buffer=0x8892,element_buffer=0x8893,static_draw=0x88e4;
struct EffectVertex {float position[3],uv[2];};
struct MountInstance {std::array<float,4> position_radius,direction_length,seed;};
static_assert(sizeof(EffectVertex)==20&&sizeof(MountInstance)==48,"Ship effect GPU formats");
constexpr ShipEffects::Matrix identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
constexpr ShipEffects::Matrix outside_clip{0,0,0,0,0,0,0,0,0,0,0,0,2,2,2,1};
float bounded(float value,float low,float high){return std::isfinite(value)?std::clamp(value,low,high):low;}
bool finite_matrix(const ShipEffects::Matrix& matrix){for(float v:matrix)if(!std::isfinite(v))return false;return true;}
void validate_sprite(const EffectSprite& sprite){
    if(sprite.rgba.empty())return;
    if(sprite.width<1||sprite.height<1||sprite.width>4096||sprite.height>4096||
       sprite.rgba.size()!=std::size_t(sprite.width)*std::size_t(sprite.height)*4||
       !sprite.columns||!sprite.rows||sprite.columns>std::uint32_t(sprite.width)||sprite.rows>std::uint32_t(sprite.height)||
       sprite.width%int(sprite.columns)||sprite.height%int(sprite.rows)||!sprite.frames||
       sprite.frames>sprite.columns*sprite.rows||!std::isfinite(sprite.fps)||sprite.fps<0||sprite.fps>1000)
        throw std::runtime_error("Invalid original ship-effect sprite/atlas");
}
constexpr const char* flame_vertex=R"GLSL(#version 330 core
layout(location=0)in vec3 unit_position;
layout(location=1)in vec2 uv;
layout(location=2)in vec4 mount_position_radius;
layout(location=3)in vec4 mount_direction_length;
layout(location=4)in vec4 mount_seed;
uniform mat4 view_projection;
uniform mat4 ship_model;
uniform vec4 time_boost;
uniform vec4 plume_params;
out vec2 t;
out float strength;
void main(){
    vec3 axis=mount_direction_length.xyz;
    vec3 reference=abs(axis.z)<.9?vec3(0,0,1):vec3(0,1,0);
    vec3 right=normalize(cross(axis,reference)),up=cross(right,axis);
    float pulse=1.0-plume_params.z+plume_params.z*sin(time_boost.y+mount_seed.x);
    float radius=mount_position_radius.w*(1.0+plume_params.x*time_boost.z);
    float extent=mount_direction_length.w*(1.0+plume_params.y*time_boost.z)*pulse;
    vec3 local=mount_position_radius.xyz+axis*(unit_position.z*extent)
        +(right*unit_position.x+up*unit_position.y)*radius;
    gl_Position=view_projection*ship_model*vec4(local,1.0);
    t=uv;strength=(1.0+plume_params.w*time_boost.z)*pulse;
})GLSL";
constexpr const char* sprite_vertex=R"GLSL(#version 330 core
layout(location=0)in vec3 unit_position;
layout(location=1)in vec2 uv;
uniform mat4 view_projection;
uniform mat4 ship_model;
uniform vec4 camera_right;
uniform vec4 camera_up;
uniform vec4 effect;
out vec2 t;
out float strength;
void main(){
    float scale=length(ship_model[0].xyz);
    vec3 center=ship_model[3].xyz;
    vec3 offset=(camera_right.xyz*unit_position.x+camera_up.xyz*unit_position.y)*effect.x*scale;
    gl_Position=view_projection*vec4(center+offset,1.0);
    t=uv;strength=effect.z;
})GLSL";
constexpr const char* effect_fragment=R"GLSL(#version 330 core
in vec2 t;
in float strength;
uniform sampler2D image;
uniform vec4 atlas;
uniform vec4 atlas_texel;
uniform vec4 time_boost;
uniform vec4 effect;
uniform vec4 effect_tint;
out vec4 color;
void main(){
    float frame=effect.w<0.0?mod(floor(max(time_boost.x,0.0)*atlas.w),atlas.z)
        :min(floor(clamp(effect.y,0.0,1.0)*atlas.z),atlas.z-1.0);
    vec2 cell=vec2(mod(frame,atlas.x),floor(frame/atlas.x));
    vec2 half_texel=atlas_texel.xy*vec2(atlas.x,atlas.y)*.5;
    vec2 local=clamp(t,half_texel,vec2(1.0)-half_texel);
    vec4 p=texture(image,(cell+local)/atlas.xy)*effect_tint;
    color=vec4(p.rgb*strength,clamp(p.a*clamp(strength,0.0,1.0),0.0,1.0));
})GLSL";
}

namespace ship_effects_math {
Vec3 transform_point(const Matrix& m,Vec3 p)noexcept{
    return {m[0]*p.x+m[4]*p.y+m[8]*p.z+m[12],m[1]*p.x+m[5]*p.y+m[9]*p.z+m[13],m[2]*p.x+m[6]*p.y+m[10]*p.z+m[14]};
}
Vec3 transform_direction(const Matrix& m,Vec3 d)noexcept{
    return {m[0]*d.x+m[4]*d.y+m[8]*d.z,m[1]*d.x+m[5]*d.y+m[9]*d.z,m[2]*d.x+m[6]*d.y+m[10]*d.z};
}
FlameScale flame_scale(float boost,float time,float seed)noexcept{
    boost=bounded(boost,0,1);if(!std::isfinite(time))time=0;if(!std::isfinite(seed))seed=0;
    const float pulse=1-pulse_amount+pulse_amount*std::sin(time*pulse_frequency+seed);
    return {1+radius_boost*boost,(1+length_boost*boost)*pulse,(1+intensity_boost*boost)*pulse};
}
}

unsigned ShipEffects::shader(unsigned kind,const char* source){
    const auto result=gl_.CreateShader(kind);
    if(!result)throw std::runtime_error("Could not create ship-effect shader");
    try{
        gl_.ShaderSource(result,1,&source,nullptr);gl_.CompileShader(result);int success{};gl_.GetShaderiv(result,0x8b81,&success);
        if(!success){char error[2048]{};gl_.GetShaderInfoLog(result,sizeof error,nullptr,error);throw std::runtime_error(std::string("Ship-effect shader: ")+error);}
    }catch(...){gl_.DeleteShader(result);throw;}
    return result;
}
unsigned ShipEffects::program(const char* vertex,const char* fragment){
    unsigned a{},b{},result{};
    try{
        a=shader(0x8b31,vertex);b=shader(0x8b30,fragment);result=gl_.CreateProgram();
        if(!result)throw std::runtime_error("Could not create ship-effect program");
        gl_.AttachShader(result,a);gl_.AttachShader(result,b);gl_.LinkProgram(result);int success{};gl_.GetProgramiv(result,0x8b82,&success);
        if(!success){char error[2048]{};gl_.GetProgramInfoLog(result,sizeof error,nullptr,error);throw std::runtime_error(std::string("Ship-effect program: ")+error);}
        gl_.DeleteShader(a);a=0;gl_.DeleteShader(b);return result;
    }catch(...){if(a)gl_.DeleteShader(a);if(b)gl_.DeleteShader(b);if(result)gl_.DeleteProgram(result);throw;}
}
ShipEffects::SpriteGpu ShipEffects::texture(const EffectSprite& src){
    SpriteGpu result{};if(src.rgba.empty())return result;
    try{
        gl_.GenTextures(1,&result.texture);if(!result.texture)throw std::runtime_error("Could not allocate ship-effect texture");
        gl_.BindTexture(0xde1,result.texture);gl_.TexParameteri(0xde1,0x2801,0x2601);gl_.TexParameteri(0xde1,0x2800,0x2601);
        gl_.TexParameteri(0xde1,0x2802,0x812f);gl_.TexParameteri(0xde1,0x2803,0x812f);
        gl_.TexImage2D(0xde1,0,0x8058,src.width,src.height,0,0x1908,0x1401,src.rgba.data());
    }catch(...){if(result.texture)gl_.DeleteTextures(1,&result.texture);throw;}
    result.columns=int(src.columns);result.rows=int(src.rows);result.frames=int(src.frames);result.fps=src.fps;
    result.width=src.width;result.height=src.height;return result;
}

void ShipEffects::initialize(GlApi::GetProc loader,const ShipEffectsAssets& assets){
    if(initialized())return;
    validate_sprite(assets.flame);validate_sprite(assets.hit);validate_sprite(assets.explosion);
    bool has_mounts=false;
    for(const auto& ship:assets.ships){
        if(ship.count>ship.mounts.size())throw std::runtime_error("Too many ship exhaust mounts");
        has_mounts=has_mounts||ship.count!=0;
        if(!assets.flame.rgba.empty())for(float channel:ship.color)
            if(!std::isfinite(channel)||channel<0||channel>2)throw std::runtime_error("Invalid native exhaust tint");
        if(!assets.flame.rgba.empty())for(std::size_t i=0;i<ship.count;++i){
            const auto& mount=ship.mounts[i];
            if(!finite(mount.position)||!finite(mount.direction)||length_squared(mount.direction)<.000001f||
               !std::isfinite(mount.radius)||mount.radius<=0||mount.radius>100||!std::isfinite(mount.length)||mount.length<=0||mount.length>100)
                throw std::runtime_error("Invalid ship exhaust anchor");
        }
    }
    const bool flames=has_mounts&&!assets.flame.rgba.empty(),sprites=!assets.hit.rgba.empty()||!assets.explosion.rgba.empty();
    if(!flames&&!sprites)return; // Legacy packs do not even query a GL proc.
    gl_.load(loader);
    try{
        const auto uniforms=[&](unsigned program_id){
            Uniforms u{};
            u.vp=gl_.GetUniformLocation(program_id,"view_projection");u.model=gl_.GetUniformLocation(program_id,"ship_model");
            u.camera_right=gl_.GetUniformLocation(program_id,"camera_right");u.camera_up=gl_.GetUniformLocation(program_id,"camera_up");
            u.time_boost=gl_.GetUniformLocation(program_id,"time_boost");u.plume_params=gl_.GetUniformLocation(program_id,"plume_params");
            u.atlas=gl_.GetUniformLocation(program_id,"atlas");u.atlas_texel=gl_.GetUniformLocation(program_id,"atlas_texel");u.effect=gl_.GetUniformLocation(program_id,"effect");
            u.effect_tint=gl_.GetUniformLocation(program_id,"effect_tint");
            gl_.UseProgram(program_id);gl_.Uniform1i(gl_.GetUniformLocation(program_id,"image"),0);return u;
        };
        const auto attributes=[&]{
            gl_.EnableVertexAttribArray(0);gl_.VertexAttribPointer(0,3,0x1406,0,sizeof(EffectVertex),nullptr);
            gl_.EnableVertexAttribArray(1);gl_.VertexAttribPointer(1,2,0x1406,0,sizeof(EffectVertex),reinterpret_cast<const void*>(12));
        };
        if(flames){
            flame_program_=program(flame_vertex,effect_fragment);flame_uniforms_=uniforms(flame_program_);flame_=texture(assets.flame);
            std::array<EffectVertex,27> vertices{};std::array<unsigned,96> indices{};
            constexpr float extent[3]={0,.35f,1},radius[3]={1,.6f,.02f};
            for(unsigned ring=0;ring<3;++ring)for(unsigned side=0;side<=8;++side){const float angle=float(side)*2*pi/8;
                vertices[ring*9+side]={{std::cos(angle)*radius[ring],std::sin(angle)*radius[ring],extent[ring]},{float(side)/8,extent[ring]}};
            }
            std::size_t at=0;for(unsigned ring=0;ring<2;++ring)for(unsigned side=0;side<8;++side){const unsigned a=ring*9+side,b=a+1,c=a+9,d=c+1;for(auto index:{a,b,c,b,d,c})indices[at++]=index;}
            flame_indices_=int(indices.size());
            gl_.GenBuffers(1,&flame_vbo_);if(!flame_vbo_)throw std::runtime_error("Could not allocate exhaust vertices");gl_.BindBuffer(array_buffer,flame_vbo_);gl_.BufferData(array_buffer,sizeof vertices,vertices.data(),static_draw);
            gl_.GenBuffers(1,&flame_ebo_);if(!flame_ebo_)throw std::runtime_error("Could not allocate exhaust indices");gl_.BindBuffer(array_buffer,flame_ebo_);gl_.BufferData(array_buffer,sizeof indices,indices.data(),static_draw);
            for(std::size_t ship=0;ship<assets.ships.size();++ship){
                const auto& src=assets.ships[ship];if(!src.count)continue;auto& dst=mounts_[ship];dst.count=int(src.count);dst.color=src.color;
                std::array<MountInstance,ShipExhaustMounts::capacity> instances{};
                for(std::size_t i=0;i<src.count;++i){const auto& mount=src.mounts[i];const auto axis=unit(mount.direction);
                    instances[i]={{mount.position.x,mount.position.y,mount.position.z,mount.radius},{axis.x,axis.y,axis.z,mount.length},{float(ship*17+i)*2.399963f,0,0,0}};
                }
                gl_.GenBuffers(1,&dst.vbo);if(!dst.vbo)throw std::runtime_error("Could not allocate exhaust anchors");gl_.BindBuffer(array_buffer,dst.vbo);gl_.BufferData(array_buffer,std::ptrdiff_t(src.count*sizeof(MountInstance)),instances.data(),static_draw);
                gl_.GenVertexArrays(1,&dst.vao);if(!dst.vao)throw std::runtime_error("Could not allocate exhaust array");gl_.BindVertexArray(dst.vao);
                gl_.BindBuffer(array_buffer,flame_vbo_);attributes();gl_.BindBuffer(element_buffer,flame_ebo_);gl_.BindBuffer(array_buffer,dst.vbo);
                for(unsigned location=2;location<5;++location){gl_.EnableVertexAttribArray(location);gl_.VertexAttribPointer(location,4,0x1406,0,sizeof(MountInstance),reinterpret_cast<const void*>(std::size_t(location-2)*16));gl_.VertexAttribDivisor(location,1);}
            }
        }
        if(sprites){
            sprite_program_=program(sprite_vertex,effect_fragment);sprite_uniforms_=uniforms(sprite_program_);hit_=texture(assets.hit);explosion_=texture(assets.explosion);
            constexpr std::array<EffectVertex,4> vertices{{{{-1,-1,0},{0,0}},{{1,-1,0},{1,0}},{{1,1,0},{1,1}},{{-1,1,0},{0,1}}}};
            constexpr unsigned indices[]={0,1,2,0,2,3};
            gl_.GenVertexArrays(1,&quad_vao_);if(!quad_vao_)throw std::runtime_error("Could not allocate ship-effect array");gl_.BindVertexArray(quad_vao_);
            gl_.GenBuffers(1,&quad_vbo_);if(!quad_vbo_)throw std::runtime_error("Could not allocate ship-effect vertices");gl_.BindBuffer(array_buffer,quad_vbo_);gl_.BufferData(array_buffer,sizeof vertices,vertices.data(),static_draw);attributes();
            gl_.GenBuffers(1,&quad_ebo_);if(!quad_ebo_)throw std::runtime_error("Could not allocate ship-effect indices");gl_.BindBuffer(element_buffer,quad_ebo_);gl_.BufferData(element_buffer,sizeof indices,indices,static_draw);
        }
        // Warm every ship-specific VAO and both optional material paths while
        // the owner is black. Uniform values/boost do not change the pipeline.
        CameraState camera{};
        for(const auto& profile:ship_profiles)draw(outside_clip,identity,camera,profile.identity,1,0,1,-1);
        draw(outside_clip,identity,camera,ShipIdentity::Green,0,0,0,.2f);
        stats_={};
    }catch(...){close();throw;}
}

void ShipEffects::draw(const Matrix& vp,const Matrix& model,const CameraState& camera,int ship,float boost,float time,float hit_flash,float destruction){
    const auto selected=ship_identity_from_native_class(ship);
    if(!selected){stats_={};return;}
    draw(vp,model,camera,*selected,boost,time,hit_flash,destruction);
}
void ShipEffects::draw(const Matrix& vp,const Matrix& model,const CameraState& camera,ShipIdentity selected,float boost,float time,float hit_flash,float destruction){
    stats_={};
    const auto* profile=ship_profile(selected);
    if(!initialized()||!profile||!finite_matrix(vp)||!finite_matrix(model)||!std::isfinite(time)||
       !finite(camera.position)||!std::isfinite(camera.yaw)||!std::isfinite(camera.pitch)||!std::isfinite(camera.roll)||!std::isfinite(destruction))return;
    const bool dying=destruction>=0;const float progress=bounded(destruction,0,1),hit=bounded(hit_flash,0,1);
    const auto& mount=mounts_[profile->slot];
    const bool show_flame=!dying&&flame_.texture&&mount.count;
    const bool show_sprite=dying?(explosion_.texture&&progress<1):(hit_.texture&&hit>0);
    if(!show_flame&&!show_sprite)return;
    gl_.ActiveTexture(0x84c0);gl_.Enable(0xb71);gl_.DepthFunc(0x201);gl_.DepthMask(0);gl_.Enable(0xbe2);
    gl_.BlendFuncSeparate(0x302,1,1,1);gl_.Disable(0xb44);gl_.Disable(0xb90);gl_.Disable(0xc11);
    const float time_boost[4]={time,time*pulse_frequency,bounded(boost,0,1),0};
    const auto sprite_uniforms=[&](const Uniforms& u,const SpriteGpu& sprite){
        const float atlas[4]={float(sprite.columns),float(sprite.rows),float(sprite.frames),sprite.fps};
        const float texel[4]={1.f/sprite.width,1.f/sprite.height,0,0};
        gl_.UniformMatrix4fv(u.vp,1,0,vp.data());gl_.UniformMatrix4fv(u.model,1,0,model.data());
        gl_.Uniform4fv(u.time_boost,1,time_boost);gl_.Uniform4fv(u.atlas,1,atlas);gl_.Uniform4fv(u.atlas_texel,1,texel);gl_.BindTexture(0xde1,sprite.texture);
    };
    if(show_flame){
        gl_.UseProgram(flame_program_);sprite_uniforms(flame_uniforms_,flame_);
        const float params[4]={radius_boost,length_boost,pulse_amount,intensity_boost},effect[4]={0,0,1,-1};
        gl_.Uniform4fv(flame_uniforms_.plume_params,1,params);gl_.Uniform4fv(flame_uniforms_.effect,1,effect);
        gl_.Uniform4fv(flame_uniforms_.effect_tint,1,mount.color.data());
        gl_.BindVertexArray(mount.vao);gl_.DrawElementsInstanced(4,flame_indices_,0x1405,nullptr,mount.count);
        ++stats_.draws;stats_.thrusters=unsigned(mount.count);stats_.triangles+=unsigned(flame_indices_/3)*unsigned(mount.count);
    }
    if(show_sprite){
        gl_.UseProgram(sprite_program_);sprite_uniforms(sprite_uniforms_,dying?explosion_:hit_);
        const auto basis=render_math::model({},camera.yaw,camera.pitch,camera.roll,1);
        const float right[4]={-basis[4],-basis[5],-basis[6],0},up[4]={basis[8],basis[9],basis[10],0};
        const float effect[4]={dying?4.2f*(1+progress*1.8f):2.8f,dying?progress:1-hit,dying?(1-progress)*(1-progress):hit,dying?1.f:0.f};
        gl_.Uniform4fv(sprite_uniforms_.camera_right,1,right);gl_.Uniform4fv(sprite_uniforms_.camera_up,1,up);gl_.Uniform4fv(sprite_uniforms_.effect,1,effect);
        const float white[4]={1,1,1,1};gl_.Uniform4fv(sprite_uniforms_.effect_tint,1,white);
        gl_.BindVertexArray(quad_vao_);gl_.DrawElements(4,6,0x1405,nullptr);++stats_.draws;++stats_.sprites;stats_.triangles+=2;
    }
    gl_.BindVertexArray(0);gl_.UseProgram(0);
}
void ShipEffects::close()noexcept{
    const auto release=[](auto&& operation)noexcept{try{operation();}catch(...){}};
    for(auto& mount:mounts_){if(mount.vao)release([&]{gl_.DeleteVertexArrays(1,&mount.vao);});if(mount.vbo)release([&]{gl_.DeleteBuffers(1,&mount.vbo);});mount={};}
    for(auto* sprite:{&flame_,&hit_,&explosion_}){if(sprite->texture)release([&]{gl_.DeleteTextures(1,&sprite->texture);});*sprite={};}
    for(auto* buffer:{&flame_vbo_,&flame_ebo_,&quad_vbo_,&quad_ebo_}){if(*buffer)release([&]{gl_.DeleteBuffers(1,buffer);});*buffer=0;}
    if(quad_vao_)release([&]{gl_.DeleteVertexArrays(1,&quad_vao_);});quad_vao_=0;
    if(flame_program_)release([&]{gl_.DeleteProgram(flame_program_);});if(sprite_program_)release([&]{gl_.DeleteProgram(sprite_program_);});flame_program_=sprite_program_=0;flame_indices_=0;stats_={};
}
}
