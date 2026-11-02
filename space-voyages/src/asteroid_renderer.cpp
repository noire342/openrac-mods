// SPDX-License-Identifier: GPL-3.0-or-later
#include "asteroid_renderer.hpp"
#include "render_math.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

namespace spacevoyages {
namespace {
constexpr unsigned array_buffer=0x8892,element_buffer=0x8893,static_draw=0x88e4;
constexpr unsigned texture_2d=0x0de1,float_type=0x1406;
struct Instance {
    std::array<float,4> center_radius,axis_speed,phase_tint;
};
static_assert(sizeof(Instance)==48,"Static asteroid instance format");

std::optional<std::uint32_t> variant_id(const std::string& id){
    constexpr std::string_view prefix="asteroid_";
    if(id.compare(0,prefix.size(),prefix)!=0)return std::nullopt;
    const auto first=id.data()+prefix.size(),end=id.data()+id.size();
    std::uint32_t result{};
    const auto parsed=std::from_chars(first,end,result);
    if(parsed.ec!=std::errc{}||parsed.ptr==first)
        throw std::runtime_error("Invalid asteroid variant id: "+id);
    if(parsed.ptr!=end){
        constexpr std::string_view part="_part";
        if(std::string_view(parsed.ptr,std::size_t(end-parsed.ptr)).substr(0,part.size())!=part)
            throw std::runtime_error("Invalid asteroid material part: "+id);
        unsigned ordinal{};
        const auto suffix=std::from_chars(parsed.ptr+part.size(),end,ordinal);
        if(suffix.ec!=std::errc{}||suffix.ptr!=end)
            throw std::runtime_error("Invalid asteroid material part: "+id);
    }
    return result;
}
void validate_mesh(const MeshAsset& mesh){
    if(mesh.vertices.empty()||mesh.indices.empty()||mesh.indices.size()%3||
       mesh.indices.size()>std::size_t(std::numeric_limits<int>::max()))
        throw std::runtime_error("Invalid asteroid geometry: "+mesh.id);
    if(mesh.alpha==AlphaMode::Blend)
        throw std::runtime_error("Asteroid transparency needs an ordered material pass: "+mesh.id);
    if(mesh.width<1||mesh.height<1||mesh.width>4096||mesh.height>4096||
       mesh.rgba.size()!=std::size_t(mesh.width)*std::size_t(mesh.height)*4)
        throw std::runtime_error("Invalid asteroid texture: "+mesh.id);
    // The visual unit sphere must fit the static collision/culling sphere at
    // every rotation. Never shrink this test using a view-dependent bound.
    for(const auto& vertex:mesh.vertices){
        const Vec3 position{vertex.position[0],vertex.position[1],vertex.position[2]};
        if(!finite(position)||length_squared(position)>1.0002f)
            throw std::runtime_error("Asteroid asset exceeds its unit sphere: "+mesh.id);
    }
    for(auto index:mesh.indices)if(index>=mesh.vertices.size())
        throw std::runtime_error("Invalid asteroid index: "+mesh.id);
}
constexpr const char* vertex_source=R"GLSL(#version 330 core
layout(location=0)in vec3 position;
layout(location=1)in vec3 normal;
layout(location=2)in vec2 uv;
layout(location=3)in vec4 vertex_color;
layout(location=4)in vec4 tile_rect;
layout(location=5)in vec4 center_radius;
layout(location=6)in vec4 axis_speed;
layout(location=7)in vec4 phase_tint;
uniform mat4 view_projection;
uniform float presentation_time;
uniform sampler2D alive_mask;
out vec3 n;
out vec2 t;
out vec4 vcolor;
out vec4 rect;
vec3 rotate_axis(vec3 p,vec3 axis,float c,float s){
    return p*c+cross(axis,p)*s+axis*dot(axis,p)*(1.0-c);
}
void main(){
    float angle=phase_tint.x+axis_speed.w*presentation_time;
    float c=cos(angle),s=sin(angle);
    vec3 p=rotate_axis(position,axis_speed.xyz,c,s)*center_radius.w+center_radius.xyz;
    gl_Position=view_projection*vec4(p,1.0);
    int rock_id=int(phase_tint.w+.5);
    vec2 alive_time=texelFetch(alive_mask,ivec2(rock_id%256,rock_id/256),0).rg;
    if(alive_time.x<.5&&presentation_time>=alive_time.y)
        gl_Position=vec4(2.0,2.0,2.0,1.0);
    n=rotate_axis(normal,axis_speed.xyz,c,s);
    t=uv;vcolor=vertex_color*vec4(phase_tint.yz,1.0,1.0);rect=tile_rect;
})GLSL";
constexpr const char* fragment_source=R"GLSL(#version 330 core
in vec3 n;
in vec2 t;
in vec4 vcolor;
in vec4 rect;
uniform sampler2D image;
uniform int alpha_mode;
uniform float alpha_cutoff;
uniform int atlas_mode;
uniform int wrap_s;
uniform int wrap_t;
out vec4 color;
float tile(float u,int w){
    if(w==10497)return fract(u);
    if(w==33648)return 1.0-abs(mod(u,2.0)-1.0);
    return clamp(u,0.0,1.0);
}
void main(){
    vec2 st=atlas_mode!=0?rect.zw+vec2(tile(t.x,wrap_s),tile(t.y,wrap_t))*rect.xy:t;
    vec4 p=texture(image,st)*vcolor;
    if(alpha_mode==1&&p.a<alpha_cutoff)discard;
    float light=max(dot(normalize(n),normalize(vec3(-.4,-.3,.85))),0.0);
    color=vec4(p.rgb*(.86+.14*light),1.0);
})GLSL";
}

