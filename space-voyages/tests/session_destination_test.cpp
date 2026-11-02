// SPDX-License-Identifier: GPL-3.0-or-later
// Production Session + real Windows GarageProvider/GetProcAddress, fake SDK14.
// No GL initialization, game, destination mount, card or campaign execution.
#include "../src/mod.cpp"
#include <algorithm>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks{};
void require(bool value,const char* why){++checks;if(!value){std::fprintf(stderr,"session_destination: %s\n",why);std::exit(1);}}
constexpr DestinationRoute home{18,DestinationVariant::VeldinGarage},final{18,DestinationVariant::Campaign};
openrac_mod* const owner=reinterpret_cast<openrac_mod*>(std::uintptr_t(0x7000));
using Publish=int(*)(const veldin_garage_snapshot_v1*,std::uint32_t);
using PublishCampaign=int(*)(const veldin_campaign_snapshot_v1*,std::uint32_t);
using Queries=std::uint32_t(*)();
Publish publish{};Queries provider_queries{};
PublishCampaign publish_campaign{};
template<class Fn>Fn export_pointer(HMODULE dll,const char* name){
    const auto symbol=GetProcAddress(dll,name);require(symbol!=nullptr,"test DLL exposes its fixed C ABI");
    Fn result{};static_assert(sizeof result==sizeof symbol,"Windows function pointer ABI");std::memcpy(&result,&symbol,sizeof result);return result;
}
openrac_destination descriptor(int level,bool added=false){
    openrac_destination d{};d.size=sizeof d;d.kind=added?OPENRAC_DESTINATION_ADDED:OPENRAC_DESTINATION_CAMPAIGN;
    d.handle=added?0x100000012ull:0x300000000ull+unsigned(level);d.registry_generation=17;d.native_level=level;
    std::snprintf(d.manifest_id,sizeof d.manifest_id,"%s",added?"veldin-casa":"");
    std::snprintf(d.name,sizeof d.name,"%s",added?"Veldin Casa":"Campaign");return d;
}
enum class Mode {Complete,CancelLease,TickError,StaleCommit,VariantInApproach,AvailabilityInApproach,VariantAtArrival};
struct Host {
    static Host* active;
    openrac_api api{};std::vector<unsigned char> ram=std::vector<unsigned char>(0x2000000);
    openrac_destination_context current{};std::array<openrac_destination,19> campaign{};
    openrac_destination added=descriptor(18,true),mounted{},prepared{};
    Session* session{};Mode mode=Mode::Complete;DestinationRoute requested=home;
    unsigned begins{},prepares{},commits{},cancels{},fades{},native_prepares{},leases{},closings{},originals{},errors{};
    unsigned order{},begin_order{},native_prepare_order{},fade_order{},lease_order{},closing_order{},commit_order{},original_order{};
    std::uint64_t ticket=501,token=701,nonce{},handoff=801,lease_epoch{},prepared_epoch{};
    std::uint32_t selected=1,owned_mask=14,snapshot_revision=1;
    int physical_class=531,loaded=3,staging=-1,observed_pending=-1;
    bool ready=true,home_available=true,current_ok=true,lease_open{},lease_success{},ticket_live{},token_live{},committed{};
    bool canonical_ready=true,canonical_final=false,canonical_stale=false;
    bool fail_begin{},fail_prepare{},fail_commit{},fade_available=true,draw_ack{};
    bool frame_frozen{},memory_frozen{},variant_cancelled{},availability_cancelled{},arrival_cancelled{},fresh_press_required{};
    openrac_destination_departure departure{};openrac_destination_request request{};
    std::vector<std::uint64_t> cancelled;
    explicit Host(DestinationRoute from={3},unsigned identity=1,int physical=531):selected(identity),physical_class(physical){
        active=this;for(int i=1;i<19;++i)campaign[unsigned(i)]=descriptor(i);
        mounted=from.variant==DestinationVariant::VeldinGarage?added:campaign[unsigned(from.native_level)];loaded=from.native_level;
        current.size=sizeof current;current.native_generation=9;current.origin=mounted;
        api.size=sizeof api;api.api_major=1;api.api_minor=14;api.memory=ram.data();api.game="rac1";api.version="pal";
        api.relocate_data=+[](unsigned at){return at;};api.relocate_level_data=+[](int,unsigned at){return at;};
        api.loaded_level=+[](){return active->loaded;};
        api.play_status=+[](openrac_play_status* out){if(!out||out->size<sizeof *out)return OPENRAC_HOOK_UNKNOWN;out->loads=active->current.native_generation;out->level=active->loaded;return OPENRAC_HOOK_OK;};
        api.setting_bool=+[](openrac_mod*,const char*){return 0;};
        api.log=+[](openrac_mod*,int severity,const char*){if(severity==OPENRAC_LOG_ERROR)++active->errors;};
        api.function=+[](const char* name)->void*{auto& h=*active;
            if(h.fade_available&&!std::strcmp(name,disc::pal.fade_to_black))return reinterpret_cast<void*>(&fade);
            if(!std::strcmp(name,disc::pal.prepare_target))return reinterpret_cast<void*>(&prepare_native);
            return nullptr;};
        api.transient_run=+[](openrac_mod* who,const openrac_transient_source* source){return active->transient(who,source);};
        api.destination_lookup=+[](const char* id,openrac_destination* out){auto& h=*active;
            require(out&&out->size==sizeof *out,"SDK lookup output is sized");
            if(!h.home_available||std::strcmp(id,"veldin-casa"))return OPENRAC_HOOK_UNKNOWN;*out=h.added;return OPENRAC_HOOK_OK;};
        api.destination_campaign=+[](int level,openrac_destination* out){auto& h=*active;
            require(out&&out->size==sizeof *out,"SDK campaign output is sized");
            if(level<1||level>18)return OPENRAC_HOOK_UNKNOWN;*out=h.campaign[unsigned(level)];return OPENRAC_HOOK_OK;};
        api.destination_current=+[](openrac_destination_context* out){auto& h=*active;
            require(out&&out->size==sizeof *out&&out->origin.size==sizeof out->origin,"SDK current context and nested descriptor are sized");
            if(!h.current_ok)return OPENRAC_HOOK_UNKNOWN;*out=h.current;return OPENRAC_HOOK_OK;};
        api.destination_begin_departure=+[](openrac_mod* who,const openrac_destination_departure* in,std::uint64_t* out){auto& h=*active;++h.begins;
            require(in&&in->size==sizeof *in&&out&&!*out,"SDK begin request and ticket output valid");
            require(!h.current.handoff_id&&!h.lease_open,"begin occurs before native continuation and lease");
            if(who!=owner||h.fail_begin||!in->nonce||in->nonce==h.nonce||in->native_generation!=h.current.native_generation||
                in->origin_handle!=h.current.origin.handle||in->registry_generation!=h.current.origin.registry_generation)return OPENRAC_HOOK_UNKNOWN;
            h.departure=*in;h.nonce=in->nonce;h.ticket_live=true;*out=h.ticket;h.begin_order=++h.order;return OPENRAC_HOOK_OK;};
        api.destination_prepare=+[](openrac_mod* who,const openrac_destination_request* in,std::uint64_t* out){auto& h=*active;++h.prepares;
            require(in&&in->size==sizeof *in&&out&&!*out,"SDK prepare receives sized request and empty output");
            if(who!=owner||h.fail_prepare||!h.lease_open||!h.current.handoff_id||h.token_live||
                in->handoff_id!=h.current.handoff_id||in->native_generation!=h.departure.native_generation||
                in->registry_generation!=h.departure.registry_generation||in->origin_handle!=h.departure.origin_handle||in->nonce!=h.departure.nonce)return OPENRAC_HOOK_UNKNOWN;
            bool found=false;for(const auto& d:h.campaign)if(d.handle&&d.handle==in->target_handle){h.prepared=d;found=true;}
            if(h.home_available&&h.added.handle==in->target_handle){h.prepared=h.added;found=true;}if(!found)return OPENRAC_HOOK_UNKNOWN;
            h.request=*in;h.token_live=true;h.prepared_epoch=h.lease_epoch;*out=++h.token;return OPENRAC_HOOK_OK;};
        api.destination_commit=+[](openrac_mod* who,std::uint64_t value){auto& h=*active;++h.commits;h.commit_order=++h.order;
            require(!h.lease_open&&h.closings==1,"commit follows exclusive scene closing");
            if(who!=owner||h.fail_commit||!h.lease_success||!h.token_live||value!=h.token||h.prepared_epoch!=h.lease_epoch)return OPENRAC_HOOK_UNKNOWN;
            h.token_live=false;h.committed=true;h.mounted=h.prepared;return OPENRAC_HOOK_OK;};
        api.destination_cancel=+[](openrac_mod* who,std::uint64_t value){auto& h=*active;++h.cancels;h.cancelled.push_back(value);
            if(who!=owner)return OPENRAC_HOOK_UNKNOWN;
            if(value==h.token&&h.token_live){h.token_live=false;return OPENRAC_HOOK_OK;}
            if(value==h.ticket){h.ticket_live=false;return OPENRAC_HOOK_OK;}
            return OPENRAC_HOOK_UNKNOWN;};
        put<float>(disc::pal.frame_scale.value,1);put<float>(disc::pal.frame_scale_squared.value,1);put<float>(disc::pal.frame_time.value,1.f/60);
        put<int>(disc::pal.cinematic_mode.value,6);put<unsigned>(disc::pal.shared_ship.value,0x20000);
        put<unsigned short>(0x200a6,static_cast<unsigned short>(physical_class));put<short>(disc::pal.shared_ship.value+disc::ship_variant_offset,short(std::clamp(physical_class-531,0,2)));
        put<unsigned char>(disc::pal.coordinates.value+1,1);put<unsigned char>(disc::pal.coordinates.value+3,1);
        update_provider();original=&native_original;
    }
    template<class T>void put(unsigned at,T value){std::memcpy(ram.data()+at,&value,sizeof value);}
    template<class T>T read(unsigned at)const{T value{};std::memcpy(&value,ram.data()+at,sizeof value);return value;}
    SceneAssets assets(){SceneAssets a;a.full_galaxy=true;return a;}
    void update_provider(){
        if(!publish)return; // The final cases run with the DLL actually absent.
        veldin_garage_snapshot_v1 s{};s.size=sizeof s;s.version=1;s.selected_ship=VELDIN_GARAGE_SHIP_NONE;s.revision=snapshot_revision++;
        std::memcpy(s.world_id,garage_world_id,std::strlen(garage_world_id)+1);std::memcpy(s.region,"pal",4);
        if(ready){s.flags=VELDIN_GARAGE_READY;s.owned_mask=owned_mask;s.selected_ship=selected;s.generation=current.native_generation;
            if(mounted.kind==OPENRAC_DESTINATION_ADDED&&!std::strcmp(mounted.manifest_id,"veldin-casa"))s.flags|=VELDIN_GARAGE_ACTIVE;
            if(current.handoff_id)s.flags|=VELDIN_GARAGE_TRAVELLING;}
        require(publish(&s,sizeof s)==1,"loaded test provider accepts the immutable snapshot fixture");
        if(publish_campaign){
            veldin_campaign_snapshot_v1 progress{};progress.size=sizeof progress;progress.version=VELDIN_CAMPAIGN_API_V1;progress.source_native_level=-1;
            if(ready&&canonical_ready){
                progress.flags=VELDIN_CAMPAIGN_READY;
                if(s.flags&VELDIN_GARAGE_ACTIVE)progress.flags|=VELDIN_CAMPAIGN_ACTIVE;
                if(s.flags&VELDIN_GARAGE_TRAVELLING)progress.flags|=VELDIN_CAMPAIGN_TRAVELLING;
                progress.generation=current.native_generation+(canonical_stale?1u:0u);progress.capture_revision=1;
                progress.source_generation=1;progress.source_native_level=3;
                progress.coordinates[1]=progress.coordinates[3]=1;progress.coordinates[18]=canonical_final?1:0;
                std::memcpy(progress.region,"pal",4);
            }
            require(publish_campaign(&progress,sizeof progress)==1,"canonical checkpoint fixture is separate from live native world flags");
        }
    }
    static void prepare_native(int target){auto& h=*active;++h.native_prepares;h.native_prepare_order=++h.order;
        require(h.session&&h.session->preparing_departure&&h.session->destination.has_departure()&&h.begins==1,"real Session captures SDK departure BEFORE calling native initializer");
        require(h.begin_order<h.native_prepare_order&&!h.current.handoff_id,"origin is retained with handoff0 before native take-off continuation");
        h.staging=target;h.put<int>(disc::pal.pending_destination.value,target);h.put<int>(disc::pal.shared_ship.value+0x20,3);h.put<int>(disc::pal.cinematic_mode.value,6);
    }
    static void fade(int frames){auto& h=*active;++h.fades;h.fade_order=++h.order;require(frames==12&&h.session&&h.session->in_scene,"native fade12 runs under Session reentry guard");
        const auto memory_before=h.ram;const auto ticks=h.session->scene_ticks;const auto world_frames=h.session->frames;
        for(int i=0;i<14;++i)h.session->frame();
        h.frame_frozen=h.ram==memory_before&&h.session->scene_ticks==ticks&&h.session->frames==world_frames;require(h.frame_frozen,"fade-pumped callbacks advance neither native memory nor flight/world counters");
    }
    static void native_original(){auto& h=*active;++h.originals;h.original_order=++h.order;
        require(h.session&&h.session->preserving_ship&&!h.lease_open,"original transition executes once outside lease with ship preservation");
        h.observed_pending=h.read<int>(disc::pal.pending_destination.value);
        require(h.observed_pending==h.mounted.native_level,"pending native number agrees with explicitly mounted logical destination");
        h.loaded=h.mounted.native_level;h.current.origin=h.mounted;++h.current.native_generation;h.current.handoff_id=0;
        h.put<int>(disc::pal.cinematic_mode.value,0);h.put<int>(disc::pal.shared_ship.value+0x20,8);h.update_provider();
    }
    int callback(const openrac_transient_source* source,unsigned short held=0){const unsigned char analog[4]{128,128,128,128};return source->tick(source->user,1.f/60,held,analog);}
    void place_near(int level){
        auto& v=session->voyage;const auto* d=find_destination(v.catalog_data(),v.catalog_count(),level);require(d!=nullptr,"host-only pose fixture chooses a catalog body");
        auto& f=const_cast<FlightState&>(v.flight());const Vec3 radial=unit(f.position-d->position);f.position=d->position+radial*(d->radius+VoyageController::keepout_margin+12.f);
        f.yaw=yaw_of(-radial);f.pitch=pitch_of(-radial);f.model_yaw=f.yaw;f.model_pitch=f.pitch;
    }
    void finish_approach(const openrac_transient_source* source){for(int i=0;i<65&&session->voyage.phase()!=Phase::Arrival;++i)require(callback(source)==1,"approach callback remains in lease");require(session->voyage.phase()==Phase::Arrival&&session->entered==requested.native_level,"real Session reaches pending Arrival");}
    int transient(openrac_mod* who,const openrac_transient_source* source){
        ++leases;lease_order=++order;require(who==owner&&source&&source->user==session&&source->tick&&source->closing,"exclusive lease receives production Session callbacks");
        require(fades==1&&fade_order<lease_order&&current.handoff_id&&session->in_scene,"lease follows original fade and native handoff context");
        lease_open=true;lease_success=false;++lease_epoch;const auto before=ram;
        if(mode!=Mode::CancelLease){
            if(mode==Mode::TickError){current_ok=false;require(callback(source)==0&&!session->error.empty(),"invalid host context stops production tick");current_ok=true;}
            else{
                for(int i=0;i<105;++i)require(callback(source)==1,"real native-duration intro advances through tick callback");
                require(session->voyage.phase()==Phase::FreeFlight,"intro completes without hand-edited phase");place_near(requested.native_level);
                require(callback(source,0x1000)==1&&session->voyage.phase()==Phase::Approach,"real Triangle begins approach from host-only near-body pose");
                require(session->voyage.target_route()==requested&&session->destination.prepared_route()==requested,"Session prepares the exact captured logical route during the lease");
                if(mode==Mode::VariantInApproach||mode==Mode::AvailabilityInApproach){
                    if(mode==Mode::VariantInApproach){put<unsigned char>(disc::pal.coordinates.value+18,1);require(callback(source,0x1000)==1,"coordinate fixture is observed on the next real tick");variant_cancelled=session->voyage.phase()==Phase::FreeFlight;}
                    else{home_available=false;require(callback(source,0x1000)==1,"registry revocation is observed on the next real tick");availability_cancelled=session->voyage.phase()==Phase::FreeFlight;}
                    require(!session->destination.prepared_route().valid()&&!token_live,"cancelled approach releases its selection token");
                    require(callback(source,0x1000)==1&&session->voyage.phase()==Phase::FreeFlight,"held Triangle cannot confirm a changed or unavailable route");
                    if(mode==Mode::AvailabilityInApproach)home_available=true;
                    require(callback(source,0x1000)==1&&session->voyage.phase()==Phase::FreeFlight,"restoring provider support cannot manufacture a new press");fresh_press_required=true;
                    callback(source,0);callback(source,0x1000);
                }
                if(session->voyage.phase()==Phase::Approach)finish_approach(source);
                require(session->voyage.phase()==Phase::Arrival,"fixture eventually reaches final presentation phase");
                if(mode==Mode::VariantAtArrival){
                    put<unsigned char>(disc::pal.coordinates.value+18,1);require(callback(source)==1,"Arrival observes late story coordinate change");
                    arrival_cancelled=session->voyage.phase()==Phase::FreeFlight&&session->entered==-1;
                    require(arrival_cancelled&&!session->destination.prepared_route().valid(),"stale added18 handoff is discarded instead of silently becoming campaign18");
                    callback(source,0);callback(source,0x1000);finish_approach(source);
                }
                require(callback(source)==1&&!session->arrival_presented,"catch-up cannot acknowledge the final draw itself");
                // Explicit CPU fixture acknowledgement. No successful GL draw
                // or actual black framebuffer is claimed by this test.
                session->arrival_presented=true;draw_ack=true;require(callback(source)==0,"explicit presentation acknowledgement yields to host");
            }
        }
        auto expected=before;if(mode==Mode::VariantInApproach||mode==Mode::VariantAtArrival)expected[disc::pal.coordinates.value+18]=1;
        memory_frozen=ram==expected;require(memory_frozen,"all32MiB remain byte-exact during scene except declared fixture coordinate mutation");
        source->closing(source->user);++closings;closing_order=++order;lease_open=false;lease_success=mode!=Mode::CancelLease;
        if(mode==Mode::StaleCommit)++lease_epoch;
        return lease_success?OPENRAC_HOOK_OK:OPENRAC_HOOK_UNKNOWN;
    }
    void board(Session& value,bool expect_query=true){session=&value;const auto query_before=provider_queries();session->frame();require(session->garage.bound()&&(!expect_query||provider_queries()>query_before),"production provider resolves the loaded DLL and queries whenever the logical origin is compatible");}
    void run(Session& value){
        board(value);require(session->armed&&session->voyage.phase()==Phase::Departure,"real boarding frame arms the voyage");
        put<int>(disc::pal.cinematic_mode.value,3);put<int>(disc::pal.menu.value,14);session->frame();require(native_prepares==1&&begins==1&&staging>=1,"real ship-menu frame retains origin then calls native take-off preparation");
        require(current.handoff_id==0,"native preparation itself does not fake a post-take-off context");
        current.handoff_id=handoff;ticket_live=false;update_provider();session->transition();
    }
};
Host* Host::active{};
}

