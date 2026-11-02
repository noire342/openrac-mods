// SPDX-License-Identifier: GPL-3.0-or-later
#include "asteroid_contacts.hpp"
#include "flight.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <vector>
using namespace spacevoyages;
namespace {std::size_t allocations;}
void* operator new(std::size_t n){++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
namespace {
unsigned checks;
void require(bool okay,const char* why){++checks;if(!okay){std::fprintf(stderr,"asteroid_contacts: %s\n",why);std::exit(1);}}
bool outside(Vec3 point,float radius,const std::vector<Asteroid>& rocks){
    if(!finite(point))return false;
    for(const auto& rock:rocks)if(length(point-rock.position)<rock.radius+radius-.0001f)return false;
    return true;
}
bool close(Vec3 a,Vec3 b,float epsilon=.0001f){return length(a-b)<epsilon;}
FlightInput scripted(std::uint64_t tick){
    if(tick<200)return {0,0,true};
    if(tick<650)return {1,0,true};
    if(tick<950)return {-1,.2f,false};
    if(tick<1300)return {.15f,-.6f,true};
    return {0,0,false};
}
struct Simulation {
    FlightController flight,reference;
    AsteroidContacts contacts;
    const std::vector<Asteroid>& rocks;
    Vec3 safe_camera{};
    double remainder{};
    bool poses_safe=true,controls_identical=true;
    bool interpolation_safe=true,camera_has_previous{};
    Vec3 previous_ship{},previous_eye{};
    float maximum_eye_step{};
    unsigned camera_calls{};
    explicit Simulation(const std::vector<Asteroid>& data):contacts(data),rocks(data){flight.reset({-12,0,0},0,0);reference.reset({-12,0,0},0,0);}
    static void constrain(void* pointer,FlightController& flight,Vec3 previous){
        auto& self=*static_cast<Simulation*>(pointer);const auto& before=flight.state();
        const auto safe=self.contacts.move(previous,before.position);
        // Apply geometry using the same heading: the policy must not inject
        // a turn, roll, damage, input change or speed preset.
        flight.constrain(safe.position,direction(before.yaw,before.pitch),1.f);
    }
    void step(){
        const auto input=scripted(flight.state().ticks);
        flight.native_tick(input,constrain,this);reference.native_tick(input);
        const auto& actual=flight.state();const auto& native=reference.state();
        controls_identical=controls_identical&&std::abs(wrap_angle(actual.yaw-native.yaw))<.000003f&&std::abs(actual.pitch-native.pitch)<.000003f&&
            actual.roll==native.roll&&actual.speed==native.speed&&actual.model_roll==native.model_roll&&actual.boost_fraction==native.boost_fraction;
        safe_camera=contacts.camera(actual.position,flight.presentation_state().camera.position);++camera_calls;
        poses_safe=poses_safe&&outside(actual.position,AsteroidContacts::ship_radius,rocks)&&outside(safe_camera,AsteroidContacts::camera_radius,rocks)&&length(safe_camera-actual.position)>=1.85f-.0001f;
        if(camera_has_previous){maximum_eye_step=std::max(maximum_eye_step,length(safe_camera-previous_eye));
            for(int sample=0;sample<=20;++sample){const float alpha=float(sample)/20;
                const auto eye=lerp(previous_eye,safe_camera,alpha),ship=lerp(previous_ship,actual.position,alpha);
                interpolation_safe=interpolation_safe&&outside(eye,AsteroidContacts::camera_radius,rocks)&&length(eye-ship)>=1.85f-.0001f;}}
        previous_ship=actual.position;previous_eye=safe_camera;camera_has_previous=true;
    }
    // The geometry adapter's API is one60Hz step, as FlightController's native
    // path. Render/hitch partitioning supplies that same sequence explicitly.
    void elapsed(double dt){remainder+=std::min(dt,.25);while(remainder+1e-10>=1./60){step();remainder-=1./60;}}
};
}
int main(){
    require(AsteroidContacts::ship_radius==1.5f&&AsteroidContacts::camera_radius==.3f,"ship and camera envelopes retain the independently measured conservative dimensions");
    {
        const std::vector<Asteroid> rocks{{{0,0,0},3},{{20,0,0},2}};
        AsteroidContacts contacts(rocks);std::vector<std::uint8_t> alive{0,1};
        require(contacts.set_alive_mask(&alive),"a stable same-index alive mask binds without rebuilding geometry");
        const auto through=contacts.move({-10,0,0},{10,0,0});
        require(!through.blocked&&!through.touching&&close(through.position,{10,0,0})&&through.impact_speed==0,"destroyed rock has no movement, damage or contact-episode surface");
        const auto next=contacts.move({-10,0,0},{40,0,0});
        require(next.blocked&&next.contact_id==1&&next.position.x>16&&next.position.x<17,"continuous sweep skips the destroyed first rock and still finds the following live rock");
        contacts.reset();const Vec3 native_eye{8,0,0};const auto eye=contacts.camera({-8,0,0},native_eye);
        require(std::memcmp(&eye,&native_eye,sizeof(Vec3))==0&&!contacts.stats().camera_occluded,"destroyed rock no longer occludes camera or changes native eye");
        const auto repair=contacts.move({0,0,0},{0,0,0});
        require(!repair.blocked&&close(repair.position,{0,0,0}),"external placement inside only a destroyed rock needs no recovery teleport");
        const std::vector<std::uint8_t> wrong{0};
        require(!contacts.set_alive_mask(&wrong)&&!contacts.move({-10,0,0},{10,0,0}).blocked,"an invalid mask is refused and preserves the existing binding");
        const auto before=allocations;
        bool okay=true;
        for(int i=0;i<1000;++i){alive[0]=std::uint8_t(i&1);contacts.reset();
            const auto movement=contacts.move({-10,0,0},{10,0,0});
            const auto camera=contacts.camera({-8,0,0},native_eye);
            okay=okay&&(movement.blocked==(alive[0]!=0))&&(alive[0]||close(camera,native_eye));
        }
        require(okay&&allocations==before,"changing mask bytes updates ship/camera queries immediately with zero allocations or reindexing");
        require(contacts.set_alive_mask(nullptr)&&contacts.move({-10,0,0},{10,0,0}).blocked,"an absent mask preserves the legacy all-alive policy");
        alive[0]=0;alive[1]=0;require(contacts.set_alive_mask(&alive),"an all-destroyed field retains stable indexing");
        const auto empty_repair=contacts.move({20,0,0},{20,0,0});
        require(!empty_repair.blocked&&close(empty_repair.position,{20,0,0}),"an all-destroyed field contributes no recovery bounds");
    }
    {
        AsteroidContacts empty({});const auto before=allocations;
        for(int i=0;i<100;++i){const Vec3 ship{-1000.75f,float(i)*.2f,12.5f},native_eye{.2f,float(i)-21.4f,-.0625f};
            const auto actual=empty.camera(ship,native_eye);
            require(std::memcmp(&actual,&native_eye,sizeof(Vec3))==0&&!empty.stats().camera_lateral,"unobstructed empty space returns native eye bit-for-bit without another camera controller");}
        require(allocations==before,"exact empty-space camera path allocates no memory");
        const Vec3 ship{10,0,0};
        require(length(empty.camera(ship,ship)-ship)>=1.85f-.0001f,"a degenerate externally supplied native boom still cannot place the eye inside the ship");
    }
    {
        const std::vector<Asteroid> rocks{{{0,0,0},3}};AsteroidContacts contacts(rocks);
        const auto head=contacts.move({-10,0,0},{10,0,0});
        require(head.blocked&&head.contacts==1&&head.position.x<-4.5f&&std::abs(head.position.y)<1e-6f&&outside(head.position,1.5f,rocks),"head-on motion brakes before rock without inventing lateral displacement");
        auto position=head.position;
        for(int i=0;i<300;++i){const auto stopped=contacts.move(position,position+Vec3{.5f,0,0});
            require(close(stopped.position,position)&&outside(stopped.position,1.5f,rocks),"repeated boosted head-on input never oscillates or penetrates the rock");position=stopped.position;}
        const auto glance=contacts.move({-10,4,0},{10,4,0});
        require(glance.blocked&&glance.contacts>0&&glance.position.x>-10&&glance.position.y>4&&outside(glance.position,1.5f,rocks),"glancing movement preserves a safe tangential component");
        const auto fast=contacts.move({-10000,0,0},{10000,0,0});
        require(fast.blocked&&fast.position.x<-4.5f&&outside(fast.position,1.5f,rocks),"long swept movement cannot tunnel through a rock between clear endpoints");
        const auto unqueryable=contacts.move({-2000000,0,0},{2000000,0,0});
        require(close(unqueryable.position,{-2000000,0,0}),"a finite sweep outside the index query domain fails closed instead of publishing unverified motion");
        const auto recovered=contacts.move({0,0,0},{0,0,0});
        require(outside(recovered.position,1.5f,rocks),"external placement at a single rock center recovers to a finite safe pose");
        contacts.reset();require(contacts.stats().contacts==0&&contacts.stats().total_contacts==0&&contacts.stats().camera_fraction==1,"new journey resets contact bookkeeping and camera history");
    }
    {
        const std::vector<Asteroid> rocks{{{0,0,0},2},{{10,12,0},2}};AsteroidContacts contacts(rocks);
        const auto corner=contacts.move({-20,3,0},{30,3,0});
        require(corner.contacts>=2&&outside(corner.position,1.5f,rocks),"a glancing sweep processes a second intervening rock and retains a safe final pose");
        std::printf("multiple-contact fixture: contacts=%u result=(%.6f,%.6f,%.6f)\n",corner.contacts,corner.position.x,corner.position.y,corner.position.z);
    }
    {
        const std::vector<Asteroid> rocks{{{0,0,0},2}};AsteroidContacts contacts(rocks);
        const Vec3 ship{-3.55f,0,0},native_eye{.2f,0,1};
        const auto eye=contacts.camera(ship,native_eye);
        std::printf("camera hull regression: eye=(%.9f,%.9f,%.9f), ship-distance=%.9f, rock-distance=%.9f\n",eye.x,eye.y,eye.z,length(eye-ship),length(eye));
        require(length(eye-ship)>=1.85f-.0001f&&outside(eye,.3f,rocks),"blocked native boom must keep camera outside both the rock and the measured ship hull");
    }
    {
        const std::vector<Asteroid> rocks{{{-3.5f,0,0},1}};AsteroidContacts contacts(rocks);
        const auto retracted=contacts.camera({0,0,0},{-5,0,0});
        require(contacts.stats().camera_occluded&&!contacts.stats().camera_lateral&&contacts.stats().camera_fraction<1&&outside(retracted,.3f,rocks),"pure boom shortening retracts before contact without changing native view orientation");
        float previous=contacts.stats().camera_fraction,previous_delta=1;
        const Vec3 clear_ship{0,-2,0},clear_native_eye{-5,-2,0};Vec3 camera{};
        for(int i=0;i<300;++i){camera=contacts.camera(clear_ship,clear_native_eye);const float fraction=contacts.stats().camera_fraction;
            const float delta=fraction-previous;
            require(!contacts.stats().camera_occluded&&fraction>=previous&&fraction<=1&&delta<=previous_delta+.000001f&&outside(camera,.3f,rocks)&&length(camera-clear_ship)>=1.85f-.0001f,"clear boom restores monotonically with shrinking increments and no visible terminal snap");
            if(i<120)require(std::abs(delta-(1-previous)*.08f)<.0000001f,"camera restoration advances exactly one declared60Hz step per call");
            previous=fraction;previous_delta=delta;
        }
        require(previous>.99999f,"asymptotic restoration approaches the original native boom");
        require(std::memcmp(&camera,&clear_native_eye,sizeof(Vec3))==0,"restoration reaches native eye bits through float representational convergence rather than a pixel cutoff");
    }
    {
        const std::vector<Asteroid> rocks{{{0,0,0},2}};AsteroidContacts contacts(rocks);
        Vec3 previous_ship{},previous_eye{};bool previous=false,saw_lateral=false;float max_step=0;
        const auto before=allocations;
        for(int tick=0;tick<=150;++tick){
            const float yaw=-pi+2*pi*float(tick)/150;const auto radial=direction(yaw,0);
            const Vec3 ship=radial*3.55f,native_eye=ship-radial*3.75f+Vec3{0,0,1};
            const auto eye=contacts.camera(ship,native_eye);saw_lateral=saw_lateral||contacts.stats().camera_lateral;
            require(outside(eye,.3f,rocks)&&length(eye-ship)>=1.85f-.0001f,"rapid2.5second orbit across plus/minuspi preserves camera hull floor and asteroid clearance");
            if(previous){max_step=std::max(max_step,length(eye-previous_eye));
                for(int sample=0;sample<=32;++sample){const float alpha=float(sample)/32;
                    const auto interpolated_eye=lerp(previous_eye,eye,alpha),interpolated_ship=lerp(previous_ship,ship,alpha);
                    require(outside(interpolated_eye,.3f,rocks)&&length(interpolated_eye-interpolated_ship)>=1.85f-.0001f,
                        "interpolated native snapshots in the rapid orbit do not cross hull or rock");}}
            previous_ship=ship;previous_eye=eye;previous=true;
        }
        require(saw_lateral&&max_step<.6f&&allocations==before,"rapid orbit uses a continuous verified shoulder with no heap allocations or large camera jumps");
        std::printf("camera orbit150 ticks: max eye step=%.6f, all alpha0..1 samples safe\n",max_step);
    }
    {
        const std::vector<Asteroid> rocks{{{0,0,0},2}};AsteroidContacts contacts(rocks);FlightController pilot;
        pilot.reset({-3.55f,0,0},pi,0);bool saw_lateral=false;unsigned exact_after_recovery=0;
        Vec3 previous_ship{},previous_eye{};bool previous=false;float max_step=0;
        const auto constrain=+[](void* pointer,FlightController& flight,Vec3 old){
            auto& c=*static_cast<AsteroidContacts*>(pointer);const auto& state=flight.state();const auto moved=c.move(old,state.position);
            flight.constrain(moved.position,direction(state.yaw,state.pitch),1.f);
        };
        const auto before=allocations;
        for(int tick=0;tick<600;++tick){
            pilot.native_tick({tick<150?1.f:.25f,0,true},constrain,&contacts);
            const auto ship=pilot.state().position,native_eye=pilot.presentation_state().camera.position;
            const auto eye=contacts.camera(ship,native_eye);saw_lateral=saw_lateral||contacts.stats().camera_lateral;
            require(outside(eye,.3f,rocks)&&length(eye-ship)>=1.85f-.0001f,"real native flight moving away and turning around the close rock never exposes ship interior");
            if(tick>=500&&std::memcmp(&eye,&native_eye,sizeof(Vec3))==0)++exact_after_recovery;
            if(previous){max_step=std::max(max_step,length(eye-previous_eye));
                for(int sample=0;sample<=20;++sample){const float alpha=float(sample)/20;
                    const auto interpolated_eye=lerp(previous_eye,eye,alpha),interpolated_ship=lerp(previous_ship,ship,alpha);
                    require(outside(interpolated_eye,.3f,rocks)&&length(interpolated_eye-interpolated_ship)>=1.85f-.0001f,
                        "real native camera interpolation remains outside hull and asteroid throughout recovery");}}
            previous_ship=ship;previous_eye=eye;previous=true;
        }
        require(saw_lateral&&exact_after_recovery==100&&max_step<1.f&&allocations==before,
            "collision deviation restores exact native eye even during continuous turning, without a second free-space camera controller or allocation");
        std::printf("native departing/turning camera600 ticks: max eye step=%.6f, final100 eye poses bit-identical\n",max_step);
    }
    const std::vector<Asteroid> simulation_rocks{{{0,0,0},3},{{25,18,2},5},{{-30,-25,10},2.4f},{{70,-50,-20},4}};
    {
        Simulation native_steps(simulation_rocks),partitioned(simulation_rocks);
        const auto before=allocations;
        for(int tick=0;tick<1700;++tick)native_steps.step();
        require(allocations==before,"1700 real constrained FlightController steps and camera queries allocate no heap memory");
        require(native_steps.poses_safe&&native_steps.interpolation_safe&&native_steps.controls_identical&&native_steps.contacts.stats().total_contacts>0&&native_steps.camera_calls==1700,
            "1700 simulation ticks keep hull/camera outside rocks while native yaw,pitch,roll,speed,input and tick count remain intact");
        std::printf("native camera motion: max eye step=%.6f, all interpolated alpha samples safe\n",native_steps.maximum_eye_step);
        constexpr std::array<double,5> partition{{1./144,1./30,1./90,.25,1./60}};
        double accepted=0;unsigned frame=0;
        while(accepted<1700./60-1e-10){const double dt=std::min(partition[frame++%partition.size()],1700./60-accepted);partitioned.elapsed(dt);accepted+=dt;}
        require(partitioned.flight.state().ticks==1700&&partitioned.camera_calls==1700&&partitioned.poses_safe&&partitioned.controls_identical,
            "mixed render intervals and250ms hitches preserve1700 simulation/camera ticks");
        require(close(native_steps.flight.state().position,partitioned.flight.state().position)&&close(native_steps.safe_camera,partitioned.safe_camera)&&
            native_steps.contacts.stats().total_contacts==partitioned.contacts.stats().total_contacts&&native_steps.contacts.stats().camera_fraction==partitioned.contacts.stats().camera_fraction,
            "contact response and camera state are invariant to render-time partitioning of the same native tick sequence");
        Simulation hitch(simulation_rocks),fifteen(simulation_rocks);hitch.elapsed(.75);for(int i=0;i<15;++i)fifteen.step();
        require(hitch.flight.state().ticks==15&&close(hitch.flight.state().position,fifteen.flight.state().position)&&close(hitch.safe_camera,fifteen.safe_camera),
            "an oversized OS hitch consumes only the declared250ms native-step budget");
    }
    {
        std::vector<Asteroid> rocks;
        for(int x=-2;x<=2;++x)for(int y=-2;y<=2;++y)for(int z=-1;z<=1;++z)rocks.push_back({{float(x*9),float(y*9),float(z*9)},2});
        AsteroidContacts contacts(rocks);const auto before=allocations;
        for(int i=0;i<800;++i){const float y=float(i%41)-20;const auto result=contacts.move({-40,y,20},{40,-y,-20});
            require(outside(result.position,1.5f,rocks),"dense multi-contact sweeps never publish a penetrating final position");}
        require(allocations==before,"dense bounded solver uses its constructor-reserved buffers without allocations");
    }
    {
        const auto& planets=galaxy_catalog();const auto layout=build_asteroid_layout(planets.data(),planets.size());
        const auto& rocks=layout.asteroids();float max_step=0;
        for(std::size_t fixture=0;fixture<12;++fixture){const auto& selected=rocks[(fixture*13)%rocks.size()];
            AsteroidContacts contacts(rocks);FlightController pilot;const float yaw=float(fixture)*.57f,pitch=(float(fixture%3)-1)*.25f;
            pilot.reset(selected.position+direction(yaw,pitch)*(selected.radius+1.55f),yaw,pitch);
            Vec3 previous_ship{},previous_eye{};bool previous=false;const auto before=allocations;
            for(int tick=0;tick<180;++tick){
                const auto constrain=+[](void* p,FlightController& f,Vec3 old){auto& c=*static_cast<AsteroidContacts*>(p);const auto& state=f.state();const auto moved=c.move(old,state.position);f.constrain(moved.position,direction(state.yaw,state.pitch),1.f);};
                pilot.native_tick({fixture%2?1.f:-1.f,float(int(fixture%3)-1)*.35f,true},constrain,&contacts);
                const auto ship=pilot.state().position,eye=contacts.camera(ship,pilot.presentation_state().camera.position);
                require(outside(eye,.3f,rocks)&&length(eye-ship)>=1.85f-.00015f,"real native flight starting near authored field rocks keeps camera outside all spheres and hull");
                if(previous){max_step=std::max(max_step,length(eye-previous_eye));
                    for(int sample=0;sample<=12;++sample){const float alpha=float(sample)/12;
                        const auto e=lerp(previous_eye,eye,alpha),s=lerp(previous_ship,ship,alpha);
                        require(outside(e,.3f,rocks)&&length(e-s)>=1.85f-.00015f,"authored-field native camera interpolation stays outside all rocks and ship hull");}}
                previous_ship=ship;previous_eye=eye;previous=true;
            }
            require(allocations==before,"authored-field shoulder/orbit recovery uses no per-tick allocation");
        }
        std::printf("authored-field native camera2160 ticks: max eye step=%.6f, interpolation safe\n",max_step);
    }
    {
        // A deliberately impossible custom cage has no clear radius1.85 boom
        // in any direction. Even this external input must produce a verified
        // endpoint outside both hull and rocks, rather than an unchecked eye.
        std::vector<Asteroid> cage;
        for(int x=-1;x<=1;++x)for(int y=-1;y<=1;++y)for(int z=-1;z<=1;++z){if(!x&&!y&&!z)continue;cage.push_back({unit({float(x),float(y),float(z)})*3.525f,2});}
        AsteroidContacts contacts(cage);const auto before=allocations;const auto eye=contacts.camera({0,0,0},{-3.75f,0,1});
        require(finite(eye)&&outside(eye,.3f,cage)&&length(eye)>=1.85f&&allocations==before,"an unsatisfiable externally injected camera cage uses a verified finite endpoint outside hull and every rock");
    }
    // Minimal numeric regression: the public resolver accepts this layout,
    // whose physical rocks merely touch. Their expanded ship volumes overlap.
    // Recovery must choose a real lateral exit, not oscillate between pushes
    // and finally return a pose inside the other sphere.
    {
        const std::vector<Asteroid> touching{{{-2,0,0},2},{{2,0,0},2}};AsteroidContacts contacts(touching);
        const auto before=allocations;
        const auto result=contacts.move({0,0,0},{0,0,0});
        std::printf("external-overlap regression: result=(%.9f,%.9f,%.9f), distances=(%.9f,%.9f), required>=3.5\n",
            result.position.x,result.position.y,result.position.z,length(result.position-touching[0].position),length(result.position-touching[1].position));
        require(outside(result.position,1.5f,touching),"external overlap between two touching rocks must recover outside BOTH expanded envelopes");
        require(allocations==before&&result.blocked,"exceptional overlap recovery remains allocation-free and records that motion was constrained");
        const auto stationary=contacts.move(result.position,result.position);
        require(close(stationary.position,result.position),"a recovered external pose is stable on the following stationary tick");
    }
    {
        const std::vector<Asteroid> large{{{-40,0,0},40},{{40,0,0},40}};AsteroidContacts contacts(large);
        const auto result=contacts.move({0,0,0},{0,0,0});
        require(outside(result.position,1.5f,large)&&length(result.position)>10,"external recovery derives exit distance from inflated intervals rather than a fixed lateral offset");
        const std::vector<Asteroid> far{{{999996,0,0},2},{{1000000,0,0},2}};AsteroidContacts translated(far);
        const auto translated_result=translated.move({999998,0,0},{999998,0,0});
        require(outside(translated_result.position,1.5f,far),"ray recovery remains safe near the index coordinate limit with float world positions");
    }
    {
        std::vector<Asteroid> touching;
        for(int x=-1;x<=1;++x)for(int y=-1;y<=1;++y)for(int z=-1;z<=1;++z)touching.push_back({{float(x*4),float(y*4),float(z*4)},2});
        AsteroidContacts contacts(touching);const auto before=allocations;
        for(int i=0;i<120;++i){const auto& rock=touching[std::size_t(i)%touching.size()];
            const Vec3 from=rock.position+Vec3{.05f*float(i%3),-.05f*float(i%5),.05f*float(i%7)};
            const auto result=contacts.move(from,from+Vec3{.2f,-.1f,.05f});
            require(outside(result.position,1.5f,touching),"multiple externally overlapping volumes recover to a verified global free pose");
            const auto camera=contacts.camera(result.position,result.position+Vec3{-3.75f,0,1});
            require(outside(camera,.3f,touching),"camera sphere stays outside touching clusters after exceptional recovery");
        }
        require(allocations==before,"multi-volume recovery, verification and camera remain allocation-free across repeated external injections");
    }
    std::printf("asteroid_contacts: %u movement, camera, native tick and zero-allocation checks passed\n",checks);
}