unsigned AsteroidRenderer::shader(unsigned kind,const char* source){
    const auto result=gl_.CreateShader(kind);
    if(!result)throw std::runtime_error("Could not create asteroid shader");
    try{
        gl_.ShaderSource(result,1,&source,nullptr);gl_.CompileShader(result);
        int success{};gl_.GetShaderiv(result,0x8b81,&success);
        if(!success){
            char error[2048]{};gl_.GetShaderInfoLog(result,sizeof error,nullptr,error);
            throw std::runtime_error(std::string("Asteroid shader: ")+error);
        }
    }catch(...){gl_.DeleteShader(result);throw;}
    return result;
}

void AsteroidRenderer::initialize(GlApi::GetProc loader,const SceneAssets& assets,const AsteroidLayout& layout){
    if(initialized())return;
    gl_.load(loader);
    unsigned vertex_shader{},fragment_shader{};
    try{
        if(layout.asteroids().size()>16384||layout.fields().size()>1024)
            throw std::runtime_error("Asteroid layout exceeds the bounded scene budget");
        std::map<std::uint32_t,std::vector<const MeshAsset*>> sources;
        for(const auto& mesh:assets.meshes)if(const auto variant=variant_id(mesh.id)){
            validate_mesh(mesh);sources[*variant].push_back(&mesh);
        }
        for(const auto& rock:layout.asteroids()){
            if(!finite(rock.position)||!std::isfinite(rock.radius)||rock.radius<=0||
               !finite(rock.rotation_axis)||length_squared(rock.rotation_axis)<.000001f||
               !std::isfinite(rock.angular_speed)||!std::isfinite(rock.rotation_phase))
                throw std::runtime_error("Invalid static asteroid instance");
            if(sources.find(rock.variant)==sources.end())
                throw std::runtime_error("Missing asteroid variant "+std::to_string(rock.variant));
        }
        std::vector<unsigned char> covered(layout.asteroids().size(),0);
        for(const auto& field:layout.fields()){
            if(field.begin>covered.size()||field.count>covered.size()-field.begin)
                throw std::runtime_error("Invalid asteroid field range");
            for(std::size_t i=field.begin;i<field.begin+field.count;++i)
                if(covered[i]++||layout.asteroids()[i].field_id!=field.id)
                    throw std::runtime_error("Overlapping or mismatched asteroid field range");
        }
        if(std::find(covered.begin(),covered.end(),0)!=covered.end())
            throw std::runtime_error("Incomplete asteroid field ranges");

        vertex_shader=shader(0x8b31,vertex_source);fragment_shader=shader(0x8b30,fragment_source);
        program_=gl_.CreateProgram();
        if(!program_)throw std::runtime_error("Could not create asteroid program");
        gl_.AttachShader(program_,vertex_shader);gl_.AttachShader(program_,fragment_shader);gl_.LinkProgram(program_);
        int linked{};gl_.GetProgramiv(program_,0x8b82,&linked);
        if(!linked){
            char error[2048]{};gl_.GetProgramInfoLog(program_,sizeof error,nullptr,error);
            throw std::runtime_error(std::string("Asteroid program: ")+error);
        }
        gl_.DeleteShader(vertex_shader);vertex_shader=0;gl_.DeleteShader(fragment_shader);fragment_shader=0;
        vp_=gl_.GetUniformLocation(program_,"view_projection");time_=gl_.GetUniformLocation(program_,"presentation_time");
        alpha_mode_=gl_.GetUniformLocation(program_,"alpha_mode");alpha_cutoff_=gl_.GetUniformLocation(program_,"alpha_cutoff");
        atlas_mode_=gl_.GetUniformLocation(program_,"atlas_mode");wrap_s_=gl_.GetUniformLocation(program_,"wrap_s");wrap_t_=gl_.GetUniformLocation(program_,"wrap_t");
        gl_.UseProgram(program_);gl_.Uniform1i(gl_.GetUniformLocation(program_,"image"),0);
        gl_.Uniform1i(gl_.GetUniformLocation(program_,"alive_mask"),1);
        asteroid_count_=layout.asteroids().size();mask_height_=int(std::max(std::size_t(1),(asteroid_count_+255)/256));
        mask_pixels_.assign(std::size_t(mask_height_)*256*4,0);
        for(std::size_t i=0;i<mask_pixels_.size();i+=4){mask_pixels_[i]=1;mask_pixels_[i+3]=1;}
        gl_.GenTextures(1,&alive_texture_);
        if(!alive_texture_)throw std::runtime_error("Could not allocate asteroid alive mask");
        gl_.ActiveTexture(0x84c1);gl_.BindTexture(texture_2d,alive_texture_);
        gl_.TexParameteri(texture_2d,0x2801,0x2600);gl_.TexParameteri(texture_2d,0x2800,0x2600);
        gl_.TexParameteri(texture_2d,0x2802,33071);gl_.TexParameteri(texture_2d,0x2803,33071);
        gl_.TexImage2D(texture_2d,0,0x8814,256,mask_height_,0,0x1908,0x1406,mask_pixels_.data());
        gl_.ActiveTexture(0x84c0);

        std::map<std::uint32_t,std::vector<std::size_t>> variants;
        std::unordered_map<std::string,unsigned> texture_cache;
        for(const auto& source:sources)for(const auto* mesh:source.second){
            geometry_.emplace_back();auto& geometry=geometry_.back();
            geometry.count=int(mesh->indices.size());geometry.alpha=mesh->alpha;geometry.alpha_cutoff=mesh->alpha_cutoff;
            geometry.atlas=mesh->atlas;geometry.wrap_s=mesh->wrap_s;geometry.wrap_t=mesh->wrap_t;
            gl_.GenBuffers(1,&geometry.vbo);
            if(!geometry.vbo)throw std::runtime_error("Could not allocate asteroid vertex buffer");
            gl_.BindBuffer(array_buffer,geometry.vbo);
            gl_.BufferData(array_buffer,std::ptrdiff_t(mesh->vertices.size()*sizeof(Vertex)),mesh->vertices.data(),static_draw);
            // Element buffers are VAO state on desktop GL. Bind them only
            // when constructing a real batch VAO below, never on VAO zero.
            gl_.GenBuffers(1,&geometry.ebo);
            if(!geometry.ebo)throw std::runtime_error("Could not allocate asteroid index buffer");
            gl_.BindBuffer(array_buffer,geometry.ebo);
            gl_.BufferData(array_buffer,std::ptrdiff_t(mesh->indices.size()*sizeof(unsigned)),mesh->indices.data(),static_draw);
            const auto sampler_s=mesh->atlas?33071:mesh->wrap_s,sampler_t=mesh->atlas?33071:mesh->wrap_t;
            const auto key=mesh->texture_key+":"+std::to_string(mesh->width)+":"+std::to_string(mesh->height)+":"+std::to_string(sampler_s)+":"+std::to_string(sampler_t);
            const auto cached=texture_cache.find(key);
            if(cached!=texture_cache.end())geometry.texture=cached->second;
            else{
                textures_.push_back(0);gl_.GenTextures(1,&textures_.back());geometry.texture=textures_.back();
                if(!geometry.texture)throw std::runtime_error("Could not allocate asteroid texture");
                gl_.BindTexture(texture_2d,geometry.texture);
                gl_.TexParameteri(texture_2d,0x2801,0x2601);gl_.TexParameteri(texture_2d,0x2800,0x2601);
                gl_.TexParameteri(texture_2d,0x2802,int(sampler_s));gl_.TexParameteri(texture_2d,0x2803,int(sampler_t));
                gl_.TexImage2D(texture_2d,0,0x8058,mesh->width,mesh->height,0,0x1908,0x1401,mesh->rgba.data());
                texture_cache.emplace(key,geometry.texture);
            }
            variants[source.first].push_back(geometry_.size()-1);
        }
        for(const auto& field:layout.fields())for(const auto& variant:variants){
            std::vector<Instance> instances;instances.reserve(field.count);
            Vec3 lower{1e30f,1e30f,1e30f},upper{-1e30f,-1e30f,-1e30f};
            for(std::size_t i=field.begin;i<field.begin+field.count;++i){
                const auto& rock=layout.asteroids()[i];if(rock.variant!=variant.first)continue;
                const auto axis=unit(rock.rotation_axis);
                instances.push_back({{rock.position.x,rock.position.y,rock.position.z,rock.radius},
                    {axis.x,axis.y,axis.z,rock.angular_speed},{rock.rotation_phase,1,1,float(i)}});
                const Vec3 radius{rock.radius,rock.radius,rock.radius},lo=rock.position-radius,hi=rock.position+radius;
                lower={std::min(lower.x,lo.x),std::min(lower.y,lo.y),std::min(lower.z,lo.z)};
                upper={std::max(upper.x,hi.x),std::max(upper.y,hi.y),std::max(upper.z,hi.z)};
            }
            if(instances.empty())continue;
            batches_.emplace_back();auto& batch=batches_.back();batch.instances=int(instances.size());batch.center=(lower+upper)*.5f;
            for(const auto& instance:instances){
                const Vec3 center{instance.center_radius[0],instance.center_radius[1],instance.center_radius[2]};
                batch.radius=std::max(batch.radius,length(center-batch.center)+instance.center_radius[3]);
            }
            // Covers normalization tolerance and floating-point frustum edges.
            batch.radius=batch.radius*1.0002f+.01f;
            gl_.GenBuffers(1,&batch.instance_vbo);
            if(!batch.instance_vbo)throw std::runtime_error("Could not allocate asteroid instance buffer");
            gl_.BindBuffer(array_buffer,batch.instance_vbo);
            gl_.BufferData(array_buffer,std::ptrdiff_t(instances.size()*sizeof(Instance)),instances.data(),static_draw);
            for(auto index:variant.second){
                const auto& geometry=geometry_[index];batch.parts.push_back({0,index});auto& part=batch.parts.back();
                gl_.GenVertexArrays(1,&part.vao);
                if(!part.vao)throw std::runtime_error("Could not allocate asteroid vertex array");
                gl_.BindVertexArray(part.vao);
                gl_.BindBuffer(array_buffer,geometry.vbo);
                for(unsigned attribute=0;attribute<5;++attribute){
                    const int components=attribute==2?2:attribute>=3?4:3;
                    const std::size_t offset=attribute>=3?32+(attribute-3)*16:attribute*12;
                    gl_.EnableVertexAttribArray(attribute);gl_.VertexAttribPointer(attribute,components,float_type,0,sizeof(Vertex),reinterpret_cast<const void*>(offset));
                }
                gl_.BindBuffer(element_buffer,geometry.ebo);gl_.BindBuffer(array_buffer,batch.instance_vbo);
                for(unsigned attribute=5;attribute<8;++attribute){
                    gl_.EnableVertexAttribArray(attribute);gl_.VertexAttribPointer(attribute,4,float_type,0,sizeof(Instance),reinterpret_cast<const void*>(std::size_t(attribute-5)*16));
                    gl_.VertexAttribDivisor(attribute,1);
                }
            }
        }
        // Exercise the exact instanced pipeline, material and maximum batch
        // sizes while the owner is black. A normal non-instanced warmup would
        // leave this Vulkan pipeline and its lazy uploads cold.
        const Matrix outside_clip{0,0,0,0,0,0,0,0,0,0,0,0,2,2,2,1};
        gl_.UseProgram(program_);gl_.ActiveTexture(0x84c0);gl_.Enable(0xb71);gl_.DepthMask(1);gl_.DepthFunc(0x201);
        gl_.Disable(0xbe2);gl_.Disable(0xb44);gl_.Disable(0xb90);gl_.Disable(0xc11);
        gl_.UniformMatrix4fv(vp_,1,0,outside_clip.data());gl_.Uniform1f(time_,0);
        for(const auto& batch:batches_)draw_batch(batch);
        gl_.BindVertexArray(0);gl_.UseProgram(0);stats_={};
    }catch(...){
        if(vertex_shader)gl_.DeleteShader(vertex_shader);
        if(fragment_shader)gl_.DeleteShader(fragment_shader);
        close();throw;
    }
}

