#include "native_thumbnails.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
using namespace salvage;namespace fs=std::filesystem;
namespace {
unsigned checks{};void require(bool yes,const char* what){++checks;if(!yes){std::fprintf(stderr,"thumbnail: %s\n",what);std::exit(1);}}
fs::path utf8_path(const std::string& text){std::u8string bytes(text.size(),u8'\0');if(!text.empty())std::memcpy(bytes.data(),text.data(),text.size());return fs::path(bytes);}
void word(std::vector<unsigned char>& v,unsigned n){for(unsigned i=0;i<4;++i)v.push_back(static_cast<unsigned char>(n>>(i*8)));}
void write_glb(const fs::path& path){
 std::string json=R"({"asset":{"version":"2.0"},"meshes":[{"primitives":[{"attributes":{"POSITION":0,"COLOR_0":1},"indices":2}]}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5126,"count":3,"type":"VEC4"},{"bufferView":2,"componentType":5123,"count":3,"type":"SCALAR"}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":48},{"buffer":0,"byteOffset":84,"byteLength":6}],"buffers":[{"byteLength":92}]})";
 while(json.size()%4)json+=' ';const float positions[9]{0,0,0,1,0,0,0,0,1},colors[12]{1,.3f,.2f,1,.3f,1,.2f,1,.2f,.3f,1,1};const unsigned short indices[3]{0,1,2};
 std::vector<unsigned char> bin(92),glb;std::memcpy(bin.data(),positions,sizeof positions);std::memcpy(bin.data()+36,colors,sizeof colors);std::memcpy(bin.data()+84,indices,sizeof indices);
 word(glb,0x46546c67);word(glb,2);word(glb,unsigned(12+8+json.size()+8+bin.size()));word(glb,unsigned(json.size()));word(glb,0x4e4f534a);glb.insert(glb.end(),json.begin(),json.end());word(glb,unsigned(bin.size()));word(glb,0x004e4942);glb.insert(glb.end(),bin.begin(),bin.end());
 std::ofstream stream(path,std::ios::binary);stream.write(reinterpret_cast<const char*>(glb.data()),std::streamsize(glb.size()));
}
struct Fake {
 std::vector<unsigned char> memory=std::vector<unsigned char>(0x2000000);openrac_api api{};GuestArena arena;unsigned draws{};bool foreign{};
 static inline Fake* self{};
 template<class T>void put(unsigned at,T value){std::memcpy(memory.data()+at,&value,sizeof value);}
 template<class T>T get(unsigned at){T value{};std::memcpy(&value,memory.data()+at,sizeof value);return value;}
 static unsigned relocate(unsigned n){return n;}static unsigned relocate_level(int,unsigned n){return n;}static int loaded(){return 1;}
 static void begin(int mode){require(mode==0,"thumbnail uses native pane paging mode");}
 static void end(){}
 static void draw(int frame,int x,int y,int width,int height,int alpha){
  auto& f=*self;const unsigned state=0x19a4e8,header=f.get<unsigned>(state+0x18),refs=f.get<unsigned>(state+0x20),images=f.get<unsigned>(state+0x24),palettes=f.get<unsigned>(state+0x28);
  require(frame==1&&x==56&&y==6&&width==88&&height==88&&alpha==128,"actual component raster is centered inside native pane");
  require(f.get<unsigned>(header+0x44)==2&&f.get<unsigned>(header+0x24)==2&&f.get<unsigned>(refs+4)==0x10001,"private appended image, palette and frame are coherent");
  require(f.get<unsigned char>(images+8+6)==7&&f.get<unsigned char>(images+8+7)==7,"native thumbnail texture is128 square");
  const auto pixels=f.get<unsigned>(images+8),clut=f.get<unsigned>(palettes+8);
  require(std::count_if(f.memory.begin()+pixels,f.memory.begin()+pixels+128*128,[](unsigned char p){return p!=0;})>100,"native frame contains actual rasterized geometry");
  require(f.get<unsigned char>(clut+3)==0,"palette index0 remains clean transparent");
  f.put<unsigned short>(images+4,42);++f.draws;
  if(f.foreign)f.put<unsigned>(state+0x24,0x48000);
 }
 static void* function(const char* name){if(!std::strcmp(name,"func_001F4630"))return reinterpret_cast<void*>(begin);if(!std::strcmp(name,"func_001F4748"))return reinterpret_cast<void*>(end);if(!std::strcmp(name,"func_00200468"))return reinterpret_cast<void*>(draw);return nullptr;}
 Fake(){self=this;api.memory=memory.data();api.loaded_level=loaded;api.relocate_data=relocate;api.relocate_level_data=relocate_level;api.function=function;
  const unsigned state=0x19a4e8;put(state+0x18,0x34000u);put(state+0x1c,0x30000u);put(state+0x20,0x31000u);put(state+0x24,0x32000u);put(state+0x28,0x33000u);
  put<unsigned short>(0x30000,59901);put<unsigned short>(0x30002,1);put<unsigned short>(0x30008,0xffff);
  put<unsigned>(0x34000+0x44,1);put<unsigned>(0x34000+0x24,1);put<unsigned>(0x50000+0x20,200);put<unsigned>(0x50000+0x24,100);arena.reserve(0x100000,0x40000);
 }
};
}
int main(int argc,char** argv){
 if(argc==3){ThumbnailBitmap bitmap;std::string error;if(!render_thumbnail(utf8_path(argv[1]),bitmap,error)||!write_thumbnail_png(utf8_path(argv[2]),bitmap,error)){std::fprintf(stderr,"%s\n",error.c_str());return 1;}std::printf("rendered source hash %llu\n",static_cast<unsigned long long>(bitmap.source_hash));return 0;}
 const auto root=fs::temp_directory_path()/("salvage-thumb-test-"+std::to_string(std::uint64_t(std::chrono::steady_clock::now().time_since_epoch().count())));
 const auto data=root/"owned data",environment=root/"environment",custom=root/"custom";
 require(thumbnail_levels({"--data",data},"pal")==data/"port/pal","manual PAL content root follows the official --data default");
 require(thumbnail_levels({"--data",data},"ntsc")==data/"port/ntsc","region uses the API version rather than a PAL folder for every client");
 require(thumbnail_levels({},"pal",environment)==environment/"port/pal","OPENRAC_DATA supplies the official default when --data is absent");
 require(thumbnail_levels({"--data",data},"pal",environment)==data/"port/pal","explicit --data overrides OPENRAC_DATA");
 require(thumbnail_levels({"--levels",custom,"--data",data},"pal",environment)==custom,"explicit levels override all data roots regardless of argument order");
 require(thumbnail_levels({"--data",data,"--levels",custom},"pal",environment)==custom,"launcher levels retain precedence over its --data argument");
 require(thumbnail_levels({"--levels",root/"old","--levels",custom},"pal")==custom,"duplicate options use the final value like the official parser");
 require(thumbnail_levels({"--data",root/"old","--data",data},"pal")==data/"port/pal","duplicate data options use the final value");
 require(thumbnail_levels({},"pal").empty(),"missing content roots do not guess the current directory or another installation");
 require(thumbnail_levels({"--data",data},"../foreign").empty(),"unknown API regions cannot escape the data root");
 const auto unicode=fs::path(L"C:/dati con spazi/\u00e8\u65e5");
 require(thumbnail_levels({"--data",unicode},"pal")==unicode/"port/pal","wide Windows paths preserve spaces and Unicode");
 fs::create_directories(data/"port/level_99");std::ofstream(data/"port/level_99/manifest.json")<<"{}";
 require(thumbnail_levels({"--data",data},"pal")==data/"port","legacy flat PAL exports follow the official level99 marker");
 require(thumbnail_levels({"--levels",data/"port/pal"},"pal")==data/"port","explicit PAL levels also support the official legacy layout");
 require(thumbnail_levels({"--levels",data/"port/ntsc"},"ntsc")==data/"port/ntsc","legacy PAL exports cannot redirect NTSC into PAL assets");
 fs::create_directories(data/"port/pal/level_99");std::ofstream(data/"port/pal/level_99/manifest.json")<<"{}";
 require(thumbnail_levels({"--data",data},"pal")==data/"port/pal","a current PAL export takes precedence over an older flat export");
 require(thumbnail_levels({"--levels",custom},"",environment)==custom,"explicit levels need no inferred region");
 fs::create_directories(root/"level_01/mobys");const auto source=root/"level_01/mobys/moby_1736.glb";write_glb(source);
 std::ofstream(root/"level_01/manifest.json")<<R"({"format":4,"classes":{"moby":{"459":{"mesh":"mobys/source.glb"},"1736":{"mesh":"mobys/moby_1736.glb"}}}})";
 ThumbnailBitmap bitmap;std::string error;require(render_thumbnail(source,bitmap,error),error.c_str());
 require(std::count_if(bitmap.pixels.begin(),bitmap.pixels.end(),[](unsigned char p){return p!=0;})>100,"CPU renderer produces visible actual geometry");
 require(bitmap.palette[3]==0&&bitmap.source_hash!=0,"thumbnail has transparent index0 and content fingerprint");
 require(write_thumbnail_png(root/"preview.png",bitmap,error)&&fs::file_size(root/"preview.png")>100,"actual raster can be inspected as PNG");
 {Fake f;NativeThumbnails thumbs(&f.api,f.arena,root,root/"cache");PartDescriptor part;part.id=1;part.source_class=459;part.fragment_class=1736;
  require(thumbs.draw(0x50000,part),thumbs.last_error().c_str());require(f.draws==1,"native thumbnail renderer is invoked");
  require(f.get<unsigned>(0x19a4e8+0x18)==0x34000&&f.get<unsigned>(0x19a4e8+0x20)==0x31000&&f.get<unsigned>(0x19a4e8+0x24)==0x32000&&f.get<unsigned>(0x19a4e8+0x28)==0x33000,"native tables restored after scoped rendering");
  require(f.get<unsigned>(0x34000+0x44)==1&&f.get<unsigned>(0x34000+0x24)==1&&f.get<unsigned short>(0x32004)==42,"shared counts preserved and native cache changes propagated");
  f.foreign=true;require(!thumbs.draw(0x50000,part)&&f.get<unsigned>(0x19a4e8+0x24)==0x48000,"foreign texture owner is never overwritten during rollback");}
 fs::path cache_file;for(const auto& file:fs::directory_iterator(root/"cache"))if(file.path().extension()==".rst")cache_file=file.path();
 require(!cache_file.empty()&&fs::file_size(cache_file)==32+128*128+1024,"content-keyed disk cache stores indexed pixels and CLUT");
 const auto old_time=fs::file_time_type::clock::now()-std::chrono::hours(24);fs::last_write_time(cache_file,old_time);
 {Fake f;NativeThumbnails thumbs(&f.api,f.arena,root,root/"cache");PartDescriptor part;part.id=1;part.source_class=459;part.fragment_class=1736;
  require(thumbs.draw(0x50000,part),thumbs.last_error().c_str());require(fs::last_write_time(cache_file)==old_time,"new renderer session reads valid disk cache without rewriting it");}
 std::ofstream(cache_file,std::ios::binary|std::ios::trunc)<<"corrupt";
 {Fake f;NativeThumbnails thumbs(&f.api,f.arena,root,root/"cache");PartDescriptor part;part.id=1;part.source_class=459;part.fragment_class=1736;
  require(thumbs.draw(0x50000,part)&&fs::file_size(cache_file)==32+128*128+1024,"corrupt thumbnail cache regenerates safely from original owned mesh");}
 // This is our unique synthetic fixture, created below the resolved temp root.
 require(fs::weakly_canonical(root.parent_path())==fs::weakly_canonical(fs::temp_directory_path()),"test cleanup remains in unique temp fixture");fs::remove_all(root);
 std::printf("robot salvage thumbnail: %u checks passed\n",checks);
}
