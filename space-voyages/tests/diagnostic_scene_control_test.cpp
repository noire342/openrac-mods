// SPDX-License-Identifier: GPL-3.0-or-later
// Pure CPU QA-control tests. Never starts a game, reads saves or opens a UI.
#include "diagnostic_scene_control.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
using spacevoyages::DiagnosticSceneControl;
namespace {
unsigned checks{},reads{};const char* trip_value="2";const char* tick_value="180";
void require(bool pass,const char* why){++checks;if(!pass){std::fprintf(stderr,"diagnostic-scene-control: %s\n",why);std::exit(1);}}
const char* environment(const char* name){++reads;
 if(!std::strcmp(name,DiagnosticSceneControl::trip_variable))return trip_value;
 if(!std::strcmp(name,DiagnosticSceneControl::tick_variable))return tick_value;
 require(false,"only the two documented QA variables are read");return nullptr;
}
}
int main(){
 using Control=DiagnosticSceneControl;
 for(const char* invalid:{static_cast<const char*>(nullptr),"","0","-1","+1"," 2","2 ","2x","1.5","2\n","4294967296","99999999999999999999999999"}){
  require(!Control(Control::parse(invalid,"180")).enabled(),"invalid trip disables the complete request");
  require(!Control(Control::parse("2",invalid)).enabled(),"invalid tick disables the complete request");
 }
 require(!Control(Control::parse("65","1")).enabled(),"trip bound is enforced");
 require(!Control(Control::parse("1","36001")).enabled(),"tick bound is enforced");
 require(Control(Control::parse("64","36000")).enabled(),"maximum bounds accepted without truncation");
 require(Control(Control::parse("0002","00180")).request().tick==180,"decimal leading zeroes are unambiguous");
 require(!Control(Control::parse("00000000002","180")).enabled(),"text is length-bounded even with leading zeroes");
 for(const auto bad:{Control::Cancel{0,180},Control::Cancel{2,0},Control::Cancel{65,1},Control::Cancel{1,36001}})
  require(!Control(bad).enabled(),"direct configuration cannot bypass the parser bounds");
 reads=0;auto inactive=Control::from_environment(false,environment);
 require(!inactive.enabled()&&reads==0,"trace-off does not even inspect the environment");
 for(unsigned trip=0;trip<3;++trip){inactive.begin_scene();for(unsigned tick=0;tick<500;++tick)require(!inactive.cancel_tick(),"disabled control never cancels a lease");}
 reads=0;auto active=Control::from_environment(true,environment);
 require(active.enabled()&&reads==2,"trace-on reads exactly two variables once at construction");
 require(!active.cancel_tick()&&active.scene_count()==0&&active.tick_count()==0,"no lease can be cancelled before real scene entry");
 active.begin_scene();for(unsigned tick=1;tick<=300;++tick)require(!active.cancel_tick(),"the first real scene is never cancelled by a second-trip request");
 active.begin_scene();require(active.scene_count()==2&&active.tick_count()==0,"new real scene resets only its tick counter");
 for(unsigned tick=1;tick<180;++tick)require(!active.cancel_tick(),"second scene remains active before the exact requested tick");
 require(active.cancel_tick()&&active.fired()&&active.tick_count()==180,"second scene cancels exactly on its180th native callback");
 for(unsigned tick=0;tick<300;++tick)require(!active.cancel_tick(),"one request fires only once");
 active.begin_scene();for(unsigned tick=0;tick<500;++tick)require(!active.cancel_tick(),"later scenes cannot replay a consumed request");
 require(reads==2,"scene/tick paths never reread environment or allocate strings");
 auto first=Control(Control::parse("1","1"));first.begin_scene();require(first.cancel_tick()&&!first.cancel_tick(),"minimum boundary fires once on the first native callback");
 auto maximum=Control(Control::parse("64","36000"));
 for(unsigned trip=1;trip<64;++trip){maximum.begin_scene();require(!maximum.cancel_tick(),"maximum trip request ignores earlier real scene entries");}
 maximum.begin_scene();for(unsigned tick=1;tick<36000;++tick)require(!maximum.cancel_tick(),"maximum tick remains exact without integer wrap or truncation");
 require(maximum.cancel_tick()&&!maximum.cancel_tick(),"maximum accepted bounds cancel once at scene64 tick36000");
 trip_value="2";tick_value=nullptr;require(!Control::from_environment(true,environment).enabled(),"missing counterpart leaves control off");
 trip_value=nullptr;tick_value="180";require(!Control::from_environment(true,environment).enabled(),"missing trip leaves control off");
 std::printf("diagnostic-scene-control: %u bounded-parser, trace-gate, real-scene and once-only checks passed; no game/GPU\n",checks);
}
