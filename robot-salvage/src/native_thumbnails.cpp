// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_thumbnails.hpp"
#include "common/json.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cwchar>
#include <fstream>
#include <limits>
#include <map>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <wincodec.h>
#include <wrl/client.h>
#endif
namespace salvage {
namespace {
using openrac::json::Value;namespace fs=std::filesystem;
using Bytes=std::vector<unsigned char>;
constexpr unsigned Size=ThumbnailBitmap::dimension;
fs::path utf8_path(std::string_view text){std::u8string bytes(text.size(),u8'\0');if(!text.empty())std::memcpy(bytes.data(),text.data(),text.size());return fs::path(bytes);}
std::uint64_t hash(const unsigned char* bytes,std::size_t count,std::uint64_t n=14695981039346656037ull){for(std::size_t i=0;i<count;++i){n^=bytes[i];n*=1099511628211ull;}return n;}
Bytes read_file(const fs::path& path,std::size_t maximum=64*1024*1024){
 std::ifstream stream(path,std::ios::binary|std::ios::ate);if(!stream)throw std::runtime_error("Cannot read part asset: "+path.filename().string());
 const auto length=stream.tellg();if(length<0||std::uint64_t(length)>maximum)throw std::runtime_error("Part asset exceeds safe size");
 Bytes bytes(static_cast<std::size_t>(length));stream.seekg(0);if(!bytes.empty()&&!stream.read(reinterpret_cast<char*>(bytes.data()),std::streamsize(bytes.size())))throw std::runtime_error("Truncated part asset");return bytes;
}
std::uint32_t u32(const unsigned char* at){std::uint32_t n;std::memcpy(&n,at,4);return n;}
std::size_t integer(const Value& value,std::size_t fallback=0){if(!value.is_number())return fallback;const auto n=value.number();if(!std::isfinite(n)||n<0||n>16777216||std::floor(n)!=n)throw std::runtime_error("Invalid GLB integer");return std::size_t(n);}
struct Glb {Value json;Bytes bin,raw;fs::path path;};
Glb load_glb(const fs::path& path){
 Glb glb;glb.path=path;glb.raw=read_file(path);
 if(glb.raw.size()<20||u32(glb.raw.data())!=0x46546c67||u32(glb.raw.data()+4)!=2||u32(glb.raw.data()+8)!=glb.raw.size())throw std::runtime_error("Invalid part GLB header");
 bool json=false,bin=false;
 for(std::size_t at=12;at+8<=glb.raw.size();){const auto size=u32(glb.raw.data()+at),kind=u32(glb.raw.data()+at+4);at+=8;if(size>glb.raw.size()-at)throw std::runtime_error("Invalid part GLB chunk");
  if(kind==0x4e4f534a&&!json){std::string error;if(!openrac::json::parse({reinterpret_cast<const char*>(glb.raw.data()+at),size},glb.json,error))throw std::runtime_error(error);json=true;}
  else if(kind==0x004e4942&&!bin){glb.bin.assign(glb.raw.begin()+std::ptrdiff_t(at),glb.raw.begin()+std::ptrdiff_t(at+size));bin=true;}at+=size;}
 if(!json||!bin)throw std::runtime_error("Part GLB needs JSON and embedded BIN");return glb;
}
using Rows=std::vector<std::vector<float>>;
Rows accessor(const Glb& glb,std::size_t index){
 const auto& as=glb.json["accessors"][index];if(!as.is_object()||as["sparse"].is_object())throw std::runtime_error("Unsupported part accessor");
 const auto& view=glb.json["bufferViews"][integer(as["bufferView"])];if(!view.is_object()||integer(view["buffer"])!=0)throw std::runtime_error("Invalid part buffer view");
 const auto type=as["type"].string();const unsigned n=type=="SCALAR"?1:type=="VEC2"?2:type=="VEC3"?3:type=="VEC4"?4:type=="MAT4"?16:0;
 const auto kind=integer(as["componentType"]);const unsigned bytes=kind==5120||kind==5121?1:kind==5122||kind==5123?2:kind==5125||kind==5126?4:0;
 if(!n||!bytes)throw std::runtime_error("Unsupported part accessor type");
 const auto count=integer(as["count"]),start=integer(view["byteOffset"])+integer(as["byteOffset"]),stride=integer(view["byteStride"],n*bytes);
 const auto view_start=integer(view["byteOffset"]),view_size=integer(view["byteLength"]);
 if(count>2000000||stride<n*bytes||view_start>glb.bin.size()||view_size>glb.bin.size()-view_start||start>view_start+view_size||
    (count&&(count-1)>((view_start+view_size-start>=n*bytes)?(view_start+view_size-start-n*bytes)/stride:0))||
    (count&&view_start+view_size-start<n*bytes))throw std::runtime_error("Part accessor is out of bounds");
 Rows out(count,std::vector<float>(n));const bool normalized=as["normalized"].boolean();
 for(std::size_t i=0;i<count;++i)for(unsigned j=0;j<n;++j){const auto* p=glb.bin.data()+start+i*stride+j*bytes;float value{};
  switch(kind){case 5120:{std::int8_t x;std::memcpy(&x,p,1);value=normalized?std::max(-1.f,float(x)/127):float(x);break;}
   case 5121:value=normalized?float(*p)/255:float(*p);break;case 5122:{std::int16_t x;std::memcpy(&x,p,2);value=normalized?std::max(-1.f,float(x)/32767):float(x);break;}
   case 5123:{std::uint16_t x;std::memcpy(&x,p,2);value=normalized?float(x)/65535:float(x);break;}case 5125:value=float(u32(p));break;case 5126:std::memcpy(&value,p,4);break;}
  if(!std::isfinite(value))throw std::runtime_error("Nonfinite part geometry");out[i][j]=value;}
 return out;
}
struct Image {unsigned width{},height{};Bytes rgba;};
#ifdef _WIN32
using Microsoft::WRL::ComPtr;
struct ComScope {HRESULT result=CoInitializeEx(nullptr,COINIT_MULTITHREADED);~ComScope(){if(SUCCEEDED(result))CoUninitialize();}};
void checked(HRESULT result,const char* operation){if(FAILED(result))throw std::runtime_error(operation);}
ComPtr<IWICImagingFactory> imaging(){ComPtr<IWICImagingFactory> factory;checked(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)),"Cannot create thumbnail decoder");return factory;}
Image decode_png(const Bytes& bytes){
 if(bytes.empty()||bytes.size()>64*1024*1024)throw std::runtime_error("Invalid part texture");
 auto factory=imaging();ComPtr<IWICStream> stream;checked(factory->CreateStream(&stream),"Cannot create texture stream");
 checked(stream->InitializeFromMemory(const_cast<BYTE*>(bytes.data()),DWORD(bytes.size())),"Cannot read texture stream");
 ComPtr<IWICBitmapDecoder> decoder;checked(factory->CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnLoad,&decoder),"Cannot decode part texture");
 ComPtr<IWICBitmapFrameDecode> frame;checked(decoder->GetFrame(0,&frame),"Cannot read part texture frame");Image image;
 checked(frame->GetSize(&image.width,&image.height),"Cannot size part texture");if(!image.width||!image.height||image.width>8192||image.height>8192)throw std::runtime_error("Part texture dimensions exceed budget");
 image.rgba.resize(std::size_t(image.width)*image.height*4);ComPtr<IWICFormatConverter> convert;
 checked(factory->CreateFormatConverter(&convert),"Cannot create texture converter");checked(convert->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom),"Cannot convert texture pixels");
 checked(convert->CopyPixels(nullptr,image.width*4,UINT(image.rgba.size()),image.rgba.data()),"Cannot copy texture pixels");return image;
}
void indexed(const Bytes& rgba,ThumbnailBitmap& out){
 auto factory=imaging();ComPtr<IWICBitmap> image;checked(factory->CreateBitmapFromMemory(Size,Size,GUID_WICPixelFormat32bppRGBA,Size*4,UINT(rgba.size()),const_cast<BYTE*>(rgba.data()),&image),"Cannot create part raster");
 ComPtr<IWICPalette> palette;checked(factory->CreatePalette(&palette),"Cannot create part palette");checked(palette->InitializeFromBitmap(image.Get(),255,TRUE),"Cannot quantize part raster");
 std::array<WICColor,256> colors{};UINT count{};checked(palette->GetColors(UINT(colors.size()),colors.data(),&count),"Cannot copy part palette");
 unsigned transparent=0;for(unsigned i=0;i<count;++i)if((colors[i]>>24)==0){transparent=i;break;}
 ComPtr<IWICFormatConverter> convert;checked(factory->CreateFormatConverter(&convert),"Cannot create indexed converter");
 checked(convert->Initialize(image.Get(),GUID_WICPixelFormat8bppIndexed,WICBitmapDitherTypeNone,palette.Get(),50,WICBitmapPaletteTypeCustom),"Cannot index part raster");
 checked(convert->CopyPixels(nullptr,Size,UINT(out.pixels.size()),out.pixels.data()),"Cannot copy indexed raster");
 for(auto& pixel:out.pixels)pixel=std::uint8_t(pixel==transparent?0:pixel==0?transparent:pixel);
 for(unsigned i=0;i<count;++i){const unsigned logical=i==transparent?0:i==0?transparent:i;const unsigned physical=(logical&~0x18u)|((logical&8u)<<1)|((logical&0x10u)>>1);const auto c=colors[i];
  out.palette[physical*4]=std::uint8_t(c>>16);out.palette[physical*4+1]=std::uint8_t(c>>8);out.palette[physical*4+2]=std::uint8_t(c);out.palette[physical*4+3]=std::uint8_t(((c>>24)+1)/2);}
 out.palette[3]=0;
}
#else
Image decode_png(const Bytes&){throw std::runtime_error("Windows WIC thumbnail decoding required");}
void indexed(const Bytes&,ThumbnailBitmap&){throw std::runtime_error("Windows WIC thumbnail quantization required");}
#endif
struct Vertex {Vec3 p{},normal{0,0,1};float u{},v{};std::array<float,4> color{{1,1,1,1}};};
struct Triangle {std::array<Vertex,3> vertices;int image=-1;std::array<float,4> color{{1,1,1,1}};float cutoff=.5f;};
struct Geometry {std::vector<Triangle> triangles;std::vector<Image> images;std::uint64_t source_hash{};};
std::vector<Bytes> image_sources(const Glb& glb){
 std::vector<Bytes> result;
 const auto root=glb.path.parent_path().parent_path().lexically_normal();
 for(const auto& source:glb.json["images"].items()){
  Bytes bytes;const auto uri=source["uri"].string();
  if(!uri.empty()){
   const auto path=(glb.path.parent_path()/utf8_path(uri)).lexically_normal();
   const auto relative=path.lexically_relative(root);if(relative.empty()||relative.is_absolute()||*relative.begin()=="..")throw std::runtime_error("Part texture path leaves its level");bytes=read_file(path);
  }else{const auto& view=glb.json["bufferViews"][integer(source["bufferView"])];const auto start=integer(view["byteOffset"]),size=integer(view["byteLength"]);
   if(start>glb.bin.size()||size>glb.bin.size()-start)throw std::runtime_error("Part image buffer out of bounds");bytes.assign(glb.bin.begin()+std::ptrdiff_t(start),glb.bin.begin()+std::ptrdiff_t(start+size));}
  result.push_back(std::move(bytes));
 }
 return result;
}
std::uint64_t fingerprint(const Glb& glb,const std::vector<Bytes>& textures){
 constexpr unsigned char version[]{'r','o','b','o','t','-','t','h','u','m','b','-','1'};
 auto n=hash(version,sizeof version);n=hash(glb.raw.data(),glb.raw.size(),n);
 for(const auto& texture:textures)n=hash(texture.data(),texture.size(),n);return n;
}
Geometry geometry(const Glb& glb){
 Geometry result;const auto textures=image_sources(glb);result.source_hash=fingerprint(glb,textures);
 for(const auto& texture:textures)result.images.push_back(decode_png(texture));
 for(const auto& mesh:glb.json["meshes"].items())for(const auto& primitive:mesh["primitives"].items()){
  if(integer(primitive["mode"],4)!=4)continue;const auto& attrs=primitive["attributes"];
  const auto positions=accessor(glb,integer(attrs["POSITION"]));
  const auto normals=attrs["NORMAL"].is_number()?accessor(glb,integer(attrs["NORMAL"])):Rows{};
  const auto uv=attrs["TEXCOORD_0"].is_number()?accessor(glb,integer(attrs["TEXCOORD_0"])):Rows{};
  const auto color=attrs["COLOR_0"].is_number()?accessor(glb,integer(attrs["COLOR_0"])):Rows{};
  Rows indices;if(primitive["indices"].is_number())indices=accessor(glb,integer(primitive["indices"]));else for(std::size_t i=0;i<positions.size();++i)indices.push_back({float(i)});
  const auto& material=glb.json["materials"][integer(primitive["material"])];
  int image=-1;const auto& texture=material["pbrMetallicRoughness"]["baseColorTexture"]["index"];
  if(texture.is_number()){const auto& source=glb.json["textures"][integer(texture)]["source"];if(source.is_number())image=int(integer(source));}
  for(std::size_t i=0;i+2<indices.size();i+=3){Triangle triangle;triangle.image=image;
   if(material["alphaCutoff"].is_number())triangle.cutoff=float(material["alphaCutoff"].number());
   const auto& factor=material["pbrMetallicRoughness"]["baseColorFactor"].items();if(factor.size()==4)for(unsigned k=0;k<4;++k)triangle.color[k]=float(factor[k].number(1));
   for(unsigned j=0;j<3;++j){const auto index=std::size_t(indices[i+j][0]);if(index>=positions.size()||positions[index].size()!=3)throw std::runtime_error("Invalid part triangle index");auto& vertex=triangle.vertices[j];
    const auto& p=positions[index];vertex.p={p[0],p[1],p[2]};
    if(index<normals.size()&&normals[index].size()>=3)vertex.normal={normals[index][0],normals[index][1],normals[index][2]};
    if(index<uv.size()&&uv[index].size()>=2){vertex.u=uv[index][0];vertex.v=uv[index][1];}
    if(index<color.size())for(unsigned k=0;k<std::min<std::size_t>(4,color[index].size());++k)vertex.color[k]=color[index][k];}
   result.triangles.push_back(triangle);
  }
 }
 if(result.triangles.empty()||result.triangles.size()>100000)throw std::runtime_error("Part has no supported geometry or exceeds raster budget");return result;
}
Vec3 normalize(Vec3 v){const auto n=length(v);return n>1.e-8f?v*(1/n):Vec3{0,0,1};}
Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
Bytes raster(const Geometry& mesh){
 const auto view=normalize({1.8f,-2.8f,1.9f}),right=normalize(cross({0,0,1},view)),up=cross(view,right),light=normalize({-.5f,-.8f,1});
 float minx=std::numeric_limits<float>::max(),miny=minx,maxx=-minx,maxy=-minx;
 for(const auto& t:mesh.triangles)for(const auto& v:t.vertices){const float x=dot(v.p,right),y=dot(v.p,up);minx=std::min(minx,x);maxx=std::max(maxx,x);miny=std::min(miny,y);maxy=std::max(maxy,y);}
 const float extent=std::max(maxx-minx,maxy-miny);if(!std::isfinite(extent)||extent<=1.e-6f)throw std::runtime_error("Degenerate part thumbnail");
 const float scale=float(Size-14)/extent,cx=(minx+maxx)*.5f,cy=(miny+maxy)*.5f;
 Bytes rgba(Size*Size*4);std::vector<float> depth(Size*Size,-std::numeric_limits<float>::infinity());
 auto edge=[](float ax,float ay,float bx,float by,float px,float py){return (px-ax)*(by-ay)-(py-ay)*(bx-ax);};
 for(const auto& triangle:mesh.triangles){std::array<float,3>x{},y{},z{};
  for(unsigned i=0;i<3;++i){const auto& v=triangle.vertices[i];x[i]=(dot(v.p,right)-cx)*scale+Size*.5f;y[i]=Size*.5f-(dot(v.p,up)-cy)*scale;z[i]=dot(v.p,view);}
  const float area=edge(x[0],y[0],x[1],y[1],x[2],y[2]);if(std::abs(area)<.00001f)continue;
  const int left=std::clamp(int(std::floor(*std::min_element(x.begin(),x.end()))),0,int(Size)-1),top=std::clamp(int(std::floor(*std::min_element(y.begin(),y.end()))),0,int(Size)-1);
  const int endx=std::clamp(int(std::ceil(*std::max_element(x.begin(),x.end()))),0,int(Size)-1),endy=std::clamp(int(std::ceil(*std::max_element(y.begin(),y.end()))),0,int(Size)-1);
  for(int py=top;py<=endy;++py)for(int px=left;px<=endx;++px){const float a=edge(x[1],y[1],x[2],y[2],px+.5f,py+.5f)/area,b=edge(x[2],y[2],x[0],y[0],px+.5f,py+.5f)/area,c=1-a-b;
   if(a<0||b<0||c<0)continue;const auto index=unsigned(py)*Size+unsigned(px);const float distance=a*z[0]+b*z[1]+c*z[2];if(distance<=depth[index])continue;
   const auto& v=triangle.vertices;std::array<float,4> color{};
   for(unsigned k=0;k<4;++k)color[k]=(a*v[0].color[k]+b*v[1].color[k]+c*v[2].color[k])*triangle.color[k];
   if(triangle.image>=0&&std::size_t(triangle.image)<mesh.images.size()){
    const auto& image=mesh.images[std::size_t(triangle.image)];const float u=a*v[0].u+b*v[1].u+c*v[2].u,t=a*v[0].v+b*v[1].v+c*v[2].v;
    const auto tx=std::min(image.width-1,unsigned((u-std::floor(u))*float(image.width))),ty=std::min(image.height-1,unsigned((t-std::floor(t))*float(image.height)));
    for(unsigned k=0;k<4;++k)color[k]*=float(image.rgba[(ty*image.width+tx)*4+k])/255.f;
   }
   if(color[3]<triangle.cutoff)continue;const auto normal=normalize(v[0].normal*a+v[1].normal*b+v[2].normal*c);const float shade=.55f+.45f*std::max(0.f,dot(normal,light));
   for(unsigned k=0;k<3;++k)rgba[index*4+k]=std::uint8_t(std::clamp(color[k]*shade*255.f,0.f,255.f));rgba[index*4+3]=255;depth[index]=distance;
  }
 }
 return rgba;
}
constexpr std::uint64_t CacheMagic=0x31544d4854535255ull;
bool read_cache(const fs::path& path,std::uint64_t expected,ThumbnailBitmap& out){
 try{
  const auto raw=read_file(path,32+Size*Size+1024);if(raw.size()!=32+Size*Size+1024)return false;
  std::uint64_t header[4];std::memcpy(header,raw.data(),sizeof header);
  if(header[0]!=CacheMagic||header[1]!=expected||header[2]!=hash(raw.data()+32,Size*Size)||header[3]!=hash(raw.data()+32+Size*Size,1024)||raw[32+Size*Size+3]!=0)return false;
  for(unsigned i=3;i<1024;i+=4)if(raw[32+Size*Size+i]>128)return false;
  std::memcpy(out.pixels.data(),raw.data()+32,Size*Size);std::memcpy(out.palette.data(),raw.data()+32+Size*Size,1024);out.source_hash=expected;return true;
 }catch(...){return false;}
}
fs::path default_levels(const openrac_api* api){
#ifdef _WIN32
 int argc{};auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);std::vector<fs::path> arguments;
 if(argv){try{for(int i=1;i<argc;++i)arguments.emplace_back(argv[i]);}catch(...){LocalFree(argv);throw;}LocalFree(argv);}
 fs::path data;const DWORD size=GetEnvironmentVariableW(L"OPENRAC_DATA",nullptr,0);
 if(size){std::vector<wchar_t> buffer(size);const DWORD count=GetEnvironmentVariableW(L"OPENRAC_DATA",buffer.data(),size);if(count&&count<size)data=std::wstring(buffer.data(),count);}
 return thumbnail_levels(arguments,api&&api->version?api->version:"",data);