int main(int argc,char** argv){
    require(argc==2,"pass absolute path to fake veldin_garage_preview.dll");
    const auto path=std::filesystem::absolute(argv[1]);require(std::filesystem::is_regular_file(path),"test provider DLL exists");
    require(GetModuleHandleW(L"veldin_garage_preview.dll")==nullptr,"unit process starts without any actual garage provider");
    HMODULE dll=LoadLibraryW(path.c_str());require(dll!=nullptr,"load the fixture DLL by absolute path");
    publish=export_pointer<Publish>(dll,"garage_fixture_snapshot");provider_queries=export_pointer<Queries>(dll,"garage_fixture_queries");
    publish_campaign=export_pointer<PublishCampaign>(dll,"garage_fixture_campaign");
    {
        Host h;Session s(&h.api,owner,disc::pal,h.assets());h.run(s);
        require(h.committed&&h.commits==1&&h.mounted.kind==OPENRAC_DESTINATION_ADDED&&h.mounted.handle==h.added.handle,"campaign origin commits registered Home18, not campaign18");
        require(h.observed_pending==18&&!h.read<unsigned char>(disc::pal.coordinates.value+18)&&h.draw_ack&&h.memory_frozen,"pre-final home entry never grants final coordinate and waits for explicit presentation ack");
        require(h.closings==1&&h.closing_order<h.commit_order&&h.commit_order<h.original_order&&h.originals==1,"closing precedes one commit and exactly one original continuation");
        s.frame();require(s.voyage.phase()==Phase::Ground,"native-world simulation acknowledges landing and clears voyage");
    }
    {
        Host h(home,2,532);h.requested={3};Session s(&h.api,owner,disc::pal,h.assets());h.run(s);
        require(h.committed&&h.originals==1&&s.ship==532&&h.mounted.kind==OPENRAC_DESTINATION_CAMPAIGN&&h.mounted.native_level==3,"actual Home source leaves for another campaign planet with the committed new Red ship");
        require(h.departure.origin_handle==h.added.handle,"Home departure retains its added manifest handle before native initializer");
    }
    {
        Host h(home,3,533);h.put<unsigned char>(disc::pal.coordinates.value+18,1);h.canonical_final=true;h.update_provider();h.requested=final;Session s(&h.api,owner,disc::pal,h.assets());h.run(s);
        require(h.committed&&s.ship==533&&h.mounted.kind==OPENRAC_DESTINATION_CAMPAIGN&&h.mounted.handle==h.campaign[18].handle,"Home enters final18 only when the canonical checkpoint, not the custom native template, grants it");
    }
    {
        Host h(home,3,533);h.put<unsigned char>(disc::pal.coordinates.value+18,1);h.requested=home;
        Session s(&h.api,owner,disc::pal,h.assets());h.run(s);
        require(h.committed&&h.mounted.kind==OPENRAC_DESTINATION_ADDED&&h.mounted.handle==h.added.handle,
            "Home local native18 flag cannot redirect a pre-final return to campaign18");
        require(!s.coordinates()[18]&&h.read<unsigned char>(disc::pal.coordinates.value+18)==1,
            "navigation uses canonical coordinates without rewriting custom world simulation RAM");
    }
    for(bool stale:{false,true}){
        Host h(home);h.canonical_ready=stale;h.canonical_stale=stale;h.update_provider();
        h.put<unsigned char>(disc::pal.coordinates.value+18,1);const auto before=h.ram;
        Session s(&h.api,owner,disc::pal,h.assets());h.board(s);
        require(!s.armed&&!s.destination.has_departure()&&h.ram==before&&h.begins==0&&h.native_prepares==0,
            "unready or stale canonical Home progress cannot consume native local18 flags or start a departure");
        if(!stale){
            require(s.awaiting_fleet&&!s.native_only_departure,"a still-pending canonical publication can become ready before the handoff");
            h.canonical_ready=true;h.update_provider();s.frame();
            require(s.armed&&!s.awaiting_fleet&&s.voyage.resolved_route(18)==home,
                "late canonical readiness confirms the Home route without inheriting its local final flag");
        }else require(s.native_only_departure&&!s.awaiting_fleet,"stale progress is a refusal, not a fallback to raw Home state");
    }
    {
        Host h({3},VELDIN_GARAGE_SHIP_ORIGINAL,531);h.owned_mask=15;h.update_provider();
        auto assets=h.assets();assets.original_ship.available=true;
        const auto before=h.ram;Session s(&h.api,owner,disc::pal,std::move(assets));h.board(s);
        require(!s.armed&&s.native_only_departure&&h.begins==0&&h.native_prepares==0&&h.ram==before,"a legacy hull-only Original pack cannot launch an empty cockpit or silently replace it with Green");
    }
    for(const auto from:{DestinationRoute{3},home})for(int physical:{531,2047}){
        Host h(from,VELDIN_GARAGE_SHIP_ORIGINAL,physical);h.owned_mask=15;h.update_provider();
        h.requested=from.variant==DestinationVariant::VeldinGarage?DestinationRoute{3}:home;
        auto assets=h.assets();assets.original_ship.available=assets.original_ship.cockpit_available=true;Session s(&h.api,owner,disc::pal,std::move(assets));h.run(s);
        require(h.committed&&s.identity==ShipIdentity::Original&&s.voyage.ship_identity()==ShipIdentity::Original&&s.ship==531,"verified Original retains a separate appearance identity with its actual native carrier");
        require(!s.voyage.combat().state().armed,"Original cannot inherit Green or armed-ship weapon metadata");
    }
    {
        Host h({3},VELDIN_GARAGE_SHIP_GREEN,2047);Session s(&h.api,owner,disc::pal,h.assets());h.board(s);
        require(!s.armed&&s.native_only_departure&&h.begins==0,"class2047 is not a Green carrier or ownership proof");
    }
    {
        Host h({3},VELDIN_GARAGE_SHIP_ORIGINAL,531);h.owned_mask=15;h.ready=false;h.update_provider();
        auto assets=h.assets();assets.original_ship.available=assets.original_ship.cockpit_available=true;Session s(&h.api,owner,disc::pal,std::move(assets));h.board(s);
        require(!s.armed&&s.awaiting_fleet&&!s.native_only_departure,"first callback waits for provider ownership without guessing Green or irreversibly rejecting the boarding");
        h.ready=true;h.update_provider();h.put<int>(disc::pal.cinematic_mode.value,3);h.put<int>(disc::pal.menu.value,14);s.frame();
        require(s.armed&&!s.awaiting_fleet&&s.identity==ShipIdentity::Original&&h.begins==1&&h.native_prepares==1,"fresh READY at the real postboarding menu confirms Original before bypass and SDK begin");
        h.current.handoff_id=h.handoff;h.ticket_live=false;h.update_provider();s.transition();
        require(h.committed&&s.identity==ShipIdentity::Original&&h.observed_pending==18,"late authoritative Original completes the same fake host handoff without selection UI");
    }
    {
        Host h;h.ready=false;h.update_provider();Session s(&h.api,owner,disc::pal,h.assets());h.board(s);
        require(s.awaiting_fleet&&!s.armed,"unverified first callback is pending rather than Green");
        h.put<int>(disc::pal.cinematic_mode.value,3);h.put<int>(disc::pal.menu.value,14);const auto before=h.ram;s.frame();
        require(!s.armed&&!s.awaiting_fleet&&s.native_only_departure&&h.begins==0&&h.native_prepares==0&&h.ram==before,"missing READY at the actual handoff never starts an unidentified journey");
    }
    for(bool changed_identity:{false,true}){
        Host h;auto assets=h.assets();assets.original_ship.available=assets.original_ship.cockpit_available=true;Session s(&h.api,owner,disc::pal,std::move(assets));h.board(s);
        if(changed_identity){h.selected=0;h.owned_mask=15;}else h.ready=false;h.update_provider();
        h.put<int>(disc::pal.cinematic_mode.value,3);h.put<int>(disc::pal.menu.value,14);const auto before=h.ram;s.frame();
        require(!s.armed&&s.native_only_departure&&h.begins==0&&h.native_prepares==0&&h.ram==before,"the handoff rechecks READY and exact captured identity, even when Original and Green share the same carrier");
    }
    for(auto mode:{Mode::CancelLease,Mode::TickError,Mode::StaleCommit}){
        Host h(home);h.mode=mode;Session s(&h.api,owner,disc::pal,h.assets());h.run(s);
        require(!h.committed&&h.originals==1&&h.observed_pending==18&&h.mounted.kind==OPENRAC_DESTINATION_ADDED&&h.mounted.handle==h.added.handle,"cancel/error/stale commit return to the same Home18 mount without changing to campaign18");
        require(!s.armed&&!s.in_scene&&!s.destination.has_departure()&&!s.entered_route.valid()&&s.voyage.phase()==Phase::Ground,"fallback clears all owned Session/client handoff state");
        require(h.mode!=Mode::StaleCommit||h.commits==1,"stale token is offered at most once");
    }
    for(auto mode:{Mode::VariantInApproach,Mode::AvailabilityInApproach,Mode::VariantAtArrival}){
        Host h;h.mode=mode;Session s(&h.api,owner,disc::pal,h.assets());h.run(s);
        require(h.committed&&h.cancels>=1&&h.originals==1,"revoked selection can retry through the same owned departure and lease");
        require(mode!=Mode::VariantInApproach||h.variant_cancelled,"coordinate-driven Home/final switch cancels original approach");
        require(mode!=Mode::AvailabilityInApproach||h.availability_cancelled,"provider entry revocation cancels original approach");
        require(mode!=Mode::VariantAtArrival||h.arrival_cancelled,"late variant switch cancels cached Arrival");
        require(mode==Mode::VariantAtArrival||h.fresh_press_required,"retry after approach revocation required a released and pressed Triangle");
    }
    for(unsigned scenario=0;scenario<4;++scenario){
        Host h(scenario==2?home:DestinationRoute{3});
        if(scenario==0){h.selected=0;h.owned_mask=15;}
        if(scenario==1){h.physical_class=532;h.put<unsigned short>(0x200a6,532);}
        if(scenario==2){std::snprintf(h.mounted.manifest_id,sizeof h.mounted.manifest_id,"foreign-world");h.current.origin=h.mounted;}
        if(scenario==3)h.current.native_generation=10; // provider publication below stays deliberately old.
        if(scenario!=3)h.update_provider();
        const auto before=h.ram;Session s(&h.api,owner,disc::pal,h.assets());h.board(s,scenario!=2);
        if(scenario<3)require(!s.armed&&s.native_only_departure&&h.begins==0&&h.native_prepares==0&&h.ram==before,"Original/mismatched selection or unknown added world retains untouched native boarding");
        else require(!s.armed&&s.awaiting_fleet&&!s.native_only_departure&&!s.voyage.resolved_route(18).valid(),"a present provider with stale generation waits without granting a fallback ship identity");
    }
    for(unsigned minor:{13u,14u})for(const auto from:{DestinationRoute{3},home})for(unsigned state=0;state<4;++state){
        // Native531 can be Original's hidden controller. Cover its positively
        // identified Original snapshot, an expired generation, invalid ownership
        // and a valid cleared/NotReady snapshot without guessing Green.
        Host h(from,VELDIN_GARAGE_SHIP_ORIGINAL,531);h.owned_mask=15;
        h.api.api_minor=static_cast<std::uint16_t>(minor);
        if(minor==13)h.api.size=offsetof(openrac_api,destination_lookup);
        if(state==2)h.owned_mask=14;
        if(state==3)h.ready=false;
        h.update_provider();if(state==1)++h.current.native_generation;
        const auto before=h.ram;Session s(&h.api,owner,disc::pal,h.assets());h.board(s);
        require(!s.armed&&(s.native_only_departure||s.awaiting_fleet)&&s.voyage.phase()==Phase::Ground&&!s.destination.has_departure(),"Original/unknown/stale/invalid531 identity never guesses Green on SDK13 and14");
        require(h.ram==before&&h.begins==0&&h.native_prepares==0&&h.leases==0&&h.originals==0,"identity refusal changes no native bytes, menu or loader state");
        // READY can be published after our first callback. Accept a freshly
        // verified identity before the handoff, but never undo a final refusal.
        h.ready=true;h.owned_mask=14;h.selected=1;h.update_provider();s.frame();
        const bool can_confirm=state!=0&&(minor==14||from.variant==DestinationVariant::Campaign);
        require(h.ram==before&&(can_confirm?(s.armed&&!s.awaiting_fleet&&s.identity==ShipIdentity::Green):(!s.armed&&s.native_only_departure)),"late READY confirms a still-pending boarding but cannot reinterpret an already-refused departure");
    }
    {
        Host h(home);h.fail_begin=true;Session s(&h.api,owner,disc::pal,h.assets());h.board(s);h.put<int>(disc::pal.menu.value,14);const auto before=h.ram;s.frame();
        require(!s.armed&&s.native_only_departure&&h.native_prepares==0&&h.ram==before&&h.mounted.kind==OPENRAC_DESTINATION_ADDED,"failed SDK origin retention does not close the ship menu or start staging");
    }
    {
        Host h;Session s(&h.api,owner,disc::pal,h.assets());require(s.garage.bound(),"real provider retained loaded fixture");
        FreeLibrary(dll);dll=nullptr;require(GetModuleHandleW(L"veldin_garage_preview.dll")!=nullptr,"GarageProvider pins provider code after original loader releases its reference");
        h.board(s);require(s.armed,"query remains callable while production provider owns the module pin");
        s.garage.close();require(GetModuleHandleW(L"veldin_garage_preview.dll")==nullptr,"close unbinds observer before releasing final module reference");
    }
    publish=nullptr;provider_queries=nullptr;publish_campaign=nullptr;
    for(unsigned minor:{13u,14u})for(int physical:{531,532,533}){
        Host h({3},unsigned(physical-530),physical);h.api.api_minor=static_cast<std::uint16_t>(minor);
        if(minor==13)h.api.size=offsetof(openrac_api,destination_lookup);
        require(GetModuleHandleW(L"veldin_garage_preview.dll")==nullptr,"campaign fallback fixture has no provider DLL loaded");
        const auto before=h.ram;Session s(&h.api,owner,disc::pal,h.assets());h.session=&s;s.frame();
        require(!s.garage.bound()&&s.armed&&!s.native_only_departure&&s.ship==physical&&s.voyage.phase()==Phase::Departure,"provider-absent campaign boarding retains all3 stock ships on SDK13 and14");
        require(s.voyage.origin_route()==DestinationRoute{3}&&!s.voyage.resolved_route(18).valid()&&h.ram==before,"absent provider preserves campaign provenance without enabling Home or changing native bytes");
        require(s.destination.supported()==(minor==14)&&h.begins==0&&h.native_prepares==0,"SDK13 optional degradation and SDK14 capability detection remain independent of provider absence");
    }
    std::printf("session_destination: %u production Session/Windows provider/SDK14 handoff checks passed; fake registry and explicit presentation ack, no game/GPU/save compatibility proof\n",checks);
    return 0;
}