void AsteroidRenderer::draw_batch(const Batch& batch){
    for(const auto& part:batch.parts){
        const auto& geometry=geometry_[part.geometry];
        gl_.Uniform1i(alpha_mode_,int(geometry.alpha));gl_.Uniform1f(alpha_cutoff_,geometry.alpha_cutoff);
        gl_.Uniform1i(atlas_mode_,int(geometry.atlas));gl_.Uniform1i(wrap_s_,int(geometry.wrap_s));gl_.Uniform1i(wrap_t_,int(geometry.wrap_t));
        gl_.BindTexture(texture_2d,geometry.texture);gl_.BindVertexArray(part.vao);
        gl_.DrawElementsInstanced(4,geometry.count,0x1405,nullptr,batch.instances);
    }
}
void AsteroidRenderer::draw(const Matrix& view_projection,float presentation_seconds,
        const std::uint8_t* alive,std::size_t count,std::uint64_t revision,const double* destroyed_seconds){
    stats_={};if(!initialized())return;
    if((alive&&count!=asteroid_count_)||(!alive&&(count||destroyed_seconds)))
        throw std::invalid_argument("Asteroid alive mask does not match the static layout");
    for(float value:view_projection)if(!std::isfinite(value))return;
    if(!std::isfinite(presentation_seconds))return;
    gl_.ActiveTexture(0x84c1);gl_.BindTexture(texture_2d,alive_texture_);
    if((alive&&(!custom_mask_||revision!=mask_revision_||timed_mask_!=(destroyed_seconds!=nullptr)))||(!alive&&custom_mask_)){
        for(std::size_t i=0;i<asteroid_count_;++i){
            const bool living=!alive||alive[i];const double death=destroyed_seconds&&!living?destroyed_seconds[i]:0;
            if(!std::isfinite(death)||death<0||death>std::numeric_limits<float>::max())
                throw std::invalid_argument("Invalid continuous asteroid destruction time");
            mask_pixels_[i*4]=living?1.f:0.f;mask_pixels_[i*4+1]=float(death);
        }
        gl_.TexSubImage2D(texture_2d,0,0,0,256,mask_height_,0x1908,0x1406,mask_pixels_.data());
        custom_mask_=alive!=nullptr;timed_mask_=destroyed_seconds!=nullptr;mask_revision_=revision;
    }
    gl_.UseProgram(program_);gl_.ActiveTexture(0x84c0);gl_.Enable(0xb71);gl_.DepthMask(1);gl_.DepthFunc(0x201);
    gl_.Disable(0xbe2);gl_.Disable(0xb44);gl_.Disable(0xb90);gl_.Disable(0xc11);
    gl_.UniformMatrix4fv(vp_,1,0,view_projection.data());gl_.Uniform1f(time_,presentation_seconds);
    for(const auto& batch:batches_){
        if(!render_math::sphere_visible(view_projection,batch.center,batch.radius)){++stats_.culled_batches;continue;}
        draw_batch(batch);stats_.instances+=unsigned(batch.instances);
        stats_.draws+=unsigned(batch.parts.size());
        for(const auto& part:batch.parts)stats_.triangles+=std::uint64_t(geometry_[part.geometry].count/3)*unsigned(batch.instances);
    }
    gl_.BindVertexArray(0);gl_.UseProgram(0);
}
void AsteroidRenderer::close()noexcept{
    // Continue releasing the remaining handles even if a diagnostic facade
    // reports an error during teardown. This also covers partial initialize.
    const auto release=[](auto&& operation)noexcept{try{operation();}catch(...){}};
    for(auto& batch:batches_){
        for(auto& part:batch.parts)if(part.vao)release([&]{gl_.DeleteVertexArrays(1,&part.vao);});
        if(batch.instance_vbo)release([&]{gl_.DeleteBuffers(1,&batch.instance_vbo);});
    }
    for(auto& geometry:geometry_){
        if(geometry.ebo)release([&]{gl_.DeleteBuffers(1,&geometry.ebo);});
        if(geometry.vbo)release([&]{gl_.DeleteBuffers(1,&geometry.vbo);});
    }
    for(auto texture:textures_)if(texture)release([&]{gl_.DeleteTextures(1,&texture);});
    if(alive_texture_)release([&]{gl_.DeleteTextures(1,&alive_texture_);});
    if(program_)release([&]{gl_.DeleteProgram(program_);});
    program_=alive_texture_=0;asteroid_count_=0;mask_height_=0;mask_pixels_.clear();custom_mask_=timed_mask_=false;mask_revision_=0;
    batches_.clear();geometry_.clear();textures_.clear();stats_={};
}
}
