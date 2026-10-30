// SPDX-License-Identifier: MIT
// Copyright (c) 2026 noire342
#include "rotor_controller.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

using namespace revolverator;
namespace {
constexpr float tau=6.2831853071795864769f;
int checks;
void require(bool test,const char* message){++checks;if(!test){std::fprintf(stderr,"rotor: %s\n",message);std::exit(1);}}
float difference(float a,float b){return std::remainder(a-b,tau);}
bool close(float a,float b,float epsilon=.00002f){return std::abs(difference(a,b))<epsilon;}
RotorFrame frame(){RotorFrame f;f.ammunition=20;f.seconds=1.f/60;f.playing=true;return f;}
}
int main(){
    {
        RotorController rotor;auto f=frame();
        for(int i=0;i<60;++i)rotor.update(f);
        require(close(rotor.angle(),1.f),"loaded idle turns the head at one radian per second");
        f.ammunition=0;const float stopped=rotor.angle();
        for(int i=0;i<600;++i)rotor.update(f);
        require(rotor.angle()==stopped&&rotor.speed()==0,"empty idle remains exactly stationary");
        f.phase=Phase::Windup;f.seconds=.025f;rotor.update(f);
        require(close(rotor.angle(),stopped+.2f),"last cartridge still powers an active capture windup");
        f.phase=Phase::Recovery;const float recovered=rotor.angle();rotor.update(f);
        require(rotor.angle()==recovered,"an empty recovery does not start idle rotation");
    }
    {
        RotorController rotor;rotor.reset(2.9f);auto f=frame();f.ammunition=0;
        const float before=rotor.angle();rotor.dry_fire();f.seconds=.03f;rotor.update(f);
        const float first=rotor.angle();require(difference(first,before)>0,"empty trigger gives a short forward jerk");
        rotor.dry_fire();rotor.update(f);
        require(close(rotor.angle(),before+RotorController::dry_excursion),"a repeated request cannot restart a pending jerk");
        f.seconds=.04f;rotor.update(f);require(difference(rotor.angle(),before)<RotorController::dry_excursion,"motor visibly rolls backward");
        rotor.update(f);rotor.update(f);
        require(rotor.angle()==before&&!rotor.dry_firing(),"dry fire returns to the exact original angle with zero net rotation");
        for(int i=0;i<600;++i)rotor.update(f);
        require(rotor.angle()==before,"dry fire cannot leave residual idle drift");
    }
    {
        RotorController rotor;auto f=frame();rotor.update(f);rotor.dry_fire();f.ammunition=0;rotor.update(f);
        const float paused=rotor.angle();f.paused=true;
        for(int i=0;i<100;++i)rotor.update(f);
        require(rotor.angle()==paused&&rotor.dry_firing(),"pause freezes the dry-fire animation");
        f.paused=false;f.playing=false;rotor.update(f);require(rotor.angle()==paused,"menus freeze motor time");
        f.playing=true;for(int i=0;i<20;++i)rotor.update(f);
        require(!rotor.dry_firing(),"dry-fire rollback resumes after pause");
    }
    {
        RotorController rotor;auto f=frame();rotor.reset(-2.7f);
        f.phase=Phase::Spinning;f.enemy_angle=2.9f;rotor.update(f);
        const float offset=difference(rotor.angle(),f.enemy_angle);
        require(close(rotor.angle(),-2.7f),"entering synchronization cannot snap the shaft angle");
        float last_motor=rotor.angle(),last_enemy=f.enemy_angle;
        for(int i=0;i<2000;++i){
            f.seconds=(i%3==0)?.013f:(i%3==1)?.019f:.017f;
            f.charge=float(i%101)/100;
            f.enemy_angle=std::remainder(f.enemy_angle+f.seconds*(8+10*f.charge),tau);
            rotor.update(f);
            require(close(difference(rotor.angle(),last_motor),difference(f.enemy_angle,last_enemy)),
                "head and captive must have identical signed angular displacement, including wrap and variable frame time");
            require(close(difference(rotor.angle(),f.enemy_angle),offset),"synchronization must not accumulate phase drift");
            last_motor=rotor.angle();last_enemy=f.enemy_angle;
        }
        f.paused=true;const float pause=rotor.angle();rotor.update(f);
        require(rotor.angle()==pause,"pause also freezes a synchronized shaft");
    }
    {
        RotorController rotor;auto f=frame();f.seconds=std::numeric_limits<float>::quiet_NaN();rotor.update(f);
        require(rotor.angle()==0,"nonfinite frame duration cannot poison the motor");
        f.seconds=.2f;rotor.update(f);require(rotor.angle()==0,"unbounded elapsed time cannot advance the motor");
        f.seconds=.01f;f.phase=Phase::Spinning;f.enemy_angle=std::numeric_limits<float>::quiet_NaN();rotor.update(f);
        require(std::isfinite(rotor.angle()),"an invalid captive angle is ignored");
    }
    std::printf("rotor controller: %d timing, dry-fire rollback and exact synchronization checks passed\n",checks);
}
