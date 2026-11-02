// SPDX-License-Identifier: GPL-3.0-or-later
// Production asset-reader tests using only synthetic, temporary bytes.
// No game, GL context, installation, original artwork or save data is used.
#include "assets.hpp"
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
namespace fs=std::filesystem;
unsigned checks{},rejections{};
void require(bool value,const std::string& message){
 ++checks;if(!value)throw std::runtime_error("effect-assets: "+message);
}
bool near(float a,float b){return std::abs(a-b)<.00001f;}
std::string replaced(std::string text,const std::string& from,const std::string& to){
 const auto at=text.find(from);if(at==std::string::npos)throw std::runtime_error("Missing test mutation token");
 text.replace(at,from.size(),to);return text;
}
class Fixture {
 fs::path parent_,root_;
 bool removed_{};
public:
 Fixture():parent_(fs::canonical(fs::current_path())){
  const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
  for(unsigned i=0;i<64;++i){
   auto candidate=parent_/("_space-effect-assets-"+std::to_string(stamp)+"-"+std::to_string(i));
   if(fs::create_directory(candidate)){root_=fs::canonical(candidate);break;}
  }
  if(root_.empty())throw std::runtime_error("Cannot create unique synthetic fixture");
  require(root_.is_absolute()&&root_.parent_path()==parent_,"fixture lives directly under the current working directory");
  const float vertices[3][8]={{0,0,0,0,0,1,0,0},{1,0,0,0,0,1,1,0},{0,1,0,0,0,1,0,1}};
  const std::uint32_t indices[3]={0,1,2};
  binary("triangle.vertices.bin",vertices,sizeof(vertices));binary("triangle.indices.bin",indices,sizeof(indices));
  binary("one.rgba.bin",std::vector<unsigned char>{255,180,32,255});
  binary("font.rgba.bin",std::vector<unsigned char>(256u*256u*4u,0));
  text("font.tsv","256 256\n65 0 0 0 8\n97 16 0 0 8\n");
  binary("space_sky.rgba.bin",std::vector<unsigned char>(2048u*1024u*4u,0));
  binary("radar.rgba.bin",std::vector<unsigned char>(128u*128u*4u,0));
  mesh_manifest(false);payloads();
 }
 ~Fixture(){
  if(removed_||root_.empty())return;
  try{cleanup();}catch(const std::exception& e){std::fprintf(stderr,"Fixture retained: %s (%s)\n",root_.string().c_str(),e.what());}
 }
 const fs::path& root()const{return root_;}
 void text(const std::string& name,const std::string& value){binary(name,value.data(),value.size());}
 void binary(const std::string& name,const void* value,std::size_t size){
  const fs::path relative(name);
  if(relative.is_absolute()||relative.has_root_name()||name.find("..")!=std::string::npos||name.find(':')!=std::string::npos)
   throw std::runtime_error("Invalid fixture-writer path");
  std::ofstream out(root_/relative,std::ios::binary|std::ios::trunc);
  if(!out||!out.write(static_cast<const char*>(value),static_cast<std::streamsize>(size)))throw std::runtime_error("Cannot write synthetic fixture");
 }
 void binary(const std::string& name,const std::vector<unsigned char>& value){binary(name,value.data(),value.size());}
 void optional(const std::string& name,const std::string& contents){
  if(contents.empty()){fs::remove(root_/name);return;}text(name,contents);
 }
 void mesh_manifest(bool laser){
  std::string rows;
  for(const char* id:{"planet_novalis","planet_kerwan","planet_eudora","ship_531","ship_532","ship_533"})
   rows+=std::string(id)+" triangle.vertices.bin triangle.indices.bin one.rgba.bin 1 1 32 10497 10497 OPAQUE\n";
  if(laser)rows+="projectile_laser triangle.vertices.bin triangle.indices.bin one.rgba.bin 1 1 32 10497 10497 BLEND\n";
  text("assets.tsv",rows);
 }
 void payloads(){
  binary("health.indices.bin",std::vector<unsigned char>{0,1,127,252});
  std::vector<unsigned char> palette(1024);
  for(unsigned i=0;i<256;++i){palette[i*4]=static_cast<unsigned char>(i);palette[i*4+1]=static_cast<unsigned char>(255-i);palette[i*4+2]=99;palette[i*4+3]=255;}
  binary("health.palette.rgba.bin",palette);
  binary("flame.rgba.bin",std::vector<unsigned char>{243,243,243,255});
  binary("atlas.rgba.bin",std::vector<unsigned char>{255,0,0,255,0,255,0,255,0,0,255,255,255,255,255,255});
 }
 void original_meshes(bool body=true,bool glass=true,const std::string& glass_alpha="BLEND"){
  mesh_manifest(true);
  std::ifstream input(root_/"assets.tsv");std::string manifest((std::istreambuf_iterator<char>(input)),{});input.close();
  const float vertices[3][12]={{-1,0,0,0,0,1,0,0,1,1,1,1},{1,0,0,0,0,1,1,0,1,1,1,1},{0,1.05f,1,0,0,1,0,1,1,1,1,1}};
  binary("original.vertices.bin",vertices,sizeof(vertices));
  float glass_vertices[3][12];
  for(unsigned i=0;i<3;++i)for(unsigned j=0;j<12;++j)glass_vertices[i][j]=vertices[i][j];
  for(auto& v:glass_vertices){v[6]=v[7]=.5f;v[8]=v[9]=v[10]=.5f;}
  binary("original-glass.vertices.bin",glass_vertices,sizeof(glass_vertices));
  if(body)manifest+="ship_original original.vertices.bin triangle.indices.bin one.rgba.bin 1 1 48 10497 10497 OPAQUE\n";
  if(glass)manifest+="ship_original_part1 original-glass.vertices.bin triangle.indices.bin one.rgba.bin 1 1 48 10497 10497 "+glass_alpha+"\n";
  text("assets.tsv",manifest);
 }
 void manifests(const std::string& flight,const std::string& weapons){
  payloads();optional("flight-effects.tsv",flight);optional("weapons.tsv",weapons);
 }
 void cockpit_meshes(){
  original_meshes();std::ifstream input(root_/"assets.tsv");std::string manifest((std::istreambuf_iterator<char>(input)),{});input.close();
  const float vertices[3][12]={{0,0,0,0,0,1,0,0,1,1,1,1},{.2f,0,0,0,0,1,1,0,1,1,1,1},{0,.2f,.2f,0,0,1,0,1,1,1,1,1}};
  binary("cockpit.vertices.bin",vertices,sizeof vertices);
  manifest+="ship_original_part2 cockpit.vertices.bin triangle.indices.bin one.rgba.bin 1 1 48 10497 10497 OPAQUE\n";
  manifest+="ship_original_part3 cockpit.vertices.bin triangle.indices.bin one.rgba.bin 1 1 48 10497 10497 OPAQUE\n";
  manifest+="ship_original_part4 cockpit.vertices.bin triangle.indices.bin one.rgba.bin 1 1 48 10497 10497 BLEND\n";
  text("assets.tsv",manifest);
 }
 void cleanup(){
  if(removed_)return;
  // Resolve and verify the final absolute recursive-delete target, in the same
  // process, immediately before removal. Never enumerate/pass paths to a shell.
  const auto status=fs::symlink_status(root_);
  if(fs::is_symlink(status)||!fs::is_directory(status))throw std::runtime_error("Fixture root is no longer an owned directory");
  const auto resolved=fs::canonical(root_);
  if(!resolved.is_absolute()||resolved.parent_path()!=parent_||resolved==parent_||
     resolved.filename().string().rfind("_space-effect-assets-",0)!=0)
   throw std::runtime_error("Unsafe fixture cleanup target");
  fs::remove_all(resolved);removed_=true;
  require(!fs::exists(resolved),"verified owned fixture was removed");
 }
};
std::string valid_flight(){
 std::string out="# synthetic native-format contract\nformat\t1\nhealth health.indices.bin health.palette.rgba.bin 2 2 # preserve logical indices\n"
  "sprite flame flame.rgba.bin 1 1 1 1 1 0\n"
  "sprite hit atlas.rgba.bin 2 2 2 2 4 30\n"
  "sprite explosion flame.rgba.bin 1 1 1 1 1 0\n";
 for(int ship=531;ship<=533;++ship){
  const int count=ship==531?8:2;
  for(int i=0;i<count;++i)out+="mount "+std::to_string(ship)+" -1 "+std::to_string(i*.1)+" .2 "+(ship==533?"-.8 0 -.6":"-1 0 0")+" .1 .4\n";
  out+="color "+std::to_string(ship)+(ship==533?" 0 1 .375 1\n":" 1.375 .6875 .25 1\n");
 }
 return out;
}
std::string valid_weapons(){
 return "# synthetic MOD gun-port metadata\nformat 1\nship 531 0\nship 532 1\nship 533 1\n"
  "muzzle 532 .3 .6 .2 1 0 0\nmuzzle 533 .2 -.7 .1 .8 .6 0\n"
  "tuning 400 200 4\nbolt projectile_laser\nbolt_scale 531 1\nbolt_scale 532 .25\nbolt_scale 533 .1\n"
  "effect impact atlas.rgba.bin 2 2 2 2 4 30 # optional native frames\n"
  "effect explosion flame.rgba.bin 1 1 1 1 1 0\n";
}
std::string valid_visibility(){
 return valid_weapons()+
  "reticle ring atlas.rgba.bin 2 2 1 1 1 0\n"
  "reticle marks atlas.rgba.bin 2 2 1 1 1 0\n"
  "reticle center atlas.rgba.bin 2 2 1 1 1 0\n"
  "bolt_growth 532 3 6\nbolt_growth 533 6 6\n";
}
std::string valid_original(){
 return "format 1\nship original 0 ship_original 2047 530 31\npose initial_cruise_v1\n"
  "bounds 1.45 2\nglass ship_original_part1\n"
  "mount -.9 .2 0 -1 0 0 .1 .4\nmount -.9 -.2 0 -1 0 0 .1 .4\n"
  "color 1.375 .6875 .25 1\n";
}
void original_cases(Fixture& f){
 f.original_meshes();f.manifests(valid_flight(),valid_weapons());f.text("original-ship.tsv",valid_original());
 {
  const auto a=spacevoyages::SceneAssets::read(f.root());
  require(a.original_ship.available&&!a.original_ship.cockpit_available&&a.original_ship.mesh_group=="ship_original","v1 Original hull stays compatible without claiming absent cockpit actors");
  require(near(a.original_ship.radius,1.45f)&&near(a.original_ship.length,2),"Original bounds remain in the unchanged contact envelope");
  require(a.get("ship_original_part1").sphere_uv&&!a.get("ship_original").sphere_uv,"only explicitly declared native glass has sphere mapping");
  require(near(a.get("ship_original_part1").vertices[0].color[0],.5f),"Original48-byte glass colors survive loading");
  require(a.ship_effects.ships.at(3).count==2&&a.ship_effects.ships[0].count==8,"Original two engines never replace the eight Green engines");
  require(near(a.ship_effects.ships.at(3).mounts[0].position.y,.2f)&&near(a.ship_effects.ships.at(3).color[0],1.375f),"Original mounts and overbright modulation remain exact");
 }
 const auto bad=[&](const std::string& text,const std::string& label,const std::function<void()>& mutate={}){
  f.original_meshes();f.manifests(valid_flight(),valid_weapons());f.optional("original-ship.tsv",text);if(mutate)mutate();
  bool threw=false;try{(void)spacevoyages::SceneAssets::read(f.root());}catch(const std::exception&){threw=true;}
  require(threw,"Original malformed profile fails closed: "+label);++rejections;
 };
 const auto good=valid_original();
 for(const auto& pair:std::vector<std::pair<std::string,std::string>>{
  {"format 1","format 2"},{"original 0","original 1"},{"2047 530 31","531 530 31"},{"2047 530 31","2047 531 31"},
  {"2047 530 31","2047 530 27"},{"initial_cruise_v1","seq_01_loop"},{"bounds 1.45 2","bounds 1.6 2"},
  {"bounds 1.45 2","bounds 1.4 2"},{"bounds 1.45 2","bounds nan 2"},{"bounds 1.45 2","bounds 1.45 1"},
  {"glass ship_original_part1","glass ship_531"},{"-.9 .2 0 -1 0 0 .1 .4","-.9 .2 0 0 0 0 .1 .4"},
  {"-.9 .2 0 -1 0 0 .1 .4","-.9 .2 0 1 0 0 .1 .4"},{"-.9 .2 0 -1 0 0 .1 .4","-.9 .2 0 nan 0 0 .1 .4"},
  {"-.9 .2 0 -1 0 0 .1 .4","-.9 .2 0 -1 0 0 -1 .4"},{"-.9 .2 0 -1 0 0 .1 .4","-2 .2 0 -1 0 0 .1 .4"},
  {"mount -.9 -.2 0 -1 0 0 .1 .4\n",""},{"mount -.9 -.2 0","mount -.9 .2 0"},
  {"color 1.375 .6875 .25 1","color 3 .6875 .25 1"},{"color 1.375 .6875 .25 1","color nan .6875 .25 1"}})
  bad(replaced(good,pair.first,pair.second),pair.second);
 for(const auto& extra:{"format 1\n","pose initial_cruise_v1\n","bounds 1.45 2\n","glass ship_original_part1\n",
                        "mount -.5 0 0 -1 0 0 .1 .4\n","color 1 1 1 1\n","ship original 0 ship_original 2047 530 31\n","unknown 1\n"})
  bad(good+extra,std::string("extra or duplicate ")+extra);
 bad(replaced(good,"pose initial_cruise_v1","pose initial_cruise_v1 trailing"),"trailing fields");
 bad("","mesh with missing manifest");bad("format 1\n","partial profile");
 bad(good,"missing body",[&]{f.original_meshes(false,true);});
 bad(good,"missing canopy",[&]{f.original_meshes(true,false);});
 bad(good,"opaque canopy",[&]{f.original_meshes(true,true,"OPAQUE");});
 bad(good,"missing flame",[&]{f.optional("flight-effects.tsv",{});});
 bad(good,"truncated Original48 stream",[&]{f.binary("original.vertices.bin",std::vector<unsigned char>(143,0));});
 f.optional("original-ship.tsv",{});f.mesh_manifest(true);f.manifests(valid_flight(),valid_weapons());
 require(!spacevoyages::SceneAssets::read(f.root()).original_ship.available,"older stock pack stays available without a fabricated Original profile");
}
std::string valid_cockpit(){
 const std::string a(64,'a'),b(64,'b');
 return replaced(valid_original(),"format 1\n","format 2\n")+
  "sample 1 464 4 40\ncapture "+a+"\npart ship_original body 0 0\npart ship_original_part1 glass 1 0\n"
  "actor 0 111 1 1 "+a+" "+b+"\nactor 10 75 2 2 "+a+" "+b+"\n"
  "part ship_original_part2 actor0 0 0\npart ship_original_part3 actor10 0 0\npart ship_original_part4 actor10 1 1\n";
}
void cockpit_cases(Fixture& f){
 const auto good=valid_cockpit();
 const auto prepare=[&](const std::string& text){f.cockpit_meshes();f.manifests(valid_flight(),valid_weapons());f.text("original-ship.tsv",text);};
 prepare(good);
 {
  const auto a=spacevoyages::SceneAssets::read(f.root());
  require(a.original_ship.available&&a.original_ship.cockpit_available,"v2 publishes cockpit only after both explicit actors and all material parts validate");
  require(!a.get("ship_original_part2").sphere_uv&&a.get("ship_original_part4").sphere_uv,"crew native metal semantic is independent of body/glass semantics");
  require(a.get("ship_original_part4").metal_overlay&&!a.get("ship_original_part1").metal_overlay,"only coincident chrome gets the overlay depth semantic, never the glass canopy");
  require(a.ship_effects.ships.at(3).count==2&&a.ship_effects.ships[0].count==8,"cockpit parts do not change any ship propulsion profile");
 }
 const auto bad=[&](const std::string& text,const std::string& why,const std::function<void()>& mutate={}){
  prepare(text);if(mutate)mutate();bool threw=false;try{(void)spacevoyages::SceneAssets::read(f.root());}catch(const std::exception&){threw=true;}
  require(threw,"Malformed cockpit rejected: "+why);++rejections;
 };
 for(const auto& pair:std::vector<std::pair<std::string,std::string>>{
  {"sample 1 464 4 40\n",""},{"sample 1 464 4 40","sample 0 464 4 40"},
  {"capture "+std::string(64,'a')+"\n",""},{"capture "+std::string(64,'a'),"capture short"},
  {"sample 1 464 4 40","sample 1 598 4 40"},{"sample 1 464 4 40","sample 1 464 4 39"},
  {"actor 0 111 1 1","actor 0 113 1 1"},{"actor 10 75 2 2","actor 10 75 1 2"},
  {"actor 0 111 1 1","actor 0 111 1 2"},{"actor 10 75 2 2","actor 5 75 2 2"},
  {"part ship_original_part2 actor0 0","part ship_original_part2 actor10 0"},
  {"part ship_original_part4 actor10 1 1\n",""},{"part ship_original body 0","part ship_original actor0 0"},
  {"part ship_original_part4 actor10 1","part ship_original_part4 actor10 2"},
  {"part ship_original_part4 actor10 1 1","part ship_original_part4 actor10 0 1"},
  {"part ship_original_part1 glass 1 0","part ship_original_part1 glass 1 1"},
  {std::string(64,'a'),"short-hash"}})bad(replaced(good,pair.first,pair.second),pair.second);
 bad(good+"sample 1 464 4 40\n","duplicate sample");
 bad(good+"part ship_original_part2 actor0 0 0\n","duplicate part");
 bad(good+"actor 0 111 1 1 "+std::string(64,'a')+" "+std::string(64,'b')+"\n","duplicate actor");
 bad(replaced(good,"format 2","format 1"),"v1 cannot silently accept crew metadata");
 bad(good,"missing actor geometry",[&]{f.original_meshes();});
 bad(good,"extra unclaimed geometry",[&]{std::ifstream input(f.root()/"assets.tsv");std::string data((std::istreambuf_iterator<char>(input)),{});input.close();f.text("assets.tsv",data+"ship_original_part5 cockpit.vertices.bin triangle.indices.bin one.rgba.bin 1 1 48 10497 10497 OPAQUE\n");});
 bad(good,"cockpit actor outside hull envelope",[&]{const float vertices[3][12]={{2,0,0,0,0,1,0,0,1,1,1,1},{0,.2f,0,0,0,1,1,0,1,1,1,1},{0,0,.2f,0,0,1,0,1,1,1,1,1}};f.binary("cockpit.vertices.bin",vertices,sizeof vertices);});
 bad(replaced(good,"part ship_original_part2 actor0 0","part ship_original_part2 actor0 1"),"native metal requires its blended pass");
 bad(good,"chrome must repeat its own actor's opaque surface",[&]{
  const float vertices[3][12]={{.3f,0,0,0,0,1,0,0,1,1,1,1},{0,.3f,0,0,0,1,1,0,1,1,1,1},{0,0,.3f,0,0,1,0,1,1,1,1,1}};f.binary("metal.vertices.bin",vertices,sizeof vertices);
  std::ifstream input(f.root()/"assets.tsv");std::string data((std::istreambuf_iterator<char>(input)),{});input.close();f.text("assets.tsv",replaced(data,"ship_original_part4 cockpit.vertices.bin","ship_original_part4 metal.vertices.bin"));
 });
 f.optional("original-ship.tsv",{});f.mesh_manifest(true);f.manifests(valid_flight(),valid_weapons());
}
void rejected(Fixture& fixture,const std::string& flight,const std::string& weapons,const std::string& label,
              const std::function<void()>& mutation={},const std::string& error_contains={}){
 fixture.manifests(flight,weapons);if(mutation)mutation();bool threw=false;
 try{const auto ignored=spacevoyages::SceneAssets::read(fixture.root());(void)ignored;}
 catch(const std::exception& e){
  threw=true;if(!error_contains.empty())require(std::string(e.what()).find(error_contains)!=std::string::npos,label+": failed at expected validation boundary, got "+e.what());
 }
 require(threw,label+": malformed metadata must fail closed");++rejections;
}
void flight_cases(Fixture& f){
 const auto good=valid_flight();
 const std::vector<std::pair<std::string,std::string>> bad={
  {"unsupported version",replaced(good,"format\t1","format 2")},
  {"missing format header",replaced(good,"format\t1\n","")},
  {"empty versioned manifest","# comments only\n"},
  {"duplicate format",good+"format 1\n"},
  {"duplicate health",good+"health health.indices.bin health.palette.rgba.bin 2 2\n"},
  {"duplicate flame",good+"sprite flame flame.rgba.bin 1 1 1 1 1 0\n"},
  {"duplicate hit",good+"sprite hit flame.rgba.bin 1 1 1 1 1 0\n"},
  {"duplicate exhaust",good+"mount 531 -1 0 .2 -1 0 0 .1 .4\n"},
  {"duplicate color",good+"color 531 1 1 1 1\n"},
  {"mount zero direction",replaced(good,"-1 0 0 .1 .4","0 0 0 .1 .4")},
  {"mount nonunit direction",replaced(good,"-1 0 0 .1 .4","-2 0 0 .1 .4")},
  {"mount NaN direction",replaced(good,"-1 0 0 .1 .4","nan 0 0 .1 .4")},
  {"mount infinite position",replaced(good,"mount 531 -1 ","mount 531 inf ")},
  {"mount position outside bound",replaced(good,"mount 531 -1 ","mount 531 -17 ")},
  {"mount invalid class",good+"mount 530 -1 0 0 -1 0 0 .1 .4\n"},
  {"mount radius zero",replaced(good,"-1 0 0 .1 .4","-1 0 0 0 .4")},
  {"mount radius over bound",replaced(good,"-1 0 0 .1 .4","-1 0 0 5 .4")},
  {"mount length zero",replaced(good,"-1 0 0 .1 .4","-1 0 0 .1 0")},
  {"mount length over bound",replaced(good,"-1 0 0 .1 .4","-1 0 0 .1 65")},
  {"health zero width",replaced(good,"rgba.bin 2 2 #","rgba.bin 0 2 #")},
  {"health negative width",replaced(good,"rgba.bin 2 2 #","rgba.bin -1 2 #")},
  {"health oversized width",replaced(good,"rgba.bin 2 2 #","rgba.bin 4097 2 #")},
  {"unknown sprite",good+"sprite generated_blob flame.rgba.bin 1 1 1 1 1 0\n"},
  {"sprite negative fps",replaced(good,"1 1 1 1 1 0\n","1 1 1 1 1 -1\n")},
  {"sprite NaN fps",replaced(good,"1 1 1 1 1 0\n","1 1 1 1 1 nan\n")},
  {"sprite excessive fps",replaced(good,"1 1 1 1 1 0\n","1 1 1 1 1 1001\n")},
  {"sprite zero columns",replaced(good,"1 1 1 1 1 0\n","1 1 0 1 1 0\n")},
  {"sprite zero rows",replaced(good,"1 1 1 1 1 0\n","1 1 1 0 1 0\n")},
  {"sprite zero frames",replaced(good,"1 1 1 1 1 0\n","1 1 1 1 0 0\n")},
  {"sprite excessive frame count",replaced(good,"2 2 2 2 4 30","2 2 2 2 5 30")},
  {"sprite indivisible atlas",replaced(good,"2 2 2 2 4 30","3 2 2 2 4 30")},
  {"negative color",replaced(good,"1.375 .6875 .25 1","-1 .6875 .25 1")},
  {"color exceeds GS bound",replaced(good,"1.375 .6875 .25 1","2.1 .6875 .25 1")},
  {"unsupported flight record",good+"injected_state 12\n"},
  {"missing flame with mounts","format 1\nmount 531 -1 0 0 -1 0 0 .1 .4\n"},
  {"flame missing ship variants","format 1\nsprite flame flame.rgba.bin 1 1 1 1 1 0\nmount 531 -1 0 0 -1 0 0 .1 .4\n"},
  {"format trailing fields",replaced(good,"format\t1\n","format 1 extra\n")},
  {"health trailing fields",replaced(good,"2 2 # preserve","2 2 extra # preserve")},
  {"sprite trailing fields",replaced(good,"1 1 1 1 1 0\n","1 1 1 1 1 0 extra\n")},
  {"mount trailing fields",replaced(good,"-1 0 0 .1 .4\n","-1 0 0 .1 .4 extra\n")},
  {"color trailing fields",replaced(good,"color 533 0 1 .375 1\n","color 533 0 1 .375 1 extra\n")},
  {"truncated health record","format 1\nhealth health.indices.bin\n"},
  {"truncated sprite record","format 1\nsprite flame flame.rgba.bin 1 1\n"},
  {"truncated mount record","format 1\nmount 531 -1\n"}
 };
 for(const auto& test:bad)rejected(f,test.second,{},test.first);
 std::string excessive="format 1\nsprite flame flame.rgba.bin 1 1 1 1 1 0\n";
 for(int i=0;i<17;++i)excessive+="mount 531 -1 "+std::to_string(i*.1)+" 0 -1 0 0 .1 .4\n";
 excessive+="mount 532 -1 0 0 -1 0 0 .1 .4\nmount 533 -1 0 0 -1 0 0 .1 .4\n";
 rejected(f,excessive,{},"exhaust capacity",{},"Too many ship exhaust mounts");
 rejected(f,good,{},"truncated health index bytes",[&]{f.binary("health.indices.bin",std::vector<unsigned char>{0,1,127});});
 rejected(f,good,{},"truncated palette bytes",[&]{f.binary("health.palette.rgba.bin",std::vector<unsigned char>(1023,0));});
 rejected(f,good,{},"oversized palette bytes",[&]{f.binary("health.palette.rgba.bin",std::vector<unsigned char>(1025,0));});
 rejected(f,good,{},"truncated flame bytes",[&]{f.binary("flame.rgba.bin",std::vector<unsigned char>{255,255,255});});
 rejected(f,good,{},"truncated atlas bytes",[&]{f.binary("atlas.rgba.bin",std::vector<unsigned char>(15,0));});
 std::vector<std::string> paths={"../outside.bin","..\\outside.bin","/outside.bin","C:outside.bin","C:/outside.bin","C:\\outside.bin","nested:stream","//server/share/outside.bin"};
#ifdef _WIN32
 paths.push_back("\\outside.bin");paths.push_back("\\\\server\\share\\outside.bin");
#endif
 for(const auto& path:paths){
  rejected(f,replaced(good,"health.indices.bin",path),{},"health unsafe path "+path,{},"Invalid scene asset path");
  rejected(f,replaced(good,"flame.rgba.bin",path),{},"sprite unsafe path "+path,{},"Invalid scene asset path");
 }
}
void weapon_cases(Fixture& f){
 const auto good=valid_weapons();
 const std::vector<std::pair<std::string,std::string>> bad={
  {"weapon unsupported version",replaced(good,"format 1\n","format 2\n")},
  {"weapon missing format",replaced(good,"format 1\n","")},
  {"weapon duplicate format",good+"format 1\n"},
  {"green armed",replaced(good,"ship 531 0","ship 531 1")},
  {"green muzzle",good+"muzzle 531 .3 .4 0 1 0 0\n"},
  {"unknown ship flag",replaced(good,"ship 532 1","ship 530 1")},
  {"nonboolean ship flag",replaced(good,"ship 532 1","ship 532 2")},
  {"duplicate ship flag",good+"ship 532 1\n"},
  {"missing green flag",replaced(good,"ship 531 0\n","")},
  {"missing purple flag",replaced(good,"ship 533 1\n","")},
  {"missing tuning",replaced(good,"tuning 400 200 4\n","")},
  {"duplicate tuning",good+"tuning 400 200 4\n"},
  {"missing bolt",replaced(good,"bolt projectile_laser\n","")},
  {"duplicate bolt",good+"bolt projectile_laser\n"},
  {"unknown bolt model",replaced(good,"bolt projectile_laser","bolt unknown_model")},
  {"armed ship missing muzzle",replaced(good,"muzzle 532 .3 .6 .2 1 0 0\n","")},
  {"muzzle zero direction",replaced(good,".3 .6 .2 1 0 0",".3 .6 .2 0 0 0")},
  {"muzzle nonunit direction",replaced(good,".3 .6 .2 1 0 0",".3 .6 .2 2 0 0")},
  {"muzzle backward direction",replaced(good,".3 .6 .2 1 0 0",".3 .6 .2 -1 0 0")},
  {"muzzle sideways direction",replaced(good,".3 .6 .2 1 0 0",".3 .6 .2 0 1 0")},
  {"muzzle NaN direction",replaced(good,".3 .6 .2 1 0 0",".3 .6 .2 nan 0 0")},
  {"muzzle outside bound",replaced(good,"muzzle 532 .3","muzzle 532 17")},
  {"duplicate muzzle",good+"muzzle 532 .3 .6 .2 1 0 0\n"},
  {"duplicate visual scale",good+"bolt_scale 532 .25\n"},
  {"invalid scale class",good+"bolt_scale 530 .25\n"},
  {"zero visual scale",replaced(good,"bolt_scale 532 .25","bolt_scale 532 0")},
  {"excessive visual scale",replaced(good,"bolt_scale 532 .25","bolt_scale 532 17")},
  {"NaN visual scale",replaced(good,"bolt_scale 532 .25","bolt_scale 532 nan")},
  {"zero projectile speed",replaced(good,"tuning 400 200 4","tuning 0 200 4")},
  {"excessive projectile speed",replaced(good,"tuning 400 200 4","tuning 100001 200 4")},
  {"NaN projectile speed",replaced(good,"tuning 400 200 4","tuning nan 200 4")},
  {"zero projectile range",replaced(good,"tuning 400 200 4","tuning 400 0 4")},
  {"zero cadence",replaced(good,"tuning 400 200 4","tuning 400 200 0")},
  {"excessive cadence",replaced(good,"tuning 400 200 4","tuning 400 200 3601")},
  {"unsupported weapons record",good+"invented_reward 500\n"},
  {"duplicate impact",good+"effect impact flame.rgba.bin 1 1 1 1 1 0\n"},
  {"unknown impact sprite",good+"effect generated_blob flame.rgba.bin 1 1 1 1 1 0\n"},
  {"impact negative fps",replaced(good,"2 2 2 2 4 30","2 2 2 2 4 -1")},
  {"impact bad atlas frames",replaced(good,"2 2 2 2 4 30","2 2 2 2 5 30")},
  {"weapon format trailing fields",replaced(good,"format 1\n","format 1 extra\n")},
  {"ship trailing fields",replaced(good,"ship 532 1\n","ship 532 1 extra\n")},
  {"muzzle trailing fields",replaced(good,".3 .6 .2 1 0 0\n",".3 .6 .2 1 0 0 extra\n")},
  {"tuning trailing fields",replaced(good,"tuning 400 200 4\n","tuning 400 200 4 extra\n")},
  {"bolt trailing fields",replaced(good,"bolt projectile_laser\n","bolt projectile_laser extra\n")},
  {"scale trailing fields",replaced(good,"bolt_scale 532 .25\n","bolt_scale 532 .25 extra\n")},
  {"effect trailing fields",replaced(good,"1 1 1 1 1 0\n","1 1 1 1 1 0 extra\n")},
  {"truncated tuning",replaced(good,"tuning 400 200 4","tuning 400")},
  {"truncated effect",replaced(good,"effect impact atlas.rgba.bin 2 2 2 2 4 30 # optional native frames\n","effect impact\n")}
 };
 for(const auto& test:bad)rejected(f,{},test.second,test.first);
 rejected(f,{},replaced(good,"atlas.rgba.bin","../outside.bin"),"weapon effect traversal",{},"Invalid scene asset path");
 rejected(f,{},replaced(good,"atlas.rgba.bin","C:outside.bin"),"weapon effect drive-relative path",{},"Invalid scene asset path");
 rejected(f,{},good,"truncated impact bytes",[&]{f.binary("atlas.rgba.bin",std::vector<unsigned char>(15,0));});
 std::string excessive=replaced(good,"muzzle 532 .3 .6 .2 1 0 0\n","");
 for(int i=0;i<9;++i)excessive+="muzzle 532 .3 "+std::to_string(i*.1)+" .2 1 0 0\n";
 rejected(f,{},excessive,"muzzle capacity",{},"Too many ship cannon muzzles");
}
void visibility_cases(Fixture& f){
 const auto good=valid_visibility();
 const std::vector<std::pair<std::string,std::string>> bad={
  {"duplicate reticle ring",good+"reticle ring atlas.rgba.bin 2 2 1 1 1 0\n"},
  {"duplicate reticle marks",good+"reticle marks atlas.rgba.bin 2 2 1 1 1 0\n"},
  {"duplicate reticle center",good+"reticle center atlas.rgba.bin 2 2 1 1 1 0\n"},
  {"unsupported reticle component",good+"reticle generated_blob atlas.rgba.bin 2 2 1 1 1 0\n"},
  {"reticle one-pixel sprite",replaced(good,"reticle ring atlas.rgba.bin 2 2 1 1 1 0","reticle ring flame.rgba.bin 1 1 1 1 1 0")},
  {"reticle animated frame2",replaced(good,"reticle ring atlas.rgba.bin 2 2 1 1 1 0","reticle ring atlas.rgba.bin 2 2 2 1 2 30")},
  {"reticle atlas columns",replaced(good,"reticle ring atlas.rgba.bin 2 2 1 1 1 0","reticle ring atlas.rgba.bin 2 2 2 1 1 0")},
  {"reticle atlas rows",replaced(good,"reticle ring atlas.rgba.bin 2 2 1 1 1 0","reticle ring atlas.rgba.bin 2 2 1 2 1 0")},
  {"reticle NaN fps",replaced(good,"reticle ring atlas.rgba.bin 2 2 1 1 1 0","reticle ring atlas.rgba.bin 2 2 1 1 1 nan")},
  {"reticle trailing fields",replaced(good,"reticle ring atlas.rgba.bin 2 2 1 1 1 0\n","reticle ring atlas.rgba.bin 2 2 1 1 1 0 extra\n")},
  {"reticle truncated record",replaced(good,"reticle ring atlas.rgba.bin 2 2 1 1 1 0\n","reticle ring atlas.rgba.bin 2\n")},
  {"reticle traversal",replaced(good,"reticle ring atlas.rgba.bin","reticle ring ../outside.bin")},
  {"reticle drive-relative path",replaced(good,"reticle ring atlas.rgba.bin","reticle ring C:outside.bin")},
  {"duplicate plasma growth",good+"bolt_growth 532 3 6\n"},
  {"unknown growth class",replaced(good,"bolt_growth 532 3 6","bolt_growth 530 3 6")},
  {"growth below one",replaced(good,"bolt_growth 532 3 6","bolt_growth 532 .99 6")},
  {"growth over maximum",replaced(good,"bolt_growth 532 3 6","bolt_growth 532 17 6")},
  {"growth NaN multiplier",replaced(good,"bolt_growth 532 3 6","bolt_growth 532 nan 6")},
  {"growth infinite multiplier",replaced(good,"bolt_growth 532 3 6","bolt_growth 532 inf 6")},
  {"growth zero distance",replaced(good,"bolt_growth 532 3 6","bolt_growth 532 3 0")},
  {"growth negative distance",replaced(good,"bolt_growth 532 3 6","bolt_growth 532 3 -1")},
  {"growth excessive distance",replaced(good,"bolt_growth 532 3 6","bolt_growth 532 3 1025")},
  {"growth NaN distance",replaced(good,"bolt_growth 532 3 6","bolt_growth 532 3 nan")},
  {"growth distance mismatch",replaced(good,"bolt_growth 533 6 6","bolt_growth 533 6 8")},
  {"growth trailing fields",replaced(good,"bolt_growth 532 3 6\n","bolt_growth 532 3 6 extra\n")},
  {"growth truncated record",replaced(good,"bolt_growth 532 3 6\n","bolt_growth 532 3\n")}
 };
 for(const auto& test:bad)rejected(f,{},test.second,test.first);
 // Every nonempty proper subset must fail, including each individual missing
 // component; no one/two-sprite fallback may silently replace the native trio.
 const char* names[]={"ring","marks","center"};
 for(unsigned subset=1;subset<7;++subset){
  auto partial=valid_weapons();
  for(unsigned i=0;i<3;++i)if(subset&(1u<<i))partial+=std::string("reticle ")+names[i]+" atlas.rgba.bin 2 2 1 1 1 0\n";
  rejected(f,{},partial,"partial native reticle trio "+std::to_string(subset),{},"Incomplete original reticle components");
 }
 const auto reticle_without_other_atlas_use=replaced(good,"effect impact atlas.rgba.bin 2 2 2 2 4 30 # optional native frames\n","");
 rejected(f,{},reticle_without_other_atlas_use,"truncated static reticle bytes",[&]{f.binary("atlas.rgba.bin",std::vector<unsigned char>(15,0));});
}
}
int main(){
 try{
  Fixture fixture;
  fixture.manifests({},{});
  {
   const auto legacy=spacevoyages::SceneAssets::read(fixture.root());
   require(legacy.meshes.size()==6,"legacy minimal six-mesh pack loads without either optional metadata file");
   require(legacy.health_meter.indices.empty()&&legacy.ship_effects.flame.rgba.empty(),"legacy pack has no fabricated effects");
   require(!legacy.combat.ships[0].armed&&!legacy.combat.ships[1].armed&&!legacy.combat.ships[2].armed,"legacy pack remains disarmed");
   require(legacy.reticle[0].rgba.empty()&&legacy.reticle[1].rgba.empty()&&legacy.reticle[2].rgba.empty(),"legacy pack has no fabricated aiming reticle");
   require(legacy.projectile_effects.bolt_growth==std::array<float,3>{1,1,1},"legacy growth stays one for all three ships");
  }
  fixture.mesh_manifest(true);fixture.manifests(valid_flight(),valid_weapons());
  {
   const auto assets=spacevoyages::SceneAssets::read(fixture.root());
   const auto& health=assets.health_meter;
   require(health.width==2&&health.height==2&&health.indices.size()==4,"generic reader accepts a synthetic2x2 indexed health image");
   require(health.indices==std::vector<unsigned char>({0,1,127,252}),"health indices remain exact before palette lookup");
   require(health.palette_rgba.size()==1024&&health.palette_rgba[127*4]==127&&health.palette_rgba[127*4+1]==128,"logical CLUT bytes remain exact");
   require(assets.ship_effects.flame.rgba==std::vector<unsigned char>({243,243,243,255}),"static original-sprite payload remains exact");
   require(assets.ship_effects.ships[0].count==8&&assets.ship_effects.ships[1].count==2&&assets.ship_effects.ships[2].count==2,"independent per-ship exhaust mount counts are retained");
   require(near(assets.ship_effects.ships[0].color[0],1.375f),"GS color values over1 survive the asset reader");
   require(near(assets.ship_effects.ships[2].mounts[0].direction.x,-.8f)&&near(assets.ship_effects.ships[2].mounts[0].direction.z,-.6f),"inclined native nozzle normals are not forced to minusX");
   const auto& hit=assets.ship_effects.hit;
   require(hit.width==2&&hit.height==2&&hit.columns==2&&hit.rows==2&&hit.frames==4&&near(hit.fps,30),"atlas layout and frame cadence are retained");
   require(!assets.combat.ships[0].armed&&assets.combat.ships[1].armed&&assets.combat.ships[2].armed,"only selected orange and purple ships are armed");
   require(assets.combat.ships[1].count==1&&assets.combat.ships[2].count==1,"adapted muzzle counts are read independently of exhausts");
   require(near(assets.combat.ships[1].muzzles[0].position.y,.6f)&&near(assets.combat.ships[2].muzzles[0].direction.y,.6f),"weapon geometry remains exactly in baked local coordinates");
   require(near(assets.combat.projectile_speed,400)&&near(assets.combat.projectile_range,200)&&assets.combat.cadence_ticks==4,"range and speed preserve distinct native meanings");
   require(assets.projectile_effects.bolt_asset_id=="projectile_laser"&&assets.has("projectile_laser"),"borrowed projectile mesh id resolves to an existing mesh");
   require(near(assets.projectile_effects.bolt_scales[1],.25f)&&near(assets.projectile_effects.bolt_scales[2],.1f),"per-ship visual scale remains separate from combat tuning");
   require(assets.projectile_effects.impact.frames==4&&assets.projectile_effects.explosion.frames==1,"optional combat sprites retain their layouts");
   require(assets.projectile_effects.bolt_growth==std::array<float,3>{1,1,1},"valid older weapons metadata retains no-growth defaults");
   require(assets.reticle[0].rgba.empty()&&assets.reticle[1].rgba.empty()&&assets.reticle[2].rgba.empty(),"valid older weapons metadata needs no new reticle records");
  }
  fixture.manifests({},valid_visibility());
  {
   const auto assets=spacevoyages::SceneAssets::read(fixture.root());
   for(const auto& sprite:assets.reticle){
    require(sprite.width==2&&sprite.height==2&&sprite.columns==1&&sprite.rows==1&&sprite.frames==1&&near(sprite.fps,0),"each synthetic native reticle component is one static2x2sprite");
    require(sprite.rgba==std::vector<unsigned char>({255,0,0,255,0,255,0,255,0,0,255,255,255,255,255,255}),"reticle RGB and alpha are byte-exact in the reader; GS modulation belongs to GPU draw");
   }
   require(assets.projectile_effects.bolt_growth==std::array<float,3>{1,3,6}&&near(assets.projectile_effects.bolt_growth_distance,6),"per-ship visual plasma expansion retains a shared travel distance and unarmed default");
   require(near(assets.projectile_effects.bolt_scales[1],.25f)&&near(assets.projectile_effects.bolt_scales[2],.1f),"growth does not overwrite measured muzzle birth scales");
   require(near(assets.combat.projectile_speed,400)&&near(assets.combat.projectile_range,200)&&assets.combat.cadence_ticks==4&&!assets.combat.ships[0].armed,"new presentation metadata leaves physical weapon rules unchanged");
  }
  fixture.manifests({},replaced(replaced(valid_visibility(),"bolt_growth 532 3 6","bolt_growth 532 1 1024"),"bolt_growth 533 6 6","bolt_growth 533 16 1024"));
  {
   const auto assets=spacevoyages::SceneAssets::read(fixture.root());
   require(assets.projectile_effects.bolt_growth==std::array<float,3>{1,1,16}&&near(assets.projectile_effects.bolt_growth_distance,1024),"growth and shared-distance limits accept valid boundary values");
  }
  fixture.manifests("format 1\nhealth health.indices.bin health.palette.rgba.bin 2 2\n",{});
  require(spacevoyages::SceneAssets::read(fixture.root()).ship_effects.flame.rgba.empty(),"health-only extension does not require fake exhaust sprites");
  fixture.manifests({},valid_weapons());
  require(spacevoyages::SceneAssets::read(fixture.root()).health_meter.indices.empty(),"weapons-only extension does not require unrelated HUD metadata");
  flight_cases(fixture);weapon_cases(fixture);visibility_cases(fixture);original_cases(fixture);cockpit_cases(fixture);
  fixture.manifests(valid_flight(),valid_weapons());
  require(spacevoyages::SceneAssets::read(fixture.root()).meshes.size()==7,"negative cases do not corrupt the independent valid fixture");
  fixture.cleanup();
  std::printf("effect-assets: %u checks, %u malformed cases rejected; synthetic metadata only; PASS\n",checks,rejections);
  return 0;
 }catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
