// SPDX-License-Identifier: GPL-3.0-or-later
#include "assets.hpp"
#include "catalog.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <charconv>
namespace spacevoyages {
namespace {
int integer(const std::string& token){int value{};const auto parsed=std::from_chars(token.data(),token.data()+token.size(),value);if(parsed.ec!=std::errc{}||parsed.ptr!=token.data()+token.size())throw std::runtime_error("Invalid scene material integer");return value;}
bool valid_wrap(int mode){return mode==10497||mode==33071||mode==33648;}
bool sha256_hex(const std::string& s){return s.size()==64&&std::all_of(s.begin(),s.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});}
std::vector<unsigned char> bytes(const std::filesystem::path& root,const std::string& relative,std::size_t limit){
 const std::filesystem::path path(relative);
 if(path.is_absolute()||path.has_root_name()||path.has_root_directory()||relative.find(':')!=std::string::npos||relative.find("..")!=std::string::npos)throw std::runtime_error("Invalid scene asset path");
 std::ifstream in(root/path,std::ios::binary|std::ios::ate);
 if(!in)throw std::runtime_error("Missing scene asset: "+relative);
 const auto size=in.tellg();if(size<=0||static_cast<unsigned long long>(size)>limit)throw std::runtime_error("Invalid scene asset size");
 std::vector<unsigned char> out(static_cast<std::size_t>(size));in.seekg(0);in.read(reinterpret_cast<char*>(out.data()),size);
 if(!in)throw std::runtime_error("Incomplete scene asset: "+relative);return out;
}
std::vector<unsigned char> effect_payload(const std::filesystem::path& root,const std::string& path,std::size_t expected,std::size_t& used,std::size_t budget){
 if(expected>budget-used)throw std::runtime_error("Scene effects exceed the resource budget");
 auto result=bytes(root,path,expected);if(result.size()!=expected)throw std::runtime_error("Invalid original effect payload size");
 used+=result.size();return result;
}
void complete_effect_record(std::istringstream& row){row>>std::ws;if(!row.eof()&&row.peek()!='#')throw std::runtime_error("Trailing scene-effect fields");}
EffectSprite effect_sprite(std::istringstream& row,const std::filesystem::path& root,std::size_t& used,std::size_t budget){
 std::string path;EffectSprite sprite;
 if(!(row>>path>>sprite.width>>sprite.height>>sprite.columns>>sprite.rows>>sprite.frames>>sprite.fps)||
    sprite.width<1||sprite.width>4096||sprite.height<1||sprite.height>4096||!sprite.columns||!sprite.rows||
    sprite.columns>unsigned(sprite.width)||sprite.rows>unsigned(sprite.height)||sprite.width%int(sprite.columns)||sprite.height%int(sprite.rows)||
    !sprite.frames||sprite.frames>sprite.columns*sprite.rows||!std::isfinite(sprite.fps)||sprite.fps<0||sprite.fps>1000)
  throw std::runtime_error("Invalid original effect sprite");
 complete_effect_record(row);sprite.rgba=effect_payload(root,path,std::size_t(sprite.width)*sprite.height*4,used,budget);return sprite;
}
void flight_effects(SceneAssets& out,const std::filesystem::path& root,std::size_t& memory_used,std::size_t budget){
 const auto file=root/"flight-effects.tsv";
 if(!std::filesystem::exists(file))return; // Older packs retain ordinary flight.
 std::ifstream manifest(file);if(!manifest)throw std::runtime_error("Cannot read flight-effects.tsv");
 bool format=false,health=false;bool sprites[3]{},colors[3]{};std::string line;
 const auto payload=[&](const std::string& path,std::size_t expected){
  if(expected>budget-memory_used)throw std::runtime_error("Scene effects exceed the resource budget");
  auto result=bytes(root,path,expected);if(result.size()!=expected)throw std::runtime_error("Invalid ship effect payload size");
  memory_used+=result.size();return result;
 };
 const auto complete=[](std::istringstream& row){row>>std::ws;if(!row.eof()&&row.peek()!='#')throw std::runtime_error("Trailing flight-effect fields");};
 const auto dimension=[](int n){return n>=1&&n<=4096;};
 while(std::getline(manifest,line)){
  std::istringstream row(line);std::string kind;if(!(row>>kind)||kind[0]=='#')continue;
  if(kind=="format"){
   int version{};if(format||!(row>>version)||version!=1)throw std::runtime_error("Unsupported flight-effect format");
   format=true;complete(row);continue;
  }
  if(!format)throw std::runtime_error("Ship effects require a versioned format header");
  if(kind=="health"){
   std::string indices,palette;auto& sprite=out.health_meter;
   if(health||!(row>>indices>>palette>>sprite.width>>sprite.height)||!dimension(sprite.width)||!dimension(sprite.height))throw std::runtime_error("Invalid native health meter");
   complete(row);sprite.indices=payload(indices,std::size_t(sprite.width)*sprite.height);sprite.palette_rgba=payload(palette,256*4);health=true;
  }else if(kind=="sprite"){
   std::string name;if(!(row>>name))throw std::runtime_error("Malformed original ship-effect sprite");
   const int index=name=="flame"?0:name=="hit"?1:name=="explosion"?2:-1;
   if(index<0||sprites[index])throw std::runtime_error("Unknown or duplicate original ship-effect sprite");
   auto sprite=effect_sprite(row,root,memory_used,budget);
   auto& destination=index==0?out.ship_effects.flame:index==1?out.ship_effects.hit:out.ship_effects.explosion;destination=std::move(sprite);sprites[index]=true;
  }else if(kind=="mount"){
   int id{};ExhaustMount mount;
   if(!(row>>id>>mount.position.x>>mount.position.y>>mount.position.z>>mount.direction.x>>mount.direction.y>>mount.direction.z>>mount.radius>>mount.length)||id<531||id>533||
      !finite(mount.position)||!finite(mount.direction)||length_squared(mount.position)>256.f||std::abs(length_squared(mount.direction)-1.f)>.002f||
      !std::isfinite(mount.radius)||mount.radius<=0||mount.radius>4||!std::isfinite(mount.length)||mount.length<=0||mount.length>64)throw std::runtime_error("Invalid native exhaust mount");
   complete(row);auto& mounts=out.ship_effects.ships[std::size_t(id-531)];
   if(mounts.count>=mounts.capacity)throw std::runtime_error("Too many ship exhaust mounts");
   for(std::size_t i=0;i<mounts.count;++i)if(length_squared(mount.position-mounts.mounts[i].position)<1e-10f)throw std::runtime_error("Duplicate ship exhaust mount");
   mounts.mounts[mounts.count++]=mount;
  }else if(kind=="color"){
   int id{};std::array<float,4> color;
   if(!(row>>id>>color[0]>>color[1]>>color[2]>>color[3])||id<531||id>533||colors[id-531])throw std::runtime_error("Invalid native exhaust color");
   for(float c:color)if(!std::isfinite(c)||c<0||c>2)throw std::runtime_error("Native exhaust color is out of range");
   complete(row);out.ship_effects.ships[std::size_t(id-531)].color=color;colors[id-531]=true;
  }else throw std::runtime_error("Unknown flight-effect record: "+kind);
 }
 if(!manifest.eof()||!format)throw std::runtime_error("Incomplete flight-effect manifest");
 for(const auto& ship:out.ship_effects.ships)if(ship.count&&!sprites[0])throw std::runtime_error("Exhaust mounts require their original sprite");
 if(sprites[0])for(std::size_t i=0;i<3;++i)if(!out.ship_effects.ships[i].count)throw std::runtime_error("Original exhaust mounts are missing for a ship variant");
}
void read_original_ship(SceneAssets& out,const std::filesystem::path& root){
 const auto path=root/"original-ship.tsv";
 if(!std::filesystem::exists(path)){
  for(const auto& mesh:out.meshes)if(mesh.id.find("ship_original")==0)throw std::runtime_error("Original ship meshes require their explicit profile");
  return;
 }
 std::ifstream manifest(path,std::ios::binary|std::ios::ate);
 if(!manifest||manifest.tellg()<=0||manifest.tellg()>8192)throw std::runtime_error("Invalid Original ship manifest size");
 manifest.seekg(0);bool format=false,identity=false,pose=false,bounds=false,glass=false,color=false,sample=false,capture=false;
 unsigned version=0;std::array<unsigned,2> expected_parts{},seen_parts{};
 struct Part{std::string id;int role;bool sphere,overlay;};std::vector<Part> parts;
 ShipExhaustMounts mounts;float radius{},ship_length{};std::string line;unsigned records=0;
 while(std::getline(manifest,line)){
  std::istringstream row(line);std::string kind;if(!(row>>kind)||kind[0]=='#')continue;
  if(++records>64)throw std::runtime_error("Too many Original ship records");
  if(kind=="format"){
   if(format||!(row>>version)||(version!=1&&version!=2))throw std::runtime_error("Unsupported Original ship format");
   complete_effect_record(row);format=true;continue;
  }
  if(!format)throw std::runtime_error("Original ship requires its versioned format header");
  if(kind=="ship"){
   std::string name,group;int garage{},physical{},source{},joints{};
   if(identity||!(row>>name>>garage>>group>>physical>>source>>joints)||name!="original"||garage!=0||group!="ship_original"||physical!=2047||source!=530||joints!=31)
    throw std::runtime_error("Invalid independent Original ship identity");
   complete_effect_record(row);identity=true;
  }else if(kind=="pose"){
   std::string name;if(pose||!(row>>name)||name!="initial_cruise_v1")throw std::runtime_error("Invalid closed Original cruise pose");
   complete_effect_record(row);pose=true;
  }else if(kind=="bounds"){
   if(bounds||!(row>>radius>>ship_length)||!std::isfinite(radius)||radius<=0||radius>1.450001f||!std::isfinite(ship_length)||ship_length<=0||ship_length>2.5f)
    throw std::runtime_error("Invalid conservative Original hull bounds");
   complete_effect_record(row);bounds=true;
  }else if(kind=="glass"){
   std::string id;if(glass||!(row>>id)||id!="ship_original_part1")throw std::runtime_error("Invalid Original glass semantic");
   complete_effect_record(row);glass=true;
  }else if(kind=="mount"){
   ExhaustMount m;
   if(!(row>>m.position.x>>m.position.y>>m.position.z>>m.direction.x>>m.direction.y>>m.direction.z>>m.radius>>m.length)||
      !finite(m.position)||!finite(m.direction)||std::abs(length_squared(m.direction)-1.f)>.002f||m.direction.x>-.99f||
      !std::isfinite(m.radius)||m.radius<=0||m.radius>1||!std::isfinite(m.length)||m.length<=0||m.length>4||mounts.count>=2)
    throw std::runtime_error("Invalid Original two-engine nozzle");
   complete_effect_record(row);
   for(std::size_t i=0;i<mounts.count;++i)if(length_squared(m.position-mounts.mounts[i].position)<1e-10f)throw std::runtime_error("Duplicate Original nozzle");
   mounts.mounts[mounts.count++]=m;
  }else if(kind=="color"){
   if(color||!(row>>mounts.color[0]>>mounts.color[1]>>mounts.color[2]>>mounts.color[3]))throw std::runtime_error("Invalid Original exhaust modulation");
   for(float c:mounts.color)if(!std::isfinite(c)||c<0||c>2)throw std::runtime_error("Original exhaust modulation is out of range");
   complete_effect_record(row);color=true;
  }else if(kind=="sample"){
   unsigned bank{},tick{},chunk{},key{};
   if(version!=2||sample||!(row>>bank>>tick>>chunk>>key)||bank!=1||tick!=464||chunk!=4||key!=40)throw std::runtime_error("Invalid Original cockpit sample");
   complete_effect_record(row);sample=true;
  }else if(kind=="capture"){
   std::string hash;
   if(version!=2||capture||!(row>>hash)||!sha256_hex(hash))throw std::runtime_error("Invalid Original cockpit capture identity");
   complete_effect_record(row);capture=true;
  }else if(kind=="actor"){
   unsigned cls{},joints{},pelvis{},count{};std::string model_hash,class_hash;
   if(version!=2||!(row>>cls>>joints>>pelvis>>count>>model_hash>>class_hash)||(cls!=0&&cls!=10)||!count||count>16)
    throw std::runtime_error("Invalid Original cockpit actor");
   const unsigned slot=cls==0?0:1;
   if(expected_parts[slot]||joints!=(cls==0?111u:75u)||pelvis!=(cls==0?1u:2u)||!sha256_hex(model_hash)||!sha256_hex(class_hash))
    throw std::runtime_error("Invalid Original cockpit provenance");
   complete_effect_record(row);expected_parts[slot]=count;
  }else if(kind=="part"){
   std::string id,role;int sphere{},overlay{};
   if(version!=2||!(row>>id>>role>>sphere>>overlay)||(sphere!=0&&sphere!=1)||(overlay!=0&&overlay!=1)||(overlay&&!sphere)||parts.size()>=34||
      std::any_of(parts.begin(),parts.end(),[&](const Part& p){return p.id==id;}))throw std::runtime_error("Invalid or duplicate Original cockpit part");
   int value=-3;
   if(role=="body"&&id=="ship_original"&&!sphere&&!overlay)value=-1;
   else if(role=="glass"&&id=="ship_original_part1"&&sphere&&!overlay)value=-2;
   else if(role=="actor0"||role=="actor10"){
    const std::string prefix="ship_original_part";
    if(id.find(prefix)!=0)throw std::runtime_error("Invalid Original cockpit mesh group");
    const auto index=integer(id.substr(prefix.size()));
    if(index<2||index>33||id!=prefix+std::to_string(index))throw std::runtime_error("Invalid Original cockpit mesh index");
    value=role=="actor0"?0:10;
   }
   if(value==-3)throw std::runtime_error("Invalid Original cockpit part role");
   complete_effect_record(row);parts.push_back({id,value,sphere!=0,overlay!=0});
  }else throw std::runtime_error("Unknown Original ship record: "+kind);
 }
 if(!manifest.eof()||!format||!identity||!pose||!bounds||!glass||!color||mounts.count!=2||out.ship_effects.flame.rgba.empty())
  throw std::runtime_error("Incomplete Original ship profile");
 auto* body=static_cast<MeshAsset*>(nullptr);auto* canopy=static_cast<MeshAsset*>(nullptr);
 if(version==2&&(!sample||!capture||!expected_parts[0]||!expected_parts[1]||parts.size()!=expected_parts[0]+expected_parts[1]+2))
  throw std::runtime_error("Incomplete Original cockpit profile");
 float maximum=0,lo=1e9f,hi=-1e9f;
 std::array<std::vector<std::array<float,3>>,2> actor_opaque;
 std::vector<std::pair<const MeshAsset*,unsigned>> overlays;
 std::size_t visited_parts=0;
 for(auto& mesh:out.meshes)if(mesh.id.find("ship_original")==0){
  ++visited_parts;const Part* part=nullptr;
  if(version==2){
   const auto found=std::find_if(parts.begin(),parts.end(),[&](const Part& p){return p.id==mesh.id;});
   if(found==parts.end())throw std::runtime_error("Undeclared Original cockpit mesh");part=&*found;
  }
  if(mesh.id=="ship_original")body=&mesh;
  else if(mesh.id=="ship_original_part1")canopy=&mesh;
  else{
   if(!part||(part->role!=0&&part->role!=10)||mesh.atlas||(part->sphere&&mesh.alpha!=AlphaMode::Blend))throw std::runtime_error("Unexpected Original ship material part");
   const unsigned actor=part->role==0?0u:1u;++seen_parts[actor];mesh.sphere_uv=part->sphere;mesh.metal_overlay=part->overlay;
   if(part->overlay)overlays.push_back({&mesh,actor});
   else if(mesh.alpha!=AlphaMode::Blend)for(const auto& v:mesh.vertices)actor_opaque[actor].push_back({v.position[0],v.position[1],v.position[2]});
  }
  for(const auto& v:mesh.vertices){
   const Vec3 p{v.position[0],v.position[1],v.position[2]};maximum=std::max(maximum,std::sqrt(length_squared(p)));
   if(mesh.id=="ship_original"||mesh.id=="ship_original_part1"){lo=std::min(lo,p.x);hi=std::max(hi,p.x);}
  }
 }
 if(version==2&&(visited_parts!=parts.size()||seen_parts!=expected_parts))throw std::runtime_error("Missing or repeated Original cockpit parts");
 for(auto& positions:actor_opaque)std::sort(positions.begin(),positions.end());
 for(const auto& overlay:overlays)for(const auto& v:overlay.first->vertices)
  if(!std::binary_search(actor_opaque[overlay.second].begin(),actor_opaque[overlay.second].end(),std::array<float,3>{v.position[0],v.position[1],v.position[2]}))
   throw std::runtime_error("Original chrome overlay has no matching opaque actor surface");
 if(!body||!canopy||body->alpha!=AlphaMode::Opaque||canopy->alpha!=AlphaMode::Blend||body->atlas||canopy->atlas||
    maximum>radius+.000001f||maximum<radius-.001f||std::abs(hi-lo-ship_length)>.001f)
  throw std::runtime_error("Original body/glass escaped their declared pose envelope");
 for(const auto& v:canopy->vertices)if(std::abs(v.color[0]-.5f)>.00001f||std::abs(v.color[1]-.5f)>.00001f||
    std::abs(v.color[2]-.5f)>.00001f||std::abs(v.color[3]-1.f)>.00001f)
  throw std::runtime_error("Original native glass vertex modulation was lost");
 for(std::size_t i=0;i<mounts.count;++i)if(length_squared(mounts.mounts[i].position)>radius*radius)
  throw std::runtime_error("Original nozzle escaped the hull envelope");
 canopy->sphere_uv=true;out.ship_effects.ships.at(3)=mounts;
 out.original_ship.radius=radius;out.original_ship.length=ship_length;
 out.original_ship.available=true;
 out.original_ship.cockpit_available=version==2;
}
void scene_audio(SceneAssets& out,const std::filesystem::path& root,std::size_t& used,std::size_t budget){
 const auto path=root/"audio.tsv";if(!std::filesystem::exists(path))return;
 std::ifstream manifest(path);if(!manifest)throw std::runtime_error("Cannot read audio.tsv");
 constexpr std::array<const char*,scene_audio_clip_count> names{{"music","engine_531","engine_532","engine_533","laser_532","laser_533","rock_hit","rock_destroyed","ship_hit","ship_destroyed","respawn","turbo_531","turbo_532","turbo_533"}};
 std::array<bool,scene_audio_clip_count> clips{},gains{},pitches{};
 bool format=false;std::size_t audio_used=0;std::string line;
 while(std::getline(manifest,line)){
  std::istringstream row(line);std::string kind;if(!(row>>kind)||kind[0]=='#')continue;
  if(kind=="format"){
   int version{};if(format||!(row>>version)||version!=1)throw std::runtime_error("Unsupported scene-audio format");
   complete_effect_record(row);format=true;continue;
  }
  if(!format)throw std::runtime_error("Scene audio requires a versioned format header");
  std::string role;if(!(row>>role))throw std::runtime_error("Missing scene-audio role");
  const auto found=std::find(names.begin(),names.end(),role);
  if(found==names.end())throw std::runtime_error("Unknown scene-audio role: "+role);
  const auto index=std::size_t(found-names.begin());auto& clip=out.audio.clips[index];
  if(kind=="clip"){
   std::string file;std::uint64_t frames{};
   if(clips[index]||!(row>>file>>clip.sample_rate>>clip.channels>>frames>>clip.loop_begin_frame>>clip.loop_end_frame)||
      clip.sample_rate<8000||clip.sample_rate>96000||(clip.channels!=1&&clip.channels!=2)||!frames||frames>std::uint64_t(clip.sample_rate)*600)
    throw std::runtime_error("Invalid scene-audio PCM declaration");
   complete_effect_record(row);const auto end=clip.loop_end_frame?clip.loop_end_frame:frames;
   if(clip.loop_begin_frame>=end||end>frames)throw std::runtime_error("Invalid scene-audio loop bounds");
   const auto count=std::size_t(frames)*clip.channels;const auto expected=count*sizeof(std::int16_t);
   if(expected>256ull*1024*1024-audio_used)throw std::runtime_error("Scene audio exceeds the 256 MiB resource budget");
   const auto data=effect_payload(root,file,expected,used,budget);audio_used+=expected;
   clip.samples.resize(count);
   // The file contract is PCM16 little endian, independent of the host.
   for(std::size_t i=0;i<count;++i){const unsigned value=unsigned(data[i*2])|(unsigned(data[i*2+1])<<8);clip.samples[i]=std::int16_t(value<=32767?int(value):int(value)-65536);}
   clips[index]=true;
  }else if(kind=="gain"||kind=="pitch"){
   float value{};auto& seen=kind=="gain"?gains[index]:pitches[index];
   if(seen||!(row>>value)||!std::isfinite(value)||value<(kind=="gain"?0.f:.25f)||value>4)throw std::runtime_error("Invalid scene-audio gain or pitch");
   complete_effect_record(row);(kind=="gain"?clip.gain:clip.pitch)=value;seen=true;
  }else throw std::runtime_error("Unknown scene-audio record: "+kind);
 }
 // Gemlik has no dedicated respawn cue. Its absence is intentional; do not
 // substitute an unrelated sound or a synthetic silent payload.
 clips[std::size_t(SceneAudioClipId::Respawn)]=true;
 if(!manifest.eof()||!format||!std::all_of(clips.begin(),clips.end(),[](bool value){return value;}))throw std::runtime_error("Incomplete scene-audio manifest");
 out.audio_available=true;
}
void weapons(SceneAssets& out,const std::filesystem::path& root,std::size_t& used,std::size_t budget){
 const auto file=root/"weapons.tsv";if(!std::filesystem::exists(file))return;
 std::ifstream manifest(file);if(!manifest)throw std::runtime_error("Cannot read weapons.tsv");
 bool format=false,tuning=false,bolt=false,ships[3]{},scales[3]{},growth[3]{},growth_distance=false,effects[2]{},reticle[3]{};std::string line;
 while(std::getline(manifest,line)){
  std::istringstream row(line);std::string kind;if(!(row>>kind)||kind[0]=='#')continue;
  if(kind=="format"){
   int version{};if(format||!(row>>version)||version!=1)throw std::runtime_error("Unsupported space-weapon format");
   format=true;complete_effect_record(row);continue;
  }
  if(!format)throw std::runtime_error("Space weapons require a versioned format header");
  if(kind=="ship"){
   int id{},armed{};if(!(row>>id>>armed)||id<531||id>533||ships[id-531]||(armed!=0&&armed!=1)||(id==531&&armed))throw std::runtime_error("Invalid space-weapon ownership; green ship is unarmed");
   complete_effect_record(row);out.combat.ships[std::size_t(id-531)].armed=armed!=0;ships[id-531]=true;
  }else if(kind=="muzzle"){
   int id{};WeaponMuzzle muzzle;
   if(!(row>>id>>muzzle.position.x>>muzzle.position.y>>muzzle.position.z>>muzzle.direction.x>>muzzle.direction.y>>muzzle.direction.z)||id<532||id>533||
      !finite(muzzle.position)||!finite(muzzle.direction)||length_squared(muzzle.position)>256.f||std::abs(length_squared(muzzle.direction)-1.f)>.002f||muzzle.direction.x<=0)
    throw std::runtime_error("Invalid adapted ship cannon muzzle");
   complete_effect_record(row);auto& ship=out.combat.ships[std::size_t(id-531)];if(ship.count>=ship.muzzles.size())throw std::runtime_error("Too many ship cannon muzzles");
   for(std::size_t i=0;i<ship.count;++i)if(length_squared(ship.muzzles[i].position-muzzle.position)<1e-10f)throw std::runtime_error("Duplicate ship cannon muzzle");
   ship.muzzles[ship.count++]=muzzle;
  }else if(kind=="tuning"){
   if(tuning||!(row>>out.combat.projectile_speed>>out.combat.projectile_range>>out.combat.cadence_ticks))throw std::runtime_error("Invalid space-weapon tuning");
   complete_effect_record(row);tuning=true;
  }else if(kind=="bolt"){
   if(bolt||!(row>>out.projectile_effects.bolt_asset_id)||out.projectile_effects.bolt_asset_id.empty())throw std::runtime_error("Invalid original laser model id");
   complete_effect_record(row);bolt=true;
  }else if(kind=="bolt_scale"){
   int id{};float scale{};if(!(row>>id>>scale)||id<531||id>533||scales[id-531]||!std::isfinite(scale)||scale<=0||scale>16)throw std::runtime_error("Invalid ship laser visual scale");
   complete_effect_record(row);out.projectile_effects.bolt_scales[std::size_t(id-531)]=scale;scales[id-531]=true;
  }else if(kind=="bolt_growth"){
   int id{};float multiplier{},distance{};
   if(!(row>>id>>multiplier>>distance)||id<531||id>533||growth[id-531]||!std::isfinite(multiplier)||multiplier<1||multiplier>16||
      !std::isfinite(distance)||distance<=0||distance>1024||(growth_distance&&std::abs(distance-out.projectile_effects.bolt_growth_distance)>.00001f))throw std::runtime_error("Invalid laser plasma growth");
   complete_effect_record(row);out.projectile_effects.bolt_growth[std::size_t(id-531)]=multiplier;out.projectile_effects.bolt_growth_distance=distance;
   growth[id-531]=growth_distance=true;
  }else if(kind=="reticle"){
   std::string name;if(!(row>>name))throw std::runtime_error("Malformed original reticle record");
   const int index=name=="ring"?0:name=="marks"?1:name=="center"?2:-1;
   if(index<0||reticle[index])throw std::runtime_error("Unknown or duplicate original reticle sprite");
   auto sprite=effect_sprite(row,root,used,budget);
   if(sprite.columns!=1||sprite.rows!=1||sprite.frames!=1||sprite.width<2||sprite.height<2)throw std::runtime_error("Original reticle uses a single static sprite");
   out.reticle[std::size_t(index)]=std::move(sprite);reticle[index]=true;
  }else if(kind=="effect"){
   std::string name;if(!(row>>name))throw std::runtime_error("Malformed laser impact effect");const int index=name=="impact"?0:name=="explosion"?1:-1;
   if(index<0||effects[index])throw std::runtime_error("Unknown or duplicate laser impact effect");
   auto sprite=effect_sprite(row,root,used,budget);(index?out.projectile_effects.explosion:out.projectile_effects.impact)=std::move(sprite);effects[index]=true;
  }else throw std::runtime_error("Unknown space-weapon record: "+kind);
 }
 if(!manifest.eof()||!format||!tuning||!bolt||!ships[0]||!ships[1]||!ships[2])throw std::runtime_error("Incomplete space-weapon manifest");
 SpaceCombat validation;if(!validation.set_assets(out.combat))throw std::runtime_error("Invalid space-weapon configuration");
 for(const auto& ship:out.combat.ships)if(ship.armed&&!ship.count)throw std::runtime_error("An armed ship is missing its cannon muzzles");
 if((reticle[0]||reticle[1]||reticle[2])&&!(reticle[0]&&reticle[1]&&reticle[2]))throw std::runtime_error("Incomplete original reticle components");
 if(!out.has(out.projectile_effects.bolt_asset_id))throw std::runtime_error("Original space laser model is missing");
}
}
const MeshAsset& SceneAssets::get(const std::string& id)const{for(const auto& m:meshes)if(m.id==id)return m;throw std::runtime_error("Missing scene mesh "+id);}
bool SceneAssets::has(const std::string& id)const noexcept{for(const auto& m:meshes)if(m.id==id)return true;return false;}
SceneAssets SceneAssets::read(const std::filesystem::path& root){
 SceneAssets out;std::ifstream manifest(root/"assets.tsv");if(!manifest)throw std::runtime_error("Scene assets.tsv is missing");
 std::string line;std::size_t memory_used=0;constexpr std::size_t memory_budget=512ull*1024*1024;
 while(std::getline(manifest,line)){
  if(line.empty()||line[0]=='#')continue;
  std::istringstream row(line);MeshAsset mesh;std::string vertices,indices,texture;
  if(!(row>>mesh.id>>vertices>>indices>>texture>>mesh.width>>mesh.height))throw std::runtime_error("Malformed scene asset manifest");
  std::vector<std::string> extra;std::string token;while(row>>token)extra.push_back(token);
  if(extra.size()==2||extra.size()>5)throw std::runtime_error("Malformed scene material manifest");
  const int stride=extra.empty()?32:integer(extra[0]);if(stride!=32&&stride!=48&&stride!=64)throw std::runtime_error("Unsupported scene vertex stride");mesh.atlas=stride==64;mesh.texture_key=texture;
  if(mesh.id.find("ship_")==0){mesh.wrap_s=mesh.wrap_t=10497;mesh.alpha=AlphaMode::Blend;}
  if(extra.size()>=3){const int s=integer(extra[1]),t=integer(extra[2]);if(!valid_wrap(s)||!valid_wrap(t))throw std::runtime_error("Invalid scene texture wrap mode");mesh.wrap_s=unsigned(s);mesh.wrap_t=unsigned(t);}
  if(extra.size()>=4){if(extra[3]=="OPAQUE")mesh.alpha=AlphaMode::Opaque;else if(extra[3]=="MASK")mesh.alpha=AlphaMode::Mask;else if(extra[3]=="BLEND")mesh.alpha=AlphaMode::Blend;else throw std::runtime_error("Invalid scene material alpha mode");}
  if(extra.size()==5){std::size_t consumed=0;mesh.alpha_cutoff=std::stof(extra[4],&consumed);if(consumed!=extra[4].size()||!std::isfinite(mesh.alpha_cutoff)||mesh.alpha_cutoff<0||mesh.alpha_cutoff>1)throw std::runtime_error("Invalid scene material alpha cutoff");}
  if(mesh.id.empty()||std::any_of(out.meshes.begin(),out.meshes.end(),[&](const auto& other){return other.id==mesh.id;}))throw std::runtime_error("Duplicate scene mesh id");
  if(mesh.width<1||mesh.height<1||mesh.width>4096||mesh.height>4096)throw std::runtime_error("Invalid scene texture dimensions");
  auto vertex_data=bytes(root,vertices,128*1024*1024),index_data=bytes(root,indices,32*1024*1024);
  if(vertex_data.size()%stride||index_data.size()%(3*4))throw std::runtime_error("Malformed scene mesh");
  const auto vertex_count=vertex_data.size()/stride;
  const auto resident_size=vertex_count*sizeof(Vertex)+index_data.size()+std::size_t(mesh.width)*mesh.height*4;
  if(resident_size>memory_budget-memory_used)throw std::runtime_error("Scene exceeds the 512 MiB resource budget");memory_used+=resident_size;
  mesh.vertices.resize(vertex_count);mesh.indices.resize(index_data.size()/4);
  for(std::size_t i=0;i<vertex_count;++i)std::memcpy(&mesh.vertices[i],vertex_data.data()+i*stride,stride);
  std::memcpy(mesh.indices.data(),index_data.data(),index_data.size());
  for(const auto& v:mesh.vertices){
   for(float x:v.position)if(!std::isfinite(x))throw std::runtime_error("Non-finite mesh position");
   for(float x:v.normal)if(!std::isfinite(x))throw std::runtime_error("Non-finite mesh normal");
   for(float x:v.uv)if(!std::isfinite(x))throw std::runtime_error("Non-finite mesh UV");
   for(float x:v.color)if(!std::isfinite(x)||x<0||x>2.001f)throw std::runtime_error("Invalid native vertex colour");
   for(float x:v.atlas_rect)if(!std::isfinite(x)||x<0||x>1.000001f)throw std::runtime_error("Invalid scene atlas rectangle");
   if(v.atlas_rect[0]<=0||v.atlas_rect[1]<=0||v.atlas_rect[0]+v.atlas_rect[2]>1.000001f||v.atlas_rect[1]+v.atlas_rect[3]>1.000001f)throw std::runtime_error("Scene atlas rectangle escaped its texture");
   const float norm=v.normal[0]*v.normal[0]+v.normal[1]*v.normal[1]+v.normal[2]*v.normal[2];
   if(norm<.000001f)throw std::runtime_error("Degenerate mesh normal");
  }
  for(unsigned i:mesh.indices)if(i>=mesh.vertices.size())throw std::runtime_error("Out-of-range scene mesh index");
  mesh.rgba=bytes(root,texture,64*1024*1024);if(mesh.rgba.size()!=std::size_t(mesh.width)*mesh.height*4)throw std::runtime_error("Invalid scene texture data");
  out.meshes.push_back(std::move(mesh));if(out.meshes.size()>1024)throw std::runtime_error("Too many scene meshes");
 }
 out.get("planet_novalis");out.get("planet_kerwan");out.get("planet_eudora");
 out.get("ship_531");out.get("ship_532");out.get("ship_533");
 out.full_galaxy=out.has("planet_veldin");
 if(out.full_galaxy)for(const auto& body:galaxy_catalog())out.get(body.asset_id);
 out.font_rgba=bytes(root,"font.rgba.bin",4*1024*1024);
 std::ifstream metrics(root/"font.tsv");if(!(metrics>>out.font_width>>out.font_height)||out.font_width<1||out.font_height<1||out.font_rgba.size()!=std::size_t(out.font_width)*out.font_height*4)throw std::runtime_error("Invalid native font atlas");
 bool seen[256]{};int code;
 while(metrics>>code){
  FontGlyph g;
  if(!(metrics>>g.u>>g.v>>g.top>>g.advance)||code<0||code>255||seen[code]||
     g.u<0||g.v<0||g.u>=out.font_width||g.v>=out.font_height||g.top<-128||g.top>127||g.advance<-128||g.advance>127)
   throw std::runtime_error("Invalid native glyph");
  seen[code]=true;if(code<128)out.glyphs[code]=g;
 }
 if(!metrics.eof()||!seen['A']||!seen['a']||out.glyphs['A'].advance==0)throw std::runtime_error("Incomplete native font metrics");
 out.sky_rgba=bytes(root,"space_sky.rgba.bin",2048*1024*4);
 out.radar_rgba=bytes(root,"radar.rgba.bin",128*128*4);
 if(out.sky_rgba.size()!=2048*1024*4)throw std::runtime_error("Invalid original space background");
 flight_effects(out,root,memory_used,memory_budget);
 read_original_ship(out,root);
 weapons(out,root,memory_used,memory_budget);
 scene_audio(out,root,memory_used,memory_budget);
 return out;
}
}
