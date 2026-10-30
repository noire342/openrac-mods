// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "localization.hpp"
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace revolverator::localization;
#define CHECK(expression) do{if(!(expression)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#expression);std::exit(1);}}while(0)

static_assert(native_language_address==0x15ed88u);
static_assert(language_from_native(0)==Language::English);
static_assert(language_from_native(1)==Language::English);
static_assert(language_from_native(2)==Language::French);
static_assert(language_from_native(3)==Language::German);
static_assert(language_from_native(4)==Language::Spanish);
static_assert(language_from_native(5)==Language::Italian);
static_assert(language_from_native(-1)==Language::English);
static_assert(language_from_native(6)==Language::English);
static_assert(language_from_native(INT_MAX)==Language::English);

namespace {
bool ascii(const char* text){
 if(!text||!*text)return false;
 for(const unsigned char* c=reinterpret_cast<const unsigned char*>(text);*c;++c)
  if(*c<32||*c>126)return false;
 return true;
}
void pickup_format(const char* format,int count){
 unsigned conversions=0;
 for(const char* c=format;*c;++c)if(*c=='%'){CHECK(c[1]=='d');++c;++conversions;}
 CHECK(conversions==1);
 char message[128]{};
 const int length=std::snprintf(message,sizeof message,format,count);
 CHECK(length>0&&static_cast<unsigned>(length)<sizeof message&&ascii(message));
 CHECK(std::strchr(message,'%')==nullptr);
 char number[16]{};std::snprintf(number,sizeof number,"%d",count);
 CHECK(std::strstr(message,number)!=nullptr);
}
}

int main(){
 constexpr int codes[]{0,2,3,4,5};
 for(unsigned i=0;i<5;++i){
  const auto& strings=strings_for_language(codes[i]);
  CHECK(&strings==&translations[i]);
  for(const char* value:{strings.weapon_name,strings.description,strings.pickup_singular,strings.pickup_plural}){
   CHECK(ascii(value)&&std::strlen(value)<256);
  }
  CHECK(std::strcmp(strings.weapon_name,"Revolverator")==0);
  CHECK(std::strcmp(strings.pickup_singular,strings.pickup_plural)!=0);
  pickup_format(strings.pickup_singular,1);pickup_format(strings.pickup_plural,2);
 }
 for(int fallback:{INT_MIN,-100,-1,1,6,7,100,INT_MAX})
  CHECK(&strings_for_language(fallback)==&translations[0]);
 // Console3 is Spanish, but GAME3 is German: never interchange the two enums.
 CHECK(std::strcmp(strings_for_language(3).pickup_singular,"%d Revolverator-Patrone erhalten")==0);
 CHECK(std::strcmp(strings_for_language(5).pickup_singular,"Hai raccolto %d cartuccia per il Revolverator")==0);
 std::puts("localization: native language columns, English fallback, ASCII bounds, singular/plural and native integer formats passed");
}
