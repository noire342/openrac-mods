// SPDX-License-Identifier: GPL-3.0-or-later
#include "renderer.hpp"
#include "render_math.hpp"
#include "flight_presentation.hpp"
#include "health_meter.hpp"
#include "reticle.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace spacevoyages {
namespace {
using Matrix=std::array<float,16>;
constexpr Matrix identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
Matrix multiply(const Matrix&a,const Matrix&b){Matrix r{};for(int c=0;c<4;++c)for(int y=0;y<4;++y)for(int k=0;k<4;++k)r[c*4+y]+=a[k*4+y]*b[c*4+k];return r;}
using render_math::model;
using render_math::view;
Matrix perspective(float fov,float aspect){const float t=1/std::tan(fov*.5f),n=.08f,f=3000;return {t/aspect,0,0,0,0,t,0,0,0,0,(f+n)/(n-f),-1,0,0,2*f*n/(n-f),0};}
Matrix orthographic(float width){return {2/width,0,0,0,0,-2/416.f,0,0,0,0,-1,0,-1,1,0,1};}
Vertex vertex(Vec3 p,float u=0,float v=0){return {{p.x,p.y,p.z},{0,0,1},{u,v}};}
}
unsigned SceneRenderer::shader(unsigned kind,const char* source){const auto result=gl_.CreateShader(kind);gl_.ShaderSource(result,1,&source,nullptr);gl_.CompileShader(result);int status=0;gl_.GetShaderiv(result,0x8b81,&status);if(!status){char error[2048]{};gl_.GetShaderInfoLog(result,sizeof error,nullptr,error);gl_.DeleteShader(result);throw std::runtime_error(std::string("Space shader: ")+error);}return result;}
unsigned SceneRenderer::texture(const unsigned char* data,int width,int height){unsigned result;gl_.GenTextures(1,&result);gl_.BindTexture(0xde1,result);gl_.TexParameteri(0xde1,0x2801,0x2601);gl_.TexParameteri(0xde1,0x2800,0x2601);gl_.TexParameteri(0xde1,0x2802,0x812f);gl_.TexParameteri(0xde1,0x2803,0x812f);gl_.TexImage2D(0xde1,0,0x8058,width,height,0,0x1908,0x1401,data);return result;}
void SceneRenderer::initialize(GlApi::GetProc loader,const AsteroidLayout* asteroids,bool engine_flames){
 if(program_)return;gl_.load(loader);
 // Native subtractive effects can leave a different equation in the shared
 // facade. Blend factors alone do not restore ordinary alpha compositing.
 gl_.BlendEquation(0x8006); // GL_FUNC_ADD, including pipeline prewarming.
 const char* vs="#version 330 core\nlayout(location=0)in vec3 position;layout(location=1)in vec3 normal;layout(location=2)in vec2 uv;layout(location=3)in vec4 vertex_color;layout(location=4)in vec4 tile_rect;uniform mat4 mvp;uniform mat4 model;uniform mat4 camera_view;out vec3 n;out vec2 t;out vec4 vcolor;out vec4 rect;out float centre_depth;void main(){gl_Position=mvp*vec4(position,1);n=mat3(model)*normal;t=uv;vcolor=vertex_color;rect=tile_rect;centre_depth=-(camera_view*model*vec4(0,0,0,1)).z;}";
 const char* fs=R"GLSL(#version 330 core
in vec3 n;in vec2 t;in vec4 vcolor;in vec4 rect;in float centre_depth;
uniform sampler2D image;uniform sampler2D health_palette;
uniform vec4 tint;uniform float ambient;uniform int alpha_mode;
uniform float alpha_cutoff;uniform int atlas_mode;uniform int wrap_s;uniform int wrap_t;
uniform int health_mode;uniform int health_threshold;
uniform mat4 camera_view;uniform int sphere_uv;
out vec4 color;
float tile(float u,int w){if(w==10497)return fract(u);if(w==33648)return 1-abs(mod(u,2)-1);return clamp(u,0,1);}
vec4 health_texel(ivec2 p){
 ivec2 size=textureSize(image,0);
 int index=int(texelFetch(image,clamp(p,ivec2(0),size-1),0).r*255.0+.5);
 if(index>=2&&index>=health_threshold)return vec4(0,0,0,1);
 return texelFetch(health_palette,ivec2(index,0),0);
}
vec4 read_image(vec2 st){
 if(health_mode==0)return texture(image,st);
 vec2 p=st*vec2(textureSize(image,0))-.5;
 ivec2 corner=ivec2(floor(p));vec2 f=fract(p);
 return mix(mix(health_texel(corner),health_texel(corner+ivec2(1,0)),f.x),
            mix(health_texel(corner+ivec2(0,1)),health_texel(corner+ivec2(1,1)),f.x),f.y);
}
void main(){
 vec2 st=atlas_mode!=0?rect.zw+vec2(tile(t.x,wrap_s),tile(t.y,wrap_t))*rect.xy:t;
 vec4 p;
 if(sphere_uv!=0){
  vec3 e=normalize(mat3(camera_view)*normalize(n));
  st=vec2(e.x*.5+.5,.5-e.y*.5);
  // Native metal gate in game/world units (mesh.vert182-192). The model
  // origin here is the bake's recentered Space pivot, not the old Moby pivot.
  float shine=clamp((24.0-centre_depth)/16.0,0.0,1.0);
  p=read_image(st)*tint*vec4(vec3(112.0/128.0),shine);
 }else p=read_image(st)*tint*vcolor;
 if(alpha_mode!=0&&p.a<(alpha_mode==1?alpha_cutoff:.015))discard;
 if(alpha_mode==0)p.a=1;
 float d=max(dot(normalize(n),normalize(vec3(-.4,-.3,.85))),0);
 color=vec4(p.rgb*(sphere_uv!=0?1.0:ambient+(1-ambient)*d),p.a);
})GLSL";
 unsigned a=shader(0x8b31,vs),b=0;try{b=shader(0x8b30,fs);}catch(...){gl_.DeleteShader(a);throw;}
 program_=gl_.CreateProgram();gl_.AttachShader(program_,a);gl_.AttachShader(program_,b);gl_.LinkProgram(program_);gl_.DeleteShader(a);gl_.DeleteShader(b);
 int status=0;gl_.GetProgramiv(program_,0x8b82,&status);if(!status){char error[2048]{};gl_.GetProgramInfoLog(program_,sizeof error,nullptr,error);throw std::runtime_error(std::string("Space program: ")+error);}
 mvp_=gl_.GetUniformLocation(program_,"mvp");model_=gl_.GetUniformLocation(program_,"model");tint_=gl_.GetUniformLocation(program_,"tint");ambient_=gl_.GetUniformLocation(program_,"ambient");alpha_mode_=gl_.GetUniformLocation(program_,"alpha_mode");alpha_cutoff_=gl_.GetUniformLocation(program_,"alpha_cutoff");
 atlas_mode_=gl_.GetUniformLocation(program_,"atlas_mode");wrap_s_=gl_.GetUniformLocation(program_,"wrap_s");wrap_t_=gl_.GetUniformLocation(program_,"wrap_t");
 health_mode_=gl_.GetUniformLocation(program_,"health_mode");health_threshold_=gl_.GetUniformLocation(program_,"health_threshold");
 view_=gl_.GetUniformLocation(program_,"camera_view");sphere_uv_=gl_.GetUniformLocation(program_,"sphere_uv");
 gl_.UseProgram(program_);gl_.Uniform1i(gl_.GetUniformLocation(program_,"image"),0);
 gl_.Uniform1i(gl_.GetUniformLocation(program_,"health_palette"),1);gl_.Uniform1i(health_mode_,0);
 gl_.UniformMatrix4fv(view_,1,0,identity.data());gl_.Uniform1i(sphere_uv_,0);
 const auto attributes=[&]{for(unsigned i=0;i<5;++i){gl_.EnableVertexAttribArray(i);gl_.VertexAttribPointer(i,i==2?2:i>=3?4:3,0x1406,0,sizeof(Vertex),reinterpret_cast<void*>(i>=3?32+(i-3)*16:std::size_t(i)*12));}};
 gl_.GenVertexArrays(1,&ui_vao_);gl_.BindVertexArray(ui_vao_);gl_.GenBuffers(1,&ui_vbo_);gl_.BindBuffer(0x8892,ui_vbo_);gl_.BufferData(0x8892,6*sizeof(Vertex),nullptr,0x88e0);attributes();
 const unsigned char white[4]={255,255,255,255};white_=texture(white,1,1);
 gl_.ActiveTexture(0x84c1);gl_.BindTexture(0xde1,white_);gl_.ActiveTexture(0x84c0);
 auto font=assets_.font_rgba;for(std::size_t i=0;i<font.size();++i)if(i%4!=3)font[i]=static_cast<unsigned char>(std::min(255,int(font[i])*2));
 font_=texture(font.data(),assets_.font_width,assets_.font_height);sky_=texture(assets_.sky_rgba.data(),2048,1024);
 radar_=texture(assets_.radar_rgba.data(),128,128);
 for(std::size_t i=0;i<reticle_.size();++i){const auto& sprite=assets_.reticle[i];if(!sprite.rgba.empty()){
  auto rgba=sprite.rgba;for(std::size_t alpha_index=3;alpha_index<rgba.size();alpha_index+=4)rgba[alpha_index]=static_cast<unsigned char>(std::min(255,(int(rgba[alpha_index])*255+64)/128));
  reticle_[i]=texture(rgba.data(),sprite.width,sprite.height);
 }}
 const auto& health=assets_.health_meter;
 if(!health.indices.empty()){
  std::vector<unsigned char> pixels(health.indices.size()*4,255);
  for(std::size_t i=0;i<health.indices.size();++i)pixels[i*4]=health.indices[i];
  health_indices_=texture(pixels.data(),health.width,health.height);
  health_palette_=texture(health.palette_rgba.data(),256,1);
 }
 std::unordered_map<std::string,unsigned> texture_cache;
 for(const auto& src:assets_.meshes){
  // The instanced pass owns these shared meshes and textures. Do not upload
  // a second non-instanced copy or warm it through the ordinary scene pass.
  if(src.id.find("asteroid_")==0||src.id==assets_.projectile_effects.bolt_asset_id)continue;
  GpuMesh dst;dst.id=src.id;dst.count=int(src.indices.size());dst.alpha=src.alpha;dst.alpha_cutoff=src.alpha_cutoff;dst.atlas=src.atlas;dst.sphere_uv=src.sphere_uv;dst.metal_overlay=src.metal_overlay;dst.wrap_s=src.wrap_s;dst.wrap_t=src.wrap_t;
  Vec3 lo{1e10f,1e10f,1e10f},hi{-1e10f,-1e10f,-1e10f};for(const auto& v:src.vertices){lo.x=std::min(lo.x,v.position[0]);lo.y=std::min(lo.y,v.position[1]);lo.z=std::min(lo.z,v.position[2]);hi.x=std::max(hi.x,v.position[0]);hi.y=std::max(hi.y,v.position[1]);hi.z=std::max(hi.z,v.position[2]);}dst.center=(lo+hi)*.5f;
  gl_.GenVertexArrays(1,&dst.vao);gl_.BindVertexArray(dst.vao);gl_.GenBuffers(1,&dst.vbo);gl_.BindBuffer(0x8892,dst.vbo);gl_.BufferData(0x8892,std::ptrdiff_t(src.vertices.size()*sizeof(Vertex)),src.vertices.data(),0x88e4);attributes();gl_.GenBuffers(1,&dst.ebo);gl_.BindBuffer(0x8893,dst.ebo);gl_.BufferData(0x8893,std::ptrdiff_t(src.indices.size()*4),src.indices.data(),0x88e4);
  const auto sampler_s=src.atlas?33071:src.wrap_s,sampler_t=src.atlas?33071:src.wrap_t;
  const auto key=src.texture_key+":"+std::to_string(src.width)+":"+std::to_string(src.height)+":"+std::to_string(sampler_s)+":"+std::to_string(sampler_t);
  const auto cached=texture_cache.find(key);if(cached!=texture_cache.end())dst.texture=cached->second;
  else{dst.texture=texture(src.rgba.data(),src.width,src.height);gl_.TexParameteri(0xde1,0x2802,int(sampler_s));gl_.TexParameteri(0xde1,0x2803,int(sampler_t));texture_cache.emplace(key,dst.texture);mesh_textures_.push_back(dst.texture);}
  const auto part=src.id.find("_part");auto& batch=groups_[src.id.substr(0,part)];
  (src.metal_overlay?batch.overlays:src.alpha==AlphaMode::Blend?batch.blend:batch.opaque).push_back(meshes_.size());meshes_.push_back(dst);
 }
 // Large static GL uploads are made GPU-resident lazily by the Vulkan facade's
 // first draw. Touch every mesh during the black entry instead of paying
 // that allocation/upload when an unseen destination first enters view.
 // Every vertex maps to (2,2,2,1), wholly outside the clip volume: no pixel
 // or depth value is written, and the original geometry stays unchanged.
 const Matrix outside_clip{0,0,0,0,0,0,0,0,0,0,0,0,2,2,2,1};
 gl_.UseProgram(program_);gl_.ActiveTexture(0x84c0);
 gl_.BlendFuncSeparate(0x302,0x303,1,0x303);
 gl_.Disable(0xb44);gl_.Disable(0xb90);gl_.Enable(0xb71);
 gl_.DepthFunc(0x201);gl_.BlendFuncSeparate(0x302,0x303,1,0x303);
 for(const auto& item:meshes_)mesh(item,outside_clip,identity,1.f);
 // Draw records the normal upload work; submission happens with the first
 // native scene frame. No blocking flush or finish is needed here. This
 // mitigates first-use stalls, not periodic pacing or controller motion.
 gl_.DepthMask(1);gl_.BindVertexArray(0);
 if(asteroids)asteroids_.initialize(loader,assets_,*asteroids);
 if(engine_flames)ship_effects_.initialize(loader,assets_.ship_effects);
 else{auto effects=assets_.ship_effects;effects.flame={};effects.ships={};ship_effects_.initialize(loader,effects);}
 const MeshAsset* fragment=nullptr;for(const auto& mesh:assets_.meshes)if(mesh.id==assets_.projectile_effects.fragment_asset_id)fragment=&mesh;
 for(const auto& mesh:assets_.meshes)if(mesh.id==assets_.projectile_effects.bolt_asset_id){projectiles_.initialize(loader,mesh,assets_.projectile_effects,fragment);break;}
}
const SceneRenderer::GpuMesh& SceneRenderer::get(const std::string& id)const{for(const auto& m:meshes_)if(m.id==id)return m;throw std::runtime_error("GPU mesh missing: "+id);}
void SceneRenderer::mesh(const GpuMesh& mesh,const Matrix& vp,const Matrix& transform,float ambient){++stats_.draws;stats_.triangles+=unsigned(mesh.count)/3;if(mesh.id.find("station_")==0||mesh.id.find("fleet_")==0)++stats_.artificial_draws;const auto mvp=multiply(vp,transform);gl_.UniformMatrix4fv(mvp_,1,0,mvp.data());gl_.UniformMatrix4fv(model_,1,0,transform.data());const float tint[4]={1,1,1,1};gl_.Uniform4fv(tint_,1,tint);gl_.Uniform1f(ambient_,ambient);gl_.Uniform1i(alpha_mode_,int(mesh.alpha));gl_.Uniform1f(alpha_cutoff_,mesh.alpha_cutoff);gl_.Uniform1i(atlas_mode_,int(mesh.atlas));gl_.Uniform1i(sphere_uv_,int(mesh.sphere_uv));gl_.Uniform1i(wrap_s_,int(mesh.wrap_s));gl_.Uniform1i(wrap_t_,int(mesh.wrap_t));if(mesh.alpha==AlphaMode::Blend){gl_.Enable(0xbe2);gl_.DepthMask(0);}else{gl_.Disable(0xbe2);gl_.DepthMask(1);}// A native metal overlay shares exact positions with its opaque base.
 // Accept equal depth for that pass only; other transparent materials retain
 // ordinary LESS. This uses standard depth, not the native reversed-Z bias.
 struct RestoreDepth {GlApi& gl;bool active;~RestoreDepth(){if(active)gl.DepthFunc(0x201);}} restore{gl_,mesh.metal_overlay};
 if(mesh.metal_overlay)gl_.DepthFunc(0x203);
 gl_.BindTexture(0xde1,mesh.texture);gl_.BindVertexArray(mesh.vao);gl_.DrawElements(4,mesh.count,0x1405,nullptr);
}
void SceneRenderer::group(const std::string& id,const Matrix& vp,const Matrix& transform,float ambient,Vec3 eye){
 const auto found=groups_.find(id);if(found==groups_.end())throw std::runtime_error("GPU group missing: "+id);
 for(auto i:found->second.opaque)mesh(meshes_[i],vp,transform,ambient);
 // Surface overlays belong behind the later glass pass, regardless of mesh
 // centroid sorting. Opaque depth already provides visibility between actors.
 for(auto i:found->second.overlays)mesh(meshes_[i],vp,transform,ambient);
 auto blend=found->second.blend;
 const auto depth=[&](std::size_t i){const auto p=meshes_[i].center;const Vec3 world{transform[0]*p.x+transform[4]*p.y+transform[8]*p.z+transform[12],transform[1]*p.x+transform[5]*p.y+transform[9]*p.z+transform[13],transform[2]*p.x+transform[6]*p.y+transform[10]*p.z+transform[14]};return length_squared(world-eye);};
 std::sort(blend.begin(),blend.end(),[&](auto a,auto b){return depth(a)>depth(b);});for(auto i:blend)mesh(meshes_[i],vp,transform,ambient);
}
void SceneRenderer::triangles(const std::vector<Vertex>& vertices,unsigned texture,const std::array<float,4>& color){++stats_.draws;stats_.triangles+=unsigned(vertices.size()/3);const auto projection=orthographic(ui_width_);gl_.UniformMatrix4fv(mvp_,1,0,projection.data());gl_.UniformMatrix4fv(model_,1,0,identity.data());gl_.Uniform4fv(tint_,1,color.data());gl_.Uniform1f(ambient_,1);gl_.Uniform1i(alpha_mode_,2);gl_.Uniform1f(alpha_cutoff_,.015f);gl_.Uniform1i(atlas_mode_,0);gl_.Uniform1i(sphere_uv_,0);gl_.Enable(0xbe2);gl_.BindTexture(0xde1,texture?texture:white_);gl_.BindVertexArray(ui_vao_);gl_.BindBuffer(0x8892,ui_vbo_);gl_.BufferData(0x8892,std::ptrdiff_t(vertices.size()*sizeof(Vertex)),vertices.data(),0x88e0);gl_.DrawArrays(4,0,int(vertices.size()));}
void SceneRenderer::quad(float x,float y,float w,float h,const std::array<float,4>& c,unsigned tex,float u,float v,float uw,float vh){triangles({vertex({x,y,0},u,v),vertex({x+w,y,0},u+uw,v),vertex({x+w,y+h,0},u+uw,v+vh),vertex({x,y,0},u,v),vertex({x+w,y+h,0},u+uw,v+vh),vertex({x,y+h,0},u,v+vh)},tex,c);}
float SceneRenderer::text_width(const std::string& s,float pixels)const{float width=0;for(unsigned char c:s)if(c<128)width+=assets_.glyphs[c].advance*pixels/16;return width;}
void SceneRenderer::text(float x,float y,const std::string& s,float pixels,const std::array<float,4>& color){for(unsigned char c:s){if(c>=128)continue;const auto& g=assets_.glyphs[c];const float glyph_width=(c>=0x10&&c<=0x1f)?24.f:16.f;if(c!=' ')quad(x,y+g.top*pixels/16,glyph_width*pixels/16,pixels,{0,0,0,color[3]*.8f},font_,g.u/float(assets_.font_width),g.v/float(assets_.font_height),glyph_width/assets_.font_width,16.f/assets_.font_height);if(c!=' ')quad(x-1,y-1+g.top*pixels/16,glyph_width*pixels/16,pixels,color,font_,g.u/float(assets_.font_width),g.v/float(assets_.font_height),glyph_width/assets_.font_width,16.f/assets_.font_height);x+=g.advance*pixels/16;}}
void SceneRenderer::circle(float x,float y,float radius,const std::array<float,4>& color,bool fill){std::vector<Vertex> vertices;for(int i=0;i<64;++i){float a=i*2*pi/64,b=(i+1)*2*pi/64;Vec3 p{x+std::cos(a)*radius,y+std::sin(a)*radius,0},q{x+std::cos(b)*radius,y+std::sin(b)*radius,0};if(fill){vertices.push_back(vertex({x,y,0}));vertices.push_back(vertex(p));vertices.push_back(vertex(q));}else{Vec3 p2{x+std::cos(a)*(radius-1),y+std::sin(a)*(radius-1),0},q2{x+std::cos(b)*(radius-1),y+std::sin(b)*(radius-1),0};for(auto point:{p,q,q2,p,q2,p2})vertices.push_back(vertex(point));}}triangles(vertices,0,color);}
void SceneRenderer::draw(const VoyageController& voyage,const FlightState& f,int width,int height,int ship,bool paused,float entry_progress,float approach_progress,float scene_seconds,const ShipDamageState* damage){
 const auto selected=ship_identity_from_native_class(ship);
 if(!selected){stats_={};return;}
 draw(voyage,f,width,height,*selected,paused,entry_progress,approach_progress,scene_seconds,damage);
}
bool SceneRenderer::supports_ship(ShipIdentity selected)const noexcept {
 const auto* profile=ship_profile(selected);
 if(!profile||(selected==ShipIdentity::Original&&!assets_.original_ship.available))return false;
 if(program_)return groups_.find(profile->asset_id)!=groups_.end();
 const auto length=std::char_traits<char>::length(profile->asset_id);
 for(const auto& mesh:assets_.meshes)if(mesh.id==profile->asset_id||
     (mesh.id.compare(0,length,profile->asset_id)==0&&mesh.id.compare(length,5,"_part")==0))return true;
 return false;
}
void SceneRenderer::draw(const VoyageController& voyage,const FlightState& f,int width,int height,ShipIdentity ship,bool paused,float entry_progress,float approach_progress,float scene_seconds,const ShipDamageState* damage){
 stats_={};
 if(!program_||width<1||height<1)return;
 const auto* profile=ship_profile(ship);if(!profile)return;
 if(!supports_ship(ship))throw std::runtime_error("Selected ship appearance is unavailable");
 const bool entry=voyage.phase()==Phase::Departure;
 const auto& hull=damage?*damage:voyage.ship_damage();
 const auto c=entry?presentation::space_entry_camera(f,entry_progress):f.camera;
 gl_.UseProgram(program_);gl_.ActiveTexture(0x84c0);gl_.Disable(0xb44);gl_.Disable(0xb90);gl_.Disable(0xc11);gl_.Enable(0xbe2);gl_.BlendEquation(0x8006);gl_.BlendFuncSeparate(0x302,0x303,1,0x303);gl_.Enable(0xb71);gl_.DepthMask(1);gl_.DepthFunc(0x201);
 const auto camera_view=view(c.position,c.yaw,c.pitch,c.roll);
 gl_.UniformMatrix4fv(view_,1,0,camera_view.data());
 const auto vp=multiply(perspective(c.fov,float(width)/height),camera_view);
 gl_.Disable(0xb71);gl_.DepthMask(0);auto sky=get("planet_novalis");sky.texture=sky_;mesh(sky,vp,model(c.position,0,0,0,1500),1);gl_.Enable(0xb71);gl_.DepthMask(1);
 const auto* sites=voyage.catalog_data();const auto site_count=voyage.catalog_count();
 for(std::size_t i=0;i<site_count;++i){const auto& body=sites[i];if(render_math::sphere_visible(vp,body.position,body.radius))group(body.asset_id,vp,model(body.position,0,0,0,body.radius),body.kind==DestinationKind::Planet?.86f:1.f,c.position);}
 const std::string ship_id=profile->asset_id;
 const auto ship_model=model(f.position,f.model_yaw,f.model_pitch,f.model_roll,1);
 if(!hull.destroyed||assets_.ship_effects.explosion.rgba.empty()||hull.destruction_progress<.08f)group(ship_id,vp,ship_model,.85f,c.position);
 const auto& combat=voyage.combat();const auto& alive=combat.alive_mask();
 const auto& disappeared=combat.destroyed_seconds();
 asteroids_.draw(vp,scene_seconds,alive.empty()?nullptr:alive.data(),alive.size(),combat.revision(),disappeared.empty()?nullptr:disappeared.data());
 const auto& rocks=asteroids_.last_stats();stats_.asteroid_draws=rocks.draws;stats_.asteroid_instances=rocks.instances;stats_.asteroid_culled_batches=rocks.culled_batches;
 stats_.draws+=rocks.draws;stats_.triangles+=unsigned(rocks.triangles);
 projectiles_.draw(vp,c,combat,ship,scene_seconds);
 const auto& shots=projectiles_.last_stats();stats_.projectile_draws=shots.draws;stats_.projectiles=shots.projectiles;stats_.impact_sprites=shots.effects;
 stats_.draws+=shots.draws;stats_.triangles+=shots.triangles;
 ship_effects_.draw(vp,ship_model,c,ship,f.boost_fraction,scene_seconds,hull.hit_flash,hull.destroyed?hull.destruction_progress:-1.f);
 const auto& effects=ship_effects_.last_stats();stats_.exhaust_draws=effects.draws;stats_.thrusters=effects.thrusters;stats_.effect_sprites=effects.sprites;
 stats_.draws+=effects.draws;stats_.triangles+=effects.triangles;
 // The independent instanced pass owns its program/VAO/depth state. Restore
 // this program before the existing radar, font and final fade are drawn.
 gl_.UseProgram(program_);gl_.ActiveTexture(0x84c0);
 gl_.BlendEquation(0x8006);
 gl_.BlendFuncSeparate(0x302,0x303,1,0x303);
 gl_.Disable(0xb71);gl_.DepthMask(0);ui_width_=416.f*width/height;
 // Native-style radar: the same circular layout, with destination contacts in
 // place of combat targets. The original health arc shares the same quad.
 const float hud_alpha=entry?presentation::entry_hud(entry_progress):1.f;
 if(combat.state().armed&&!hull.destroyed&&voyage.phase()==Phase::FreeFlight&&reticle_[0]&&reticle_[1]&&reticle_[2]){
  const auto aim=weapon_reticle(vp,f,combat.assets(),ship,ui_width_);
  if(aim.visible)for(std::size_t i=0;i<reticle_.size();++i){const auto& sprite=assets_.reticle[i];const float size=i==2?8.f:32.f;
   quad(aim.x-size*.5f,aim.y-size*.5f,size,size,{.25f,255.f/128.f,.25f,hud_alpha},reticle_[i],
       1.f/sprite.width,1.f/sprite.height,1.f-2.f/sprite.width,1.f-2.f/sprite.height);
  }
 }
 const float rx=ui_width_-62,ry=350,r=40;
 if(voyage.asteroid_damage_enabled()&&health_indices_){
  gl_.ActiveTexture(0x84c1);gl_.BindTexture(0xde1,health_palette_);gl_.ActiveTexture(0x84c0);
  gl_.Uniform1i(health_mode_,1);gl_.Uniform1i(health_threshold_,health_meter::palette_threshold(hull.hud_hull,float(hull.max_hull)));
  quad(rx-51,ry-51,102,102,{1,1,1,.875f*hud_alpha},health_indices_);
  gl_.Uniform1i(health_mode_,0);
 }
 quad(rx-51,ry-51,102,102,{1,1,1,.875f*hud_alpha},radar_);
 for(std::size_t i=0;i<site_count;++i){const auto& d=sites[i];Vec3 delta=d.position-f.position;const float forward=std::cos(f.yaw)*delta.x+std::sin(f.yaw)*delta.y,right=std::sin(f.yaw)*delta.x-std::cos(f.yaw)*delta.y;float dx=right*.075f,dy=-forward*.075f;const float mag=std::sqrt(dx*dx+dy*dy);if(mag>r-7){dx*=((r-7)/mag);dy*=((r-7)/mag);}const bool selected=voyage.candidate()==&d;const bool open=voyage.destination_unlocked(d.level);const std::array<float,4> contact=open?std::array<float,4>{selected?.8f:.5f,selected?.9f:.7f,1,hud_alpha}:std::array<float,4>{selected?.7f:.48f,selected?.71f:.49f,selected?.73f:.51f,.85f*hud_alpha};circle(rx+dx,ry+dy,site_count>3?(selected?2.7f:2.1f):3.f,contact,true);}
 if(voyage.phase()==Phase::FreeFlight)if(const auto* candidate=voyage.candidate()){
  const bool open=voyage.candidate_unlocked();const std::string label=open?std::string(1,char(0x12))+" Entra - "+candidate->name:"Coordinate sconosciute";
  const float text_w=text_width(label,16),x=(ui_width_-text_w)/2,y=372;
  stats_.prompt_candidate=candidate->level;stats_.font_texture=font_;
  stats_.prompt_x=x;stats_.prompt_y=y;stats_.prompt_width=text_w;stats_.prompt_alpha=1;
  for(unsigned char glyph:label)if(glyph<128&&glyph!=' ')++stats_.prompt_glyphs;
  text(x,y,label,16);
 }
 const float black=hull.destroyed?health_meter::destruction_black(hull.destruction_progress):entry?presentation::entry_black(entry_progress):
  (voyage.phase()==Phase::Approach||voyage.phase()==Phase::Arrival)?presentation::approach_black(approach_progress):0.f;
 if(black>0)quad(0,0,ui_width_,416,{0,0,0,black});
 if(paused){quad(0,0,ui_width_,416,{0,0,0,.48f});const std::string label="Pausa";text((ui_width_-text_width(label,22))/2,190,label,22);text((ui_width_-text_width("START Riprendi",14))/2,220,"START Riprendi",14);}
 gl_.DepthMask(1);gl_.BindVertexArray(0);gl_.UseProgram(0);
}
void SceneRenderer::close(){projectiles_.close();ship_effects_.close();asteroids_.close();if(!program_)return;for(auto& m:meshes_){gl_.DeleteBuffers(1,&m.ebo);gl_.DeleteBuffers(1,&m.vbo);gl_.DeleteVertexArrays(1,&m.vao);}for(auto tex:mesh_textures_)gl_.DeleteTextures(1,&tex);mesh_textures_.clear();meshes_.clear();groups_.clear();gl_.DeleteTextures(1,&font_);gl_.DeleteTextures(1,&white_);gl_.DeleteTextures(1,&sky_);gl_.DeleteTextures(1,&radar_);gl_.DeleteTextures(1,&health_indices_);gl_.DeleteTextures(1,&health_palette_);for(auto id:reticle_)if(id)gl_.DeleteTextures(1,&id);reticle_={};gl_.DeleteBuffers(1,&ui_vbo_);gl_.DeleteVertexArrays(1,&ui_vao_);gl_.DeleteProgram(program_);program_=ui_vao_=ui_vbo_=font_=white_=sky_=radar_=health_indices_=health_palette_=0;}
}