#else
 (void)api;
 return {};
#endif
}
}
fs::path thumbnail_levels(const std::vector<fs::path>& arguments,std::string_view version,const fs::path& environment_data){
 fs::path levels,data;
 for(std::size_t i=0;i<arguments.size();++i){
  if(arguments[i]=="--levels"&&i+1<arguments.size())levels=arguments[++i];
  else if(arguments[i]=="--data"&&i+1<arguments.size())data=arguments[++i];
 }
 if(levels.empty()){
  if(data.empty())data=environment_data;
  if(data.empty()||(version!="pal"&&version!="ntsc"))return {};
  levels=data/"port"/version;
 }
 // options.cpp::levels_or_flat: PAL exports predating the per-version layout
 // remain in the parent until the player extracts them again.
 std::error_code ec;const auto flat=levels.parent_path();
 if(version=="pal"&&levels.filename()=="pal"
    &&!fs::exists(levels/"level_99"/"manifest.json",ec)
    &&fs::exists(flat/"level_99"/"manifest.json",ec))return flat;
 return levels;
}
bool render_thumbnail(const fs::path& glb,ThumbnailBitmap& out,std::string& error){
 try{
#ifdef _WIN32
  ComScope com;
#endif
  const auto model=geometry(load_glb(glb));ThumbnailBitmap result;result.source_hash=model.source_hash;indexed(raster(model),result);out=result;error.clear();return true;
 }catch(const std::exception& e){error=e.what();return false;}
}
bool write_thumbnail_png(const fs::path& path,const ThumbnailBitmap& bitmap,std::string& error){
 try{
#ifdef _WIN32
  ComScope com;Bytes rgba(Size*Size*4);for(unsigned i=0;i<bitmap.pixels.size();++i){const unsigned n=bitmap.pixels[i],p=(n&~0x18u)|((n&8u)<<1)|((n&0x10u)>>1);
   for(unsigned k=0;k<3;++k)rgba[i*4+k]=bitmap.palette[p*4+k];rgba[i*4+3]=std::uint8_t(std::min(255u,unsigned(bitmap.palette[p*4+3])*2));}
  auto factory=imaging();ComPtr<IWICBitmap> image;checked(factory->CreateBitmapFromMemory(Size,Size,GUID_WICPixelFormat32bppRGBA,Size*4,UINT(rgba.size()),rgba.data(),&image),"Cannot create thumbnail output");
  ComPtr<IWICStream> stream;checked(factory->CreateStream(&stream),"Cannot create PNG stream");checked(stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE),"Cannot write thumbnail PNG");
  ComPtr<IWICBitmapEncoder> encoder;checked(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder),"Cannot create PNG encoder");checked(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache),"Cannot initialize PNG encoder");
  ComPtr<IWICBitmapFrameEncode> frame;ComPtr<IPropertyBag2> properties;checked(encoder->CreateNewFrame(&frame,&properties),"Cannot create PNG frame");checked(frame->Initialize(properties.Get()),"Cannot initialize PNG frame");checked(frame->SetSize(Size,Size),"Cannot set PNG size");WICPixelFormatGUID format=GUID_WICPixelFormat32bppRGBA;checked(frame->SetPixelFormat(&format),"Cannot set PNG format");
  ComPtr<IWICFormatConverter> convert;checked(factory->CreateFormatConverter(&convert),"Cannot create PNG converter");checked(convert->Initialize(image.Get(),format,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom),"Cannot convert PNG output");checked(frame->WriteSource(convert.Get(),nullptr),"Cannot write PNG pixels");checked(frame->Commit(),"Cannot commit PNG frame");checked(encoder->Commit(),"Cannot commit PNG output");error.clear();return true;
