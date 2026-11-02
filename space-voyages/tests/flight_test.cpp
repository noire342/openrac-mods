// SPDX-License-Identifier: GPL-3.0-or-later
#include "flight.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace spacevoyages;
namespace {
int checks;
void require(bool test,const char* message){++checks;if(!test){std::fprintf(stderr,"flight: %s\n",message);std::exit(1);}}
bool near(float a,float b,float eps=.0001f){return std::abs(a-b)<eps;}
}
int main(){
    FlightController flight;
    flight.reset({});
    // Native Gemlik at60Hz: displacement .4; camera offset(-3.75,0,1),
    // camera elevation+.09817477, cruiseFOV65degrees.
    require(near(flight.state().speed,24)&&near(flight.state().camera.position.x,-3.75f),"native cruise and chase offset");
    require(near(flight.state().camera.position.z,1)&&near(flight.state().camera.pitch,.09817477f),"native chase height and pitch offset");
    flight.tick(1./60,{});
    require(near(flight.state().position.x,.4f)&&near(flight.state().position.y,0),"one native neutral tick advances exactly0.4units");
    flight.reset({});flight.tick(1./60,{0,0,true});
    require(near(flight.state().speed,26.1f)&&near(flight.state().position.x,.435f),"Gemlik first boost tick golden value");
    flight.tick(1./60,{0,0,true});require(near(flight.state().speed,28.2f),"Gemlik second boost tick golden value");
    flight.tick(1./60,{0,0,true});require(near(flight.state().speed,30)&&near(flight.state().position.x,1.405f),"boost cap and displacement match native golden values");
    flight.tick(1./60,{});require(near(flight.state().speed,29.4f),"release uses native ten-percent speed decay");
    flight.reset({});flight.tick(1./60,{1,1,false});
    require(near(flight.state().yaw,-.00036815541f)&&near(flight.state().pitch,-.00036815541f),"first stick step preserves native spring and angular gain");
    require(near(flight.state().roll,.0015f)&&near(flight.state().model_roll,.0165f),"native visual banking golden value");
    require(near(flight.state().model_pitch,-.01536815541f),"native ship pitch includes stick lean");
    // Same elapsed time sampled at144/60/30Hz produces one60Hz trajectory.
    FlightController f30,f60,f144;f30.reset({});f60.reset({});f144.reset({});
    const FlightInput held{.35f,-.2f,true};
    for(int i=0;i<30*12;++i)f30.tick(1./30,held);
    for(int i=0;i<60*12;++i)f60.tick(1./60,held);
    for(int i=0;i<144*12;++i)f144.tick(1./144,held);
    require(f30.state().ticks==720&&f144.state().ticks==720,"native tick count independent of rendering");
    require(length(f30.state().position-f60.state().position)<.0001f&&length(f144.state().position-f60.state().position)<.0001f,"movement is render-rate independent");
    require(near(f30.state().camera.yaw,f144.state().camera.yaw)&&length(f30.state().camera.position-f144.state().camera.position)<.0001f,"camera smoothing is render-rate independent");
    flight.reset({});
    for(int i=0;i<600;++i)flight.tick(1./60,{1,1,false});
    require(near(flight.state().pitch,-FlightController::pitch_limit),"native eighty-degree pitch bound");
    require(std::abs(flight.state().yaw)<=pi&&std::abs(flight.state().camera.yaw)<=pi,"angles remain wrapped acrossmultipleturns");
    for(int i=0;i<600;++i)flight.tick(1./60,{});
    require(std::abs(flight.state().model_roll)<.001f&&std::abs(flight.state().camera.roll)<.001f,"ship and camera recover from roll afterrelease");
    flight.reset({});flight.tick(10.,{});
    require(flight.state().ticks==15&&flight.state().position.x<=6.001f,"long process stall is capped instead of teleporting");
    const auto before=flight.state();
    flight.tick(-1.,{});flight.tick(std::numeric_limits<double>::quiet_NaN(),{});
    require(length(flight.state().position-before.position)==0,"invalidclock cannot advance simulation");
    flight.tick(1./60,{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),false});
    require(finite(flight.state().position)&&finite(flight.state().camera.position),"invalid stick is neutral and cannot poison world");
    flight.reset({1,2,3},pi-.001f);flight.tick(1./60,{-1,0,true});
    require(std::isfinite(flight.state().yaw)&&std::abs(flight.state().yaw)<=pi,"turning throughpiiscontinuous");
    {
        FlightController clocked,dedicated;clocked.reset({});dedicated.reset({});unsigned callbacks=0;
        const auto no_change=+[](void* user,FlightController&,Vec3){++*static_cast<unsigned*>(user);};
        for(int frame=0;frame<720;++frame){const FlightInput input{frame<180?.7f:frame<360?0.f:-.4f,frame<360?-.3f:.2f,frame<240};
            clocked.tick(1./60,input);dedicated.native_tick(input,no_change,&callbacks);
            const auto& a=clocked.state();const auto& b=dedicated.state();
            require(length(a.position-b.position)==0&&a.yaw==b.yaw&&a.pitch==b.pitch&&a.roll==b.roll&&a.model_yaw==b.model_yaw&&a.model_pitch==b.model_pitch&&a.model_roll==b.model_roll&&a.speed==b.speed,"explicit constrained-step boundary preserves default native integration exactly when geometry is unchanged");
            require(length(a.camera.position-b.camera.position)==0&&a.camera.yaw==b.camera.yaw&&a.camera.pitch==b.camera.pitch&&a.camera.roll==b.camera.roll&&a.camera.fov==b.camera.fov,"no-op geometry does not add a second camera/FOV spring step");
        }
        require(callbacks==720&&clocked.state().ticks==dedicated.state().ticks,"a native transaction invokes geometry once per simulation tick");
    }
    {
        FlightController ordinary,corrected;ordinary.reset({});corrected.reset({});
        const auto bend=+[](void*,FlightController& controller,Vec3){controller.constrain(controller.state().position,direction(pi*.5f,.4f));};
        ordinary.tick(1./60,{0,0,true});corrected.native_tick({0,0,true},bend);
        const auto& a=ordinary.state();const auto& b=corrected.state();
        require(b.model_yaw==b.yaw&&b.model_pitch==b.pitch&&b.yaw>0&&b.pitch>0,"hull pose is published from this tick's corrected physical heading");
        require(near(b.camera.yaw,b.yaw*.15f,.000001f)&&near(b.camera.pitch,.09817477f+b.pitch*.15f,.000001f),"camera springs consume the corrected heading once with original native gain");
        const auto forward=direction(b.yaw,b.pitch),left=unit(cross({0,0,1},forward)),up=cross(forward,left);
        const auto expected=b.position+forward*(a.camera.position.x-a.position.x)+up;
        require(length(b.camera.position-expected)<.00001f,"camera offset basis is recomputed after geometry instead of remaining one frame behind");
        require(b.camera.fov==a.camera.fov&&b.speed==a.speed&&b.ticks==1,"geometry finalization leaves the native boost/FOV easing step unchanged");
        const auto& visible=corrected.presentation_state();
        require(length(visible.position-b.position)==0&&visible.yaw==b.yaw&&visible.pitch==b.pitch&&visible.speed==b.speed,"continuous presentation consumes the same post-constraint physical pose");
    }
    {
        FlightController rendered;rendered.reset({});rendered.tick(1./60,{1,1,true});
        const auto& native=rendered.state();const auto& visible=rendered.presentation_state();
        require(visible.model_roll==native.model_roll&&visible.model_pitch==native.model_pitch,"continuous presentation keeps native initial lean gains without an extra smoothing stage");
        require(near(visible.camera.yaw,native.yaw*.15f,.0000001f),"continuous camera keeps its native first spring step instead of snapping a sub-threshold residual");
        require(visible.camera.fov==native.camera.fov,"continuous presentation retains the original first boost/FOV gain");
    }
    {
        // Short/long turns and boost transitions exercise the visible final
        // settle, rather than just checking that another interpolator exists.
        unsigned bank_cases=0,camera_cases=0,fov_cases=0;
        for(int axis_index=0;axis_index<2;++axis_index)for(int held_ticks:{6,30,180})for(int boost=0;boost<3;++boost){
            FlightController rendered;rendered.reset({});float prior_native_camera=0,prior_visible_camera=0,prior_native_bank=0,prior_visible_bank=0,prior_native_fov=0,prior_visible_fov=0;
            float native_camera_peak=0,visible_camera_peak=0,native_bank_peak=0,visible_bank_peak=0,native_fov_peak=0,visible_fov_peak=0;
            for(int tick=0;tick<held_ticks+300;++tick){
                const auto previous=rendered.state(),previous_visible=rendered.presentation_state();
                rendered.tick(1./60,{tick<held_ticks&&axis_index==0?1.f:0.f,tick<held_ticks&&axis_index==1?1.f:0.f,boost==1||(boost==2&&tick<held_ticks)});
                const auto& n=rendered.state();const auto& v=rendered.presentation_state();
                require(length(n.position-v.position)==0&&n.yaw==v.yaw&&n.pitch==v.pitch&&n.speed==v.speed&&n.boost_fraction==v.boost_fraction&&n.ticks==v.ticks,"render companion never changes trajectory, control heading, speed or simulation timing");
                require(finite(v.camera.position)&&std::isfinite(v.camera.yaw)&&std::isfinite(v.camera.pitch)&&std::isfinite(v.camera.roll)&&std::isfinite(v.camera.fov)&&std::isfinite(v.model_roll)&&std::isfinite(v.model_pitch),"continuous render pose remains finite throughout turns and boost transitions");
                const float native_camera=wrap_angle((axis_index==0?n.camera.yaw:n.camera.pitch)-(axis_index==0?previous.camera.yaw:previous.camera.pitch));
                const float visible_camera=wrap_angle((axis_index==0?v.camera.yaw:v.camera.pitch)-(axis_index==0?previous_visible.camera.yaw:previous_visible.camera.pitch));
                const float native_bank=wrap_angle(n.model_roll-previous.model_roll),visible_bank=wrap_angle(v.model_roll-previous_visible.model_roll);
                const float native_fov=n.camera.fov-previous.camera.fov,visible_fov=v.camera.fov-previous_visible.camera.fov;
                if(tick>held_ticks+30){
                    native_camera_peak=std::max(native_camera_peak,std::abs(native_camera-prior_native_camera));visible_camera_peak=std::max(visible_camera_peak,std::abs(visible_camera-prior_visible_camera));
                    native_bank_peak=std::max(native_bank_peak,std::abs(native_bank-prior_native_bank));visible_bank_peak=std::max(visible_bank_peak,std::abs(visible_bank-prior_visible_bank));
                    native_fov_peak=std::max(native_fov_peak,std::abs(native_fov-prior_native_fov));visible_fov_peak=std::max(visible_fov_peak,std::abs(visible_fov-prior_visible_fov));
                }
                prior_native_camera=native_camera;prior_visible_camera=visible_camera;prior_native_bank=native_bank;prior_visible_bank=visible_bank;prior_native_fov=native_fov;prior_visible_fov=visible_fov;
            }
            if(native_bank_peak>.001f){++bank_cases;require(visible_bank_peak<native_bank_peak*.05f,"final cosmetic bank acceleration spike falls below five percent of the native threshold snap");}
            if(native_camera_peak>.001f){++camera_cases;require(visible_camera_peak<native_camera_peak*.03f,"final long-turn camera acceleration spike falls below three percent of the native threshold snap");}
            if(native_fov_peak>.001f){++fov_cases;require(visible_fov_peak<native_fov_peak*.025f,"boost settle FOV acceleration spike falls below two and a half percent of the native threshold snap");}
            // With no deadband, the remaining tail decays rather than being
            // discarded. Give the original coefficients time to converge.
            for(int tail=0;tail<180;++tail)rendered.tick(1./60,{0,0,boost==1});
            const auto& settled=rendered.presentation_state();
            require(std::abs(settled.model_roll)<.0001f&&std::abs(settled.camera.roll)<.0001f,"continuous bank and chase roll return to rest without a final snap");
            const float final_fov=boost==1?1.4835298f:1.134464f;
            require(near(settled.camera.fov,final_fov,.00001f),"continuous FOV settles to the native cruise or turbo target");
        }
        require(bank_cases==9&&camera_cases==3&&fov_cases==10,"settle regressions cover short and long turns, held turbo and released turbo");
        require(length(f30.presentation_state().camera.position-f144.presentation_state().camera.position)<.0001f&&near(f30.presentation_state().model_roll,f144.presentation_state().model_roll),"continuous presentation is render-rate independent at thirty and144Hz");
    }
    std::printf("Gemlik flight: %d reference, timing, camera and safety checks passed\n",checks);
}
