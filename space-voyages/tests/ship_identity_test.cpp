// SPDX-License-Identifier: GPL-3.0-or-later
#include "ship_identity.hpp"
#include "voyage.hpp"
#include "reticle.hpp"
#include <cstdio>
#include <cstdlib>
using namespace spacevoyages;
namespace {unsigned checks{};void require(bool value,const char* why){++checks;if(!value){std::fprintf(stderr,"ship identity: %s\n",why);std::exit(1);}}}
int main(){
    for(unsigned id=0;id<4;++id){const auto identity=ship_identity_from_stable_id(id);require(identity.has_value()&&unsigned(*identity)==id,"stable garage ids are preserved exactly");
        const auto* profile=ship_profile(*identity);require(profile&&profile->slot==(id?std::size_t(id-1):3)&&profile->native_carrier_class==(id?530+int(id):531),"one selector preserves stock slots and appends Original3 with carrier531");
        require(profile->weapons_allowed==(id>=2),"Original and Green are always disarmed capabilities");
        require(profile->boarding_entry==(id?boarding::EntryPresentation::OmitConsole:boarding::EntryPresentation::PreserveEntry),"Original preserves its visible climb while all three stock ships retain console omission");}
    for(int cls:{0,18,530,534,2047,-1})require(!ship_identity_from_native_class(cls),"legacy native-class conversion never invents an Original or clamps an unknown class");
    for(unsigned id:{4u,255u,~0u})require(!ship_identity_from_stable_id(id),"unknown stable identities are rejected");
    require(!ship_profile(static_cast<ShipIdentity>(255)),"invalid typed identity has no profile");
    require(compatible_boarding_actor(ShipIdentity::Original,2047)&&compatible_boarding_actor(ShipIdentity::Original,531),"verified Original accepts its persistent world actor and temporary native carrier");
    require(!compatible_boarding_actor(ShipIdentity::Green,2047)&&!compatible_boarding_actor(ShipIdentity::Original,533),"visible actor and carrier classes do not cross ship identities");
    require(ship_identity_from_native_class(531)==ShipIdentity::Green&&ship_identity_from_native_class(532)==ShipIdentity::Red&&ship_identity_from_native_class(533)==ShipIdentity::Purple,"legacy stock class adapter remains explicit and unchanged");
    SpaceCombatAssets malicious;for(auto& weapon:malicious.ships){weapon.armed=true;weapon.count=1;}
    SpaceCombat combat;require(combat.set_assets(malicious),"synthetic weapon data is geometrically valid");combat.set_world({},nullptr,0);FlightState ship;ship.speed=0;
    for(auto identity:{ShipIdentity::Original,ShipIdentity::Green}){combat.reset(identity);for(int i=0;i<600;++i)combat.step(ship,true);require(!combat.state().armed&&!combat.active_weapon()&&combat.state().shots==0,"forbidden ship cannot fire even with malicious armed metadata");
        const render_math::Matrix identity_matrix{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};require(!weapon_reticle(identity_matrix,ship,malicious,identity,416).visible,"unarmed capability cannot project a mirino");}
    for(auto identity:{ShipIdentity::Red,ShipIdentity::Purple}){combat.reset(identity);for(int i=0;i<60;++i)combat.step(ship,true);require(combat.state().armed&&combat.state().shots==15,"both stock armed identities retain native15Hz cadence");}
    for(int cls:{534,2047,0}){combat.reset(cls);combat.step(ship,true);require(!combat.state().armed&&!combat.state().shots,"invalid legacy reset stays disarmed without a fake ship identity");}
    std::array<bool,19> flags{};flags.fill(true);VoyageController original,green;
    require(original.begin(3,flags,ShipIdentity::Original)&&green.begin(3,flags,ShipIdentity::Green)&&original.begin_space_entry()&&green.begin_space_entry(),"explicit identity begins an ordinary pure-core journey");
    for(int i=0;i<2400;++i){const FlightInput input{float((i/120)%3-1)*.7f,float((i/90)%3-1)*.4f,(i/150)%2!=0};const bool paused=i>1200&&i<1300;
        original.tick(1./60,input,false,paused,true);green.tick(1./60,input,false,paused,true);
        const auto& a=original.flight();const auto& b=green.flight();require(length_squared(a.position-b.position)==0&&a.yaw==b.yaw&&a.pitch==b.pitch&&a.model_roll==b.model_roll&&length_squared(a.camera.position-b.camera.position)==0,"appearance identity changes no native flight/camera/boost/geometry response");
        require(original.ship_identity()==ShipIdentity::Original&&!original.combat().state().shots,"Original identity survives pause and flight without laser production");}
    original.cancel();for(int cls:{0,534,2047})require(!original.begin(3,flags,cls)&&original.phase()==Phase::Ground,"legacy Voyage begin accepts only stock531..533");
    require(!original.begin(3,flags,static_cast<ShipIdentity>(255)),"invalid explicit identity cannot start flight");
    std::printf("ship_identity: %u mapping, disarmed policy, legacy rejection and bit-identical Gemlik checks passed\n",checks);
}
