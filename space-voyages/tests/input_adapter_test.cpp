// SPDX-License-Identifier: GPL-3.0-or-later
#include "input_adapter.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
using namespace spacevoyages;
namespace {
int checks;
void require(bool test,const char* message){++checks;if(!test){std::fprintf(stderr,"input adapter: %s\n",message);std::exit(1);}}
bool near(float a,float b,float epsilon=.00001f){return std::abs(a-b)<epsilon;}
FlightState pulse(std::array<std::uint8_t,4> raw,VerticalInput mode=VerticalInput::NativeAircraft){
    FlightController flight;flight.reset({});
    for(int i=0;i<60;++i)flight.tick(1./60,flight_input_from_pad(raw.data(),0,mode));
    return flight.state();
}
}
int main(){
 require(weapon_fire_from_pad(0x8000)&&weapon_fire_from_pad(0x0400)&&weapon_fire_from_pad(0x8400),"native Square and L1 decode from the SDK report byte order");
 require(!weapon_fire_from_pad(0x84)&&!weapon_fire_from_pad(0x4000)&&!weapon_fire_from_pad(0x2000)&&!weapon_fire_from_pad(0x1000),"D-pad/native-order bits, turbo, Circle and Triangle cannot fire the primary gun");
    require(native_pad_axis(127)==0&&native_pad_axis(128)==0,"native centre127 and controller rest128 are both neutral");
    require(native_pad_axis(79)==0&&native_pad_axis(175)==0,"native deadzone includes its48-step boundary");
    require(near(native_pad_axis(176),1.f/76)&&near(native_pad_axis(78),-1.f/76),"native slope starts one step outside the deadzone");
    require(native_pad_axis(0)==-1&&native_pad_axis(255)==1,"full travel clamps to unit input");
    std::array<std::uint8_t,4> centre{128,128,128,128};
    auto input=flight_input_from_pad(centre.data(),0x4000);
    require(input.horizontal==0&&input.vertical==0&&input.boost,"Cross alone is boost, not a d-pad command");
    auto light=centre;light[2]=176;
    require(flight_input_from_pad(light.data(),0).horizontal==0,"light native stick enters hero near-centre fallback instead of banking the model");
    require(flight_input_from_pad(light.data(),0x20).horizontal==1,"near-centre fallback uses actual right d-pad");
    auto diagonal=centre;diagonal[2]=diagonal[3]=190;
    const auto diagonal_input=flight_input_from_pad(diagonal.data(),0);
    require(diagonal_input.horizontal>0&&diagonal_input.vertical>0,"native radial threshold permits a diagonal outside the hero deadzone");
    require(flight_input_from_pad(centre.data(),0x10).vertical==-1&&flight_input_from_pad(centre.data(),0x40).vertical==1,"frontend d-pad byte order is decoded correctly");
    require(flight_input_from_pad(centre.data(),0x20,VerticalInput::NativeAircraft,true).horizontal==-1,"native mirror swaps horizontal input once");
    const auto right=pulse({128,128,255,128});
    const auto left=pulse({128,128,0,128});
    require(right.position.y<-.5f&&left.position.y>.5f,"right/left stick move toward screen right/left for a rear chase camera (+X forward, right=-Y)");
    require(right.model_roll>0&&left.model_roll<0,"native bank sign follows horizontal steering");
    const auto up=pulse({128,128,128,0});
    const auto down=pulse({128,128,128,255});
    require(up.position.z<-.5f&&down.position.z>.5f,"native Gemlik aircraft control: push up dives, pull down climbs");
    const auto direct_up=pulse({128,128,128,0},VerticalInput::Direct);
    const auto direct_down=pulse({128,128,128,255},VerticalInput::Direct);
    require(direct_up.position.z>.5f&&direct_down.position.z<-.5f,"explicit Direct mode flips only the vertical input boundary");
    // Actual camera coordinates, not just angle signs: a fixed forward marker
    // moves left in the view after right steering, and upward after a dive.
    const Vec3 right_camera_forward=direction(right.camera.yaw,right.camera.pitch);
    const Vec3 right_camera_right=unit(cross(right_camera_forward,{0,0,1}));
    require(dot(Vec3{50,0,0}-right.camera.position,right_camera_right)<0,"right steering moves the previous forward marker to screen left");
    const Vec3 dive_forward=direction(up.camera.yaw,up.camera.pitch);
    const Vec3 dive_right=unit(cross(dive_forward,{0,0,1}));
    const Vec3 dive_camera_up=cross(dive_right,dive_forward);
    require(dot(Vec3{50,0,0}-up.camera.position,dive_camera_up)>0,"native dive moves the previous forward marker upward in the chase view");
    FlightController release;release.reset({});
    const auto steer=flight_input_from_pad(std::array<std::uint8_t,4>{128,128,255,128}.data(),0);
    for(int i=0;i<60;++i)release.tick(1./60,steer);
    const float peak=std::abs(release.state().model_roll);
    for(int i=0;i<60;++i)release.tick(1./60,{});
    const float after_one_second=std::abs(release.state().model_roll);
    for(int i=0;i<60;++i)release.tick(1./60,{});
    require(after_one_second<peak*.35f&&std::abs(release.state().model_roll)<.08f,"bank returns within two seconds using the original springs, without a comfort preset");
    require(std::abs(release.state().camera.roll)<.05f,"camera bank settles along with the hull");
    std::printf("Native input: %d raw-pad, movement, projection and short bank-recovery checks passed\n",checks);
}
