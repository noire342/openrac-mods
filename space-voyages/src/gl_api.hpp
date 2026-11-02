// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <stdexcept>
#include <string>
namespace spacevoyages {
// Deliberately limited to the native renderer's common OpenGL/Vulkan facade.
// No platform GL library is linked, and no Vulkan backend is bypassed.
#define SPACE_GL_FUNCTIONS(X) \
 X(void,GenVertexArrays,(int,unsigned*)) X(void,DeleteVertexArrays,(int,const unsigned*)) \
 X(void,BindVertexArray,(unsigned)) X(void,GenBuffers,(int,unsigned*)) \
 X(void,DeleteBuffers,(int,const unsigned*)) X(void,BindBuffer,(unsigned,unsigned)) \
 X(void,BufferData,(unsigned,std::ptrdiff_t,const void*,unsigned)) \
 X(void,EnableVertexAttribArray,(unsigned)) X(void,VertexAttribPointer,(unsigned,int,unsigned,unsigned char,int,const void*)) \
 X(void,VertexAttribDivisor,(unsigned,unsigned)) \
 X(unsigned,CreateShader,(unsigned)) X(void,ShaderSource,(unsigned,int,const char*const*,const int*)) \
 X(void,CompileShader,(unsigned)) X(void,GetShaderiv,(unsigned,unsigned,int*)) \
 X(void,GetShaderInfoLog,(unsigned,int,int*,char*)) X(void,DeleteShader,(unsigned)) \
 X(unsigned,CreateProgram,()) X(void,AttachShader,(unsigned,unsigned)) X(void,LinkProgram,(unsigned)) \
 X(void,GetProgramiv,(unsigned,unsigned,int*)) X(void,GetProgramInfoLog,(unsigned,int,int*,char*)) \
 X(void,DeleteProgram,(unsigned)) X(void,UseProgram,(unsigned)) X(int,GetUniformLocation,(unsigned,const char*)) \
 X(void,UniformMatrix4fv,(int,int,unsigned char,const float*)) X(void,Uniform1i,(int,int)) \
 X(void,Uniform1f,(int,float)) X(void,Uniform4fv,(int,int,const float*)) \
 X(void,GenTextures,(int,unsigned*)) X(void,DeleteTextures,(int,const unsigned*)) \
 X(void,BindTexture,(unsigned,unsigned)) X(void,TexParameteri,(unsigned,unsigned,int)) \
 X(void,TexImage2D,(unsigned,int,int,int,int,int,unsigned,unsigned,const void*)) \
 X(void,TexSubImage2D,(unsigned,int,int,int,int,int,unsigned,unsigned,const void*)) \
 X(void,ActiveTexture,(unsigned)) X(void,Enable,(unsigned)) X(void,Disable,(unsigned)) \
 X(void,DepthMask,(unsigned char)) X(void,DepthFunc,(unsigned)) X(void,BlendEquation,(unsigned)) X(void,BlendFuncSeparate,(unsigned,unsigned,unsigned,unsigned)) \
 X(void,DrawElements,(unsigned,int,unsigned,const void*)) X(void,DrawArrays,(unsigned,int,int)) \
 X(void,DrawElementsInstanced,(unsigned,int,unsigned,const void*,int))
struct GlApi {
 using GetProc=void*(*)(const char*);
#define SPACE_GL_DECLARE(result,name,args) result(*name)args{};
 SPACE_GL_FUNCTIONS(SPACE_GL_DECLARE)
#undef SPACE_GL_DECLARE
 void load(GetProc get){
  if(!get)throw std::runtime_error("Transient graphics facade is unavailable");
#define SPACE_GL_LOAD(result,name,args) name=reinterpret_cast<result(*)args>(get("gl" #name));if(!name)throw std::runtime_error("Missing transient graphics entry gl" #name);
  SPACE_GL_FUNCTIONS(SPACE_GL_LOAD)
#undef SPACE_GL_LOAD
 }
};
}
