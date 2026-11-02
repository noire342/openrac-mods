// SPDX-License-Identifier: GPL-3.0-or-later
#include "asteroid_contacts.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace spacevoyages {
namespace {
constexpr float skin=.025f;
using D3=std::array<double,3>;
D3 difference(Vec3 a,Vec3 b){return {{double(a.x)-b.x,double(a.y)-b.y,double(a.z)-b.z}};}
double scalar(D3 a,D3 b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
D3 plus(D3 a,D3 b){return {{a[0]+b[0],a[1]+b[1],a[2]+b[2]}};}
D3 times(D3 a,double t){return {{a[0]*t,a[1]*t,a[2]*t}};}
D3 normalized(D3 a,D3 fallback={{-1,0,0}}){const double n=std::sqrt(scalar(a,a));return n>1e-15?times(a,1./n):fallback;}
D3 vector_cross(D3 a,D3 b){return {{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]}};}
Vec3 eye_at(Vec3 ship,D3 direction,double distance){return {float(double(ship.x)+direction[0]*distance),float(double(ship.y)+direction[1]*distance),float(double(ship.z)+direction[2]*distance)};}
D3 angular_step(D3 from,D3 to,double fraction){
    const double cosine=std::clamp(scalar(from,to),-1.,1.);
    if(cosine>.999999999999)return normalized(plus(times(from,1-fraction),times(to,fraction)),to);
    D3 tangent=plus(to,times(from,-cosine));
    if(scalar(tangent,tangent)<1e-20){tangent=plus(D3{{0,0,1}},times(from,-from[2]));if(scalar(tangent,tangent)<1e-20)tangent=vector_cross({{0,1,0}},from);}
    tangent=normalized(tangent);const double angle=std::acos(cosine)*fraction;
    return normalized(plus(times(from,std::cos(angle)),times(tangent,std::sin(angle))),to);
}
bool same_float_direction(D3 a,D3 b){return float(a[0])==float(b[0])&&float(a[1])==float(b[1])&&float(a[2])==float(b[2]);}
D3 carry_native_rotation(D3 from,D3 to,D3 correction){
    const double cosine=std::clamp(scalar(from,to),-1.,1.);const auto axis=vector_cross(from,to);const double sine2=scalar(axis,axis);
    if(sine2<1e-20){
        if(cosine>0)return correction;
        auto tangent=plus(D3{{0,0,1}},times(from,-from[2]));if(scalar(tangent,tangent)<1e-20)tangent=vector_cross({{0,1,0}},from);
        tangent=normalized(tangent);return normalized(plus(times(correction,-1),times(tangent,2*scalar(tangent,correction))));
    }
    return normalized(plus(plus(times(correction,cosine),vector_cross(axis,correction)),times(axis,scalar(axis,correction)*(1-cosine)/sine2)));
}
std::array<Vec3,6> bounds_exits(const std::vector<Asteroid>& rocks,const std::uint8_t* alive,std::size_t alive_count,Vec3 anchor,double rock_clearance,double anchor_clearance){
    std::array<double,3> low{{double(anchor.x)-anchor_clearance,double(anchor.y)-anchor_clearance,double(anchor.z)-anchor_clearance}};
    std::array<double,3> high{{double(anchor.x)+anchor_clearance,double(anchor.y)+anchor_clearance,double(anchor.z)+anchor_clearance}};
    for(std::size_t i=0;i<rocks.size();++i){if(alive&&i<alive_count&&!alive[i])continue;
        const auto& rock=rocks[i];const double r=double(rock.radius)+rock_clearance;
        const std::array<double,3> p{{rock.position.x,rock.position.y,rock.position.z}};
        for(unsigned axis=0;axis<3;++axis){low[axis]=std::min(low[axis],p[axis]-r);high[axis]=std::max(high[axis],p[axis]+r);}}
    const float negative=-std::numeric_limits<float>::infinity(),positive=-negative;
    return {{
        {std::nextafter(float(low[0]-1.),negative),anchor.y,anchor.z},
        {std::nextafter(float(high[0]+1.),positive),anchor.y,anchor.z},
        {anchor.x,std::nextafter(float(low[1]-1.),negative),anchor.z},
        {anchor.x,std::nextafter(float(high[1]+1.),positive),anchor.z},
        {anchor.x,anchor.y,std::nextafter(float(low[2]-1.),negative)},
        {anchor.x,anchor.y,std::nextafter(float(high[2]+1.),positive)}
    }};
}
}
AsteroidContacts::AsteroidContacts(std::vector<Asteroid> asteroids):index_(std::move(asteroids)){
    const auto count=index_.asteroids().size();scratch_.reserve(count);
    sweep_.reserve(count);overlaps_.reserve(count);
}
bool AsteroidContacts::set_alive_mask(const std::vector<std::uint8_t>* mask)noexcept {
    if(mask&&mask->size()!=index_.asteroids().size())return false;
    alive_mask_=mask?mask->data():nullptr;alive_count_=mask?mask->size():0;return true;
}
bool AsteroidContacts::alive(std::size_t id)const noexcept {
    return !alive_mask_||id>=alive_count_||alive_mask_[id]!=0;
}
bool AsteroidContacts::query(Vec3 position,float radius){
    if(!index_.query_sphere(position,radius,overlaps_,scratch_))return false;
    overlaps_.erase(std::remove_if(overlaps_.begin(),overlaps_.end(),[&](auto id){return !alive(id);}),overlaps_.end());return true;
}
bool AsteroidContacts::sweep(Vec3 start,Vec3 end,float radius){
    if(!index_.sweep_sphere(start,end,radius,sweep_,scratch_))return false;
    sweep_.erase(std::remove_if(sweep_.begin(),sweep_.end(),[&](const auto& hit){return !alive(hit.index);}),sweep_.end());return true;
}
bool AsteroidContacts::clear(Vec3 position,float radius){
    if(query(position,radius))return overlaps_.empty();
    // Recovery may exceptionally exit beyond the index's query domain. Its
    // finite input geometry remains checkable directly without an allocation.
    if(!finite(position))return false;
    for(std::size_t i=0;i<index_.asteroids().size();++i){if(!alive(i))continue;const auto& a=index_.asteroids()[i];
        const double x=double(position.x)-a.position.x,y=double(position.y)-a.position.y,z=double(position.z)-a.position.z;
        const double r=double(a.radius)+radius;if(x*x+y*y+z*z<=r*r)return false;
    }
    return true;
}
Vec3 AsteroidContacts::recover_external(Vec3 origin){
    Vec3 best=origin;double best_distance=std::numeric_limits<double>::infinity();bool found=false;
    const auto consider=[&](Vec3 candidate){
        if(!finite(candidate)||!clear(candidate,ship_radius+skin))return;
        const double x=double(candidate.x)-origin.x,y=double(candidate.y)-origin.y,z=double(candidate.z)-origin.z;
        const double distance=x*x+y*y+z*z;
        if(!found||distance<best_distance){best=candidate;best_distance=distance;found=true;}
    };
    const auto& rocks=index_.asteroids();
    // Merge the occupied ray intervals connected to t=0. Merely pushing along
    // one overlap normal can alternate forever between touching rocks. Each
    // pass either extends the union to another interval or terminates, so at
    // most N+1 scans suffice; the26 fixed rays use only scalar scratch state.
    for(int x=-1;x<=1;++x)for(int y=-1;y<=1;++y)for(int z=-1;z<=1;++z){
        if(!x&&!y&&!z)continue;
        const Vec3 direction=unit({float(x),float(y),float(z)});
        const double a=double(direction.x)*direction.x+double(direction.y)*direction.y+double(direction.z)*direction.z;
        double exit=0;
        for(std::size_t pass=0;pass<=rocks.size();++pass){
            double extended=exit;
            for(std::size_t id=0;id<rocks.size();++id){if(!alive(id))continue;const auto& rock=rocks[id];
                const double mx=double(origin.x)-rock.position.x,my=double(origin.y)-rock.position.y,mz=double(origin.z)-rock.position.z;
                const double b=mx*direction.x+my*direction.y+mz*direction.z;
                const double r=double(rock.radius)+ship_radius+4.f*skin;
                const double c=mx*mx+my*my+mz*mz-r*r,discriminant=b*b-a*c;
                if(discriminant<0)continue;
                const double root=std::sqrt(discriminant),enter=(-b-root)/a,leave=(-b+root)/a;
                if(leave>=0&&enter<=exit+1e-9&&leave>extended)extended=leave;
            }
            if(extended<=exit)break;
            exit=extended;
        }
        const double distance=exit+skin;
        consider({float(double(origin.x)+direction.x*distance),float(double(origin.y)+direction.y*distance),float(double(origin.z)+direction.z*distance)});
    }
    if(found)return best;
    // Last-resort recovery, never ordinary motion: a point outside one face
    // of every inflated sphere's global AABB is guaranteed outside them all.
    // Index construction bounds centers/radii, so these exits remain finite.
    if(rocks.empty())return origin;
    const auto exits=bounds_exits(rocks,alive_mask_,alive_count_,origin,ship_radius+4.f*skin,0.);
    for(const auto& candidate:exits)consider(candidate);
    // At least one finite face exit must pass for constructor-validated data;
    // retain that guaranteed outside point even if a future guard is stricter.
    return found?best:exits[0];
}
AsteroidMotionResult AsteroidContacts::move(Vec3 previous,Vec3 desired,float seconds){
    stats_.contacts=0;
    AsteroidMotionResult result{previous,0,false};
    if(!finite(previous)||!finite(desired))return result;
    Vec3 start=previous,remaining=desired-previous;bool recovering=false;
    double remaining_seconds=std::isfinite(seconds)&&seconds>0?double(seconds):0.;
    // Normal travel starts outside all envelopes. This bounded recovery also
    // handles an externally moved ship without publishing a pose inside rock.
    for(unsigned pass=0;pass<8;++pass){
        query(start,ship_radius+skin);
        if(overlaps_.empty())break;
        recovering=true;
        for(auto id:overlaps_){const auto& a=index_.asteroids()[id];
            const auto normal=unit(start-a.position,unit(previous-desired,{1,0,0}));
            start=a.position+normal*(a.radius+ship_radius+skin*2.f);
        }
    }
    for(unsigned pass=0;pass<6;++pass){
        if(!sweep(start,start+remaining,ship_radius+skin)){remaining={};break;}
        if(sweep_.empty()){start=start+remaining;remaining={};break;}
        const auto hit=sweep_.front();
        const auto& rock=index_.asteroids()[hit.index];
        const auto normal=unit(hit.normal,unit(start-rock.position));
        if(result.contact_id==static_cast<std::size_t>(-1))result.contact_id=hit.index;
        if(remaining_seconds>0){
            const double inward=std::max(0.,-(double(remaining.x)*normal.x+double(remaining.y)*normal.y+double(remaining.z)*normal.z)/remaining_seconds);
            if(inward>result.impact_speed){result.impact_speed=float(std::min(inward,double(std::numeric_limits<float>::max())));result.contact_id=hit.index;}
        }
        const Vec3 contact=start+remaining*hit.time;
        start=rock.position+unit(contact-rock.position,normal)*(rock.radius+ship_radius+skin*2.f);
        remaining=remaining*(1.f-hit.time);
        remaining_seconds*=1.-hit.time;
        // Preserve tangential motion. A head-on contact brakes instead of
        // inventing a turn, bank, bounce or player-control impulse.
        remaining=remaining-normal*std::min(dot(remaining,normal),0.f);
        ++result.contacts;
        if(length_squared(remaining)<1e-10f){remaining={};break;}
    }
    // A pathological custom layout must not tunnel through an unprocessed
    // second contact after the bounded solver. Keep the last safe location.
    if(!clear(start,ship_radius)){
        if(clear(previous,ship_radius))start=previous;
        else {start=recover_external(previous);recovering=true;}
    }
    result.position=start;result.blocked=result.contacts>0||recovering;
    if(recovering)result.impact_speed=0; // external pose repair is not a crash
    if(query(start,ship_radius+4.f*skin)&&!overlaps_.empty()){
        result.touching=true;if(result.contact_id==static_cast<std::size_t>(-1))result.contact_id=overlaps_.front();
    }
    stats_.contacts=result.contacts;stats_.total_contacts+=result.contacts;
    // An external pose repair is a discontinuity of the ship itself. Do not
    // carry an unrelated old camera path through a newly injected world pose.
    if(recovering){camera_history_=camera_modified_=false;camera_length_=0;}
    return result;
}
bool AsteroidContacts::camera_pose_clear(Vec3 ship,Vec3 eye){
    if(!finite(eye))return false;
    const auto arm=difference(eye,ship);
    if(scalar(arm,arm)<double(camera_minimum_distance)*camera_minimum_distance)return false;
    return sweep(ship,eye,camera_radius+skin)&&sweep_.empty();
}
bool AsteroidContacts::camera_transition_clear(Vec3 ship,Vec3 eye){
    if(!camera_pose_clear(ship,eye))return false;
    if(!camera_history_)return true;
    const auto previous=difference(camera_previous_eye_,camera_previous_ship_),current=difference(eye,ship);
    const auto change=plus(current,times(previous,-1.));const double n=scalar(change,change);
    const double t=n>0?std::clamp(-scalar(previous,change)/n,0.,1.):0.;
    const auto closest=plus(previous,times(change,t));
    if(scalar(closest,closest)<double(camera_minimum_distance)*camera_minimum_distance)return false;
    return sweep(camera_previous_eye_,eye,camera_radius+skin)&&sweep_.empty();
}
bool AsteroidContacts::camera_shoulder(Vec3 ship,Vec3 native_eye,double distance,bool check_transition,Vec3& eye){
    const auto boom=normalized(difference(native_eye,ship));
    D3 up=plus(D3{{0,0,1}},times(boom,-boom[2]));
    if(scalar(up,up)<1e-15)up=plus(D3{{0,1,0}},times(boom,-boom[1]));
    up=normalized(up);const auto side=normalized(vector_cross(boom,up));
    std::array<D3,9> tangents{};std::size_t count=0;
    if(camera_history_&&camera_modified_){const auto old_side=plus(camera_direction_,times(boom,-scalar(camera_direction_,boom)));
        if(scalar(old_side,old_side)>1e-15)tangents[count++]=normalized(old_side);}
    for(const auto& t:std::array<D3,8>{{up,normalized(plus(up,side)),side,normalized(plus(times(up,-1),side)),times(up,-1),normalized(plus(times(up,-1),times(side,-1))),times(side,-1),normalized(plus(up,times(side,-1)))}})tangents[count++]=t;
    const auto valid=[&](Vec3 p){return check_transition?camera_transition_clear(ship,p):camera_pose_clear(ship,p);};
    const auto previous_eye=camera_history_?eye_at(ship,camera_direction_,camera_length_):native_eye;
    bool found=false;double best=std::numeric_limits<double>::infinity();
    for(std::size_t i=0;i<count;++i){double low=0;
        for(unsigned ring=1;ring<=12;++ring){double high=double(pi)*ring/12.;
            const auto at=[&](double angle){return eye_at(ship,plus(times(boom,std::cos(angle)),times(tangents[i],std::sin(angle))),distance);};
            Vec3 candidate=at(high);if(!valid(candidate)){low=high;continue;}
            // Refine the smallest necessary deflection rather than snapping
            // to a fixed shoulder angle. The last accepted pose stays verified.
            for(unsigned refine=0;refine<12;++refine){const double middle=(low+high)*.5;const auto trial=at(middle);
                if(valid(trial)){high=middle;candidate=trial;}else low=middle;}
            const auto native_delta=difference(candidate,native_eye),old_delta=difference(candidate,previous_eye);
            const double score=scalar(native_delta,native_delta)+(camera_history_?.3*scalar(old_delta,old_delta):0.);
            if(!found||score<best){eye=candidate;best=score;found=true;}break;
        }
    }
    return found;
}
Vec3 AsteroidContacts::camera_endpoint_recovery(Vec3 ship,Vec3 native_eye){
    const auto exits=bounds_exits(index_.asteroids(),alive_mask_,alive_count_,ship,camera_radius+4.f*skin,camera_minimum_distance+skin);
    Vec3 best=exits[0];bool found=false;double score=std::numeric_limits<double>::infinity();
    // Prefer a fully continuous valid boom if one exists. Only a pathological
    // externally discontinuous/caged input can reach the endpoint-only pass:
    // no algorithm can provide a clear ray through a completely closed cage.
    for(bool require_path:{true,false}){
        for(const auto& p:exits){const auto arm=difference(p,ship);
            if(!finite(p)||scalar(arm,arm)<double(camera_minimum_distance)*camera_minimum_distance||!clear(p,camera_radius+skin)||
               (require_path&&!camera_transition_clear(ship,p)))continue;
            const auto delta=difference(p,native_eye);const double d=scalar(delta,delta);
            if(!found||d<score){found=true;score=d;best=p;}}
        if(found)break;
    }
    camera_history_=false;return best;
}
Vec3 AsteroidContacts::camera(Vec3 ship,Vec3 desired){
    if(!finite(ship)||!finite(desired))return desired;
    if(length_squared(desired-ship)<1e-10f)desired=eye_at(ship,camera_history_?camera_direction_:D3{{-1,0,0}},double(camera_minimum_distance)+skin);
    const auto native_arm=difference(desired,ship);const double native_length=std::sqrt(scalar(native_arm,native_arm));
    const auto native_direction=normalized(native_arm);
    if(camera_history_){
        // Carry the existing correction with the native rig's own motion.
        // Only the collision deviation decays; there is no extra low-pass
        // controller that would lag a continuously turning unobstructed ship.
        camera_direction_=carry_native_rotation(camera_previous_native_direction_,native_direction,camera_direction_);
        camera_length_=std::max(double(camera_minimum_distance)+skin,native_length+(camera_length_-camera_previous_native_length_));
    }
    const bool queried=sweep(ship,desired,camera_radius+skin);
    const bool native_clear=queried&&sweep_.empty();
    const double safe_length=native_clear?native_length:queried&&!sweep_.empty()?std::max(0.,native_length*sweep_.front().time-skin):0.;
    const auto publish=[&](Vec3 eye,D3 direction,double distance){
        const bool identical=eye.x==desired.x&&eye.y==desired.y&&eye.z==desired.z;
        camera_direction_=identical?native_direction:direction;camera_length_=identical?native_length:distance;
        camera_previous_native_direction_=native_direction;camera_previous_native_length_=native_length;
        camera_previous_ship_=ship;camera_previous_eye_=eye;camera_history_=true;camera_modified_=!identical;
        camera_fraction_=float(std::clamp(camera_length_/native_length,0.,1.));
        stats_.camera_fraction=camera_fraction_;stats_.camera_occluded=!native_clear;
        stats_.camera_lateral=!identical&&!same_float_direction(camera_direction_,native_direction);return eye;
    };
    // No controller is added to unobstructed native camera movement. Return
    // the exact incoming bits, not a subtract/add reconstruction of its arm.
    if(native_clear&&!camera_modified_&&native_length>=camera_minimum_distance&&
       (index_.asteroids().empty()||camera_transition_clear(ship,desired)))return publish(desired,native_direction,native_length);
    const double orbit_minimum=double(camera_minimum_distance)+skin;
    double target_length=std::max(orbit_minimum,std::min(native_length,safe_length));
    if(native_clear)target_length=std::max(double(camera_minimum_distance),native_length);
    Vec3 target=native_clear?desired:eye_at(ship,native_direction,target_length);bool target_lateral=false;
    if(!camera_pose_clear(ship,target)){
        target_lateral=true;
        if(!camera_shoulder(ship,desired,target_length,false,target)){
            target_length=std::max(target_length,native_length);
            camera_shoulder(ship,desired,target_length,false,target);
        }
    }
    const auto target_arm=difference(target,ship);
    if(target_lateral)target_length=std::sqrt(scalar(target_arm,target_arm));
    // Preserve the native direction symbolically on a shortened boom. Float
    // world-position reconstruction must not masquerade as a lateral orbit.
    const auto target_direction=target_lateral?normalized(target_arm,native_direction):native_direction;
    const bool angular=stats_.camera_lateral||!same_float_direction(target_direction,native_direction);
    double distance=target_length;auto direction=target_direction;
    if(camera_history_){
        if(target_length>camera_length_)distance=camera_length_+(target_length-camera_length_)*.08;
        if(angular)direction=angular_step(camera_direction_,target_direction,.08);
    }
    Vec3 eye=eye_at(ship,direction,distance);
    if(!camera_transition_clear(ship,eye)){
        // Apply only as much geometric correction as necessary. A previous
        // safe pose plus a new native boom never licenses an unchecked chord.
        if(camera_shoulder(ship,desired,distance,true,eye)){
            const auto arm=difference(eye,ship);distance=std::sqrt(scalar(arm,arm));direction=normalized(arm,native_direction);
        }else if(camera_history_&&camera_transition_clear(ship,camera_previous_eye_)){
            eye=camera_previous_eye_;const auto arm=difference(eye,ship);distance=std::sqrt(scalar(arm,arm));direction=normalized(arm,native_direction);
        }else{
            // Externally discontinuous/caged poses may have no continuous
            // valid boom. Discard that history and verify the new endpoint.
            camera_history_=false;distance=std::max(native_length,orbit_minimum);
            if(camera_shoulder(ship,desired,distance,false,eye)){
                const auto arm=difference(eye,ship);distance=std::sqrt(scalar(arm,arm));direction=normalized(arm,native_direction);
            }
        }
    }
    if(!camera_pose_clear(ship,eye)){
        eye=camera_endpoint_recovery(ship,desired);const auto arm=difference(eye,ship);
        distance=std::sqrt(scalar(arm,arm));direction=normalized(arm,native_direction);
    }
    // Natural representational convergence only: when the corrected arm has
    // the same float components as the native arm, return its exact eye bits.
    // This is not a pixel/world-distance cutoff or a visible terminal snap.
    if(native_clear&&float(distance)==float(native_length)&&
       float(direction[0]*distance)==float(native_arm[0])&&float(direction[1]*distance)==float(native_arm[1])&&float(direction[2]*distance)==float(native_arm[2])&&
       camera_transition_clear(ship,desired))return publish(desired,native_direction,native_length);
    return publish(eye,direction,distance);
}
}