#else
  (void)path;(void)bitmap;error="PNG output requires Windows";return false;
#endif
 }catch(const std::exception& e){error=e.what();return false;}
}
struct NativeThumbnails::State:NativeMemory {
 GuestArena& arena;fs::path levels,cache;std::string error;std::map<PartId,ThumbnailBitmap> bitmaps;
 unsigned storage{},capacity{};int level=-1;
 State(const openrac_api* api,GuestArena& a,fs::path l,fs::path c):NativeMemory(api),arena(a),levels(std::move(l)),cache(std::move(c)){}
 const ThumbnailBitmap* bitmap(const PartDescriptor& part){
  const auto found=bitmaps.find(part.id);if(found!=bitmaps.end())return &found->second;
  if(levels.empty()){error="No content root: pass --levels, --data or OPENRAC_DATA";return nullptr;}
  fs::path mesh;
  for(int n=0;n<=18&&mesh.empty();++n){char folder[16];std::snprintf(folder,sizeof folder,"level_%02d",n);const auto root=levels/folder;
   Value manifest;std::string parse_error;if(!openrac::json::parse_file((root/"manifest.json").string(),manifest,parse_error))continue;
   const auto& classes=manifest["classes"]["moby"];if(!classes[std::to_string(part.source_class)].is_object())continue;
   const auto file=classes[std::to_string(part.fragment_class)]["mesh"].string();if(file.empty())continue;
   const auto candidate=(root/utf8_path(file)).lexically_normal();const auto relative=candidate.lexically_relative(root.lexically_normal());
   if(relative.empty()||relative.is_absolute()||*relative.begin()=="..")continue;if(fs::is_regular_file(candidate))mesh=candidate;
  }
  if(mesh.empty()){error="Owned-disc model for this component is unavailable";return nullptr;}
  const auto glb=load_glb(mesh);const auto sources=image_sources(glb);const auto expected=fingerprint(glb,sources);
  const auto target=cache/(std::to_string(part.id)+"-"+std::to_string(expected)+".rst");ThumbnailBitmap image;
  if(!cache.empty()&&read_cache(target,expected,image))return &bitmaps.emplace(part.id,std::move(image)).first->second;
  if(!render_thumbnail(mesh,image,error))return nullptr;
  if(!cache.empty()){std::error_code code;fs::create_directories(cache,code);if(!code){auto pending=target;pending+=".pending";
    std::ofstream file(pending,std::ios::binary|std::ios::trunc);if(file){const std::uint64_t header[4]{CacheMagic,image.source_hash,hash(image.pixels.data(),image.pixels.size()),hash(image.palette.data(),image.palette.size())};
     file.write(reinterpret_cast<const char*>(header),sizeof header);file.write(reinterpret_cast<const char*>(image.pixels.data()),image.pixels.size());file.write(reinterpret_cast<const char*>(image.palette.data()),image.palette.size());file.flush();const bool good=bool(file);file.close();
     if(good){fs::rename(pending,target,code);if(code&&fs::exists(target)){fs::remove(target,code);code.clear();fs::rename(pending,target,code);}}}}}
  return &bitmaps.emplace(part.id,std::move(image)).first->second;
 }
 bool draw(unsigned screen,const PartDescriptor& part){
  if(!api_||!api_->memory||!api_->loaded_level||!valid(screen,0x28)||!arena.begin())return false;
  const int current=api_->loaded_level();if(current!=level){storage=capacity=0;level=current;}
  const int width=read<int>(screen+0x20),height=read<int>(screen+0x24);if(width<16||height<16||width>2048||height>2048)return false;
  const auto* image=bitmap(part);if(!image)return false;
  const unsigned state=g(0x19a4e8),defs=read<unsigned>(state+0x1c),refs=read<unsigned>(state+0x20),header=read<unsigned>(state+0x18),images=read<unsigned>(state+0x24),palettes=read<unsigned>(state+0x28);
  constexpr unsigned HeaderBytes=45*4;unsigned frames=0;bool terminated=false;
  if(!valid(header,HeaderBytes)||!valid(defs,8))return false;
  for(unsigned i=0;i<1024;++i){const auto at=defs+i*8;if(!valid(at,8))return false;if(read<unsigned short>(at)==0xffff){terminated=true;break;}
   frames=std::max(frames,unsigned(read<unsigned short>(at+4))+read<unsigned short>(at+2));}
  if(!terminated||!frames||frames>16000||!valid(refs,frames*4))return false;
  unsigned image_count=read<unsigned>(header+0x44),palette_count=read<unsigned>(header+0x24);
  for(unsigned i=0;i<frames;++i){const auto value=read<unsigned>(refs+i*4);image_count=std::max(image_count,(value>>16)+1);palette_count=std::max(palette_count,(value&0xffffu)+1);}
  if(image_count>8190||palette_count>8191||!valid(images,image_count*8)||!valid(palettes,palette_count*8))return false;
  const unsigned image_bytes=(image_count+1)*8,palette_bytes=(palette_count+1)*8,refs_bytes=(frames+1)*4;
  const unsigned pixels_offset=(image_bytes+palette_bytes+refs_bytes+HeaderBytes+63)&~63u,required=pixels_offset+Size*Size+1024;
  if(!arena.owns(storage,required)||capacity<required){storage=arena.allocate(required);capacity=storage?required:0;}if(!storage)return false;
  const auto image_copy=storage,palette_copy=image_copy+image_bytes,refs_copy=palette_copy+palette_bytes,header_copy=refs_copy+refs_bytes,pixels=storage+pixels_offset,clut=pixels+Size*Size;
  std::memcpy(api_->memory+image_copy,api_->memory+images,image_count*8);std::memcpy(api_->memory+palette_copy,api_->memory+palettes,palette_count*8);std::memcpy(api_->memory+refs_copy,api_->memory+refs,frames*4);std::memcpy(api_->memory+header_copy,api_->memory+header,HeaderBytes);
  std::memcpy(api_->memory+pixels,image->pixels.data(),image->pixels.size());std::memcpy(api_->memory+clut,image->palette.data(),image->palette.size());
  write<unsigned>(header_copy+0x44,image_count+1);write<unsigned>(header_copy+0x24,palette_count+1);
  const auto own_image=image_copy+image_count*8,own_palette=palette_copy+palette_count*8;write(own_image,pixels);write<unsigned short>(own_image+4,0);write<unsigned char>(own_image+6,7);write<unsigned char>(own_image+7,7);write(own_palette,clut);write<unsigned>(own_palette+4,0);write(refs_copy+frames*4,(image_count<<16)|palette_count);
  using Begin=void(*)(int);using End=void(*)();using Draw=void(*)(int,int,int,int,int,int);
  const auto begin=reinterpret_cast<Begin>(api_->function("func_001F4630"));const auto end=reinterpret_cast<End>(api_->function("func_001F4748"));const auto draw=reinterpret_cast<Draw>(api_->function("func_00200468"));if(!begin||!end||!draw)return false;
  write(state+0x18,header_copy);write(state+0x20,refs_copy);write(state+0x24,image_copy);write(state+0x28,palette_copy);
  struct Restore {State& s;unsigned state,header,refs,images,palettes,own_header,own_refs,own_images,own_palettes;
   bool owns()const{return s.read<unsigned>(state+0x18)==own_header&&s.read<unsigned>(state+0x20)==own_refs&&s.read<unsigned>(state+0x24)==own_images&&s.read<unsigned>(state+0x28)==own_palettes;}
   ~Restore(){if(owns()){s.write(state+0x18,header);s.write(state+0x20,refs);s.write(state+0x24,images);s.write(state+0x28,palettes);}}} restore{*this,state,header,refs,images,palettes,header_copy,refs_copy,image_copy,palette_copy};
  begin(0);const int size=std::max(8,std::min(width-12,height-12));draw(int(frames),(width-size)/2,(height-size)/2,size,size,128);end();
  if(!restore.owns()){error="Another texture owner changed the pane during thumbnail drawing";return false;}
  for(unsigned i=0;i<image_count;++i)write<unsigned short>(images+i*8+4,read<unsigned short>(image_copy+i*8+4));
  for(unsigned i=0;i<palette_count;++i)write<unsigned short>(palettes+i*8+4,read<unsigned short>(palette_copy+i*8+4));
  for(unsigned i=0;i<45;++i)if(i!=0x24/4&&i!=0x44/4)write<unsigned>(header+i*4,read<unsigned>(header_copy+i*4));
  error.clear();return true;
 }
};
NativeThumbnails::NativeThumbnails(const openrac_api* api,GuestArena& arena,fs::path cache):NativeThumbnails(api,arena,default_levels(api),std::move(cache)){}
NativeThumbnails::NativeThumbnails(const openrac_api* api,GuestArena& arena,fs::path levels,fs::path cache):state_(std::make_unique<State>(api,arena,std::move(levels),std::move(cache))){}
NativeThumbnails::~NativeThumbnails()=default;
bool NativeThumbnails::draw(unsigned screen,const PartDescriptor& part){try{return state_->draw(screen,part);}catch(const std::exception& e){state_->error=e.what();return false;}}
void NativeThumbnails::reset(){state_->storage=state_->capacity=0;state_->level=-1;}
const std::string& NativeThumbnails::last_error()const noexcept{return state_->error;}
}
