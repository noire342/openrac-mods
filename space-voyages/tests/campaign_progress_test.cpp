// SPDX-License-Identifier: GPL-3.0-or-later
// Canonical checkpoint observer only: no native RAM, card, DLL or game.
#include "campaign_progress.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
using namespace spacevoyages;
namespace {
unsigned checks{},calls{};std::size_t allocations{};
veldin_campaign_snapshot_v1 published{};
int query_result=1;
void require(bool value,const char* why){++checks;if(!value){std::fprintf(stderr,"campaign_progress: %s\n",why);std::exit(1);}}
veldin_campaign_snapshot_v1 empty(){
    veldin_campaign_snapshot_v1 s{};s.size=sizeof s;s.version=VELDIN_CAMPAIGN_API_V1;
    s.source_native_level=-1;return s;
}
veldin_campaign_snapshot_v1 ready(){
    auto s=empty();s.flags=VELDIN_CAMPAIGN_READY|VELDIN_CAMPAIGN_ACTIVE;
    s.generation=9;s.capture_revision=1;s.source_generation=7;s.source_native_level=2;
    std::memcpy(s.region,"pal",4);
    for(unsigned i=0;i<19;++i){s.coordinates[i]=std::uint8_t(i<8);s.visits[i]=std::uint8_t(i*13);}
    return s;
}
int query(veldin_campaign_snapshot_v1* out,std::uint32_t bytes){
    ++calls;require(out&&bytes==80,"query uses its own exact 80-byte v1 buffer");
    if(query_result!=1)return query_result;
    *out=published;return 1;
}
int throws(veldin_campaign_snapshot_v1*,std::uint32_t){throw 7;}
bool cleared(const CampaignObservation& value){
    if(value.ready||value.active||value.travelling||value.generation||value.source_generation||
        value.capture_revision||value.source_native_level!=-1)return false;
    for(const auto coordinate:value.coordinates)if(coordinate)return false;
    for(const auto visit:value.visits)if(visit)return false;
    return true;
}
void rejected(const veldin_campaign_snapshot_v1& invalid,CampaignReadStatus expected=CampaignReadStatus::InvalidSnapshot){
    CampaignObservation out;out.ready=true;out.coordinates.fill(true);out.visits.fill(255);
    require(decode_campaign_snapshot(invalid,CampaignRegion::Pal,9,out)==expected,"malformed, short or stale snapshot rejected");
    require(cleared(out),"every decode failure clears previously exposed progress");
}
}
void* operator new(std::size_t bytes){++allocations;if(auto* p=std::malloc(bytes?bytes:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t bytes){return ::operator new(bytes);}
void operator delete(void* p)noexcept{std::free(p);}void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
int main(){
    require(sizeof(veldin_campaign_snapshot_v1)==80,"reviewed fixed-width ABI remains 80 bytes");
    const auto source=ready();CampaignObservation out{};
    require(decode_campaign_snapshot(source,CampaignRegion::Pal,9,out)==CampaignReadStatus::Ready,"positive PAL snapshot accepted");
    require(out.ready&&out.active&&!out.travelling&&out.generation==9&&out.source_generation==7&&
        out.capture_revision==1&&out.source_native_level==2,"current Home generation and canonical source remain separate");
    for(unsigned i=0;i<19;++i)require(out.coordinates[i]==(i<8)&&out.visits[i]==std::uint8_t(i*13),"all 19 owned coordinates and raw visit bytes preserved");
    auto s=source;s.flags|=VELDIN_CAMPAIGN_TRAVELLING;
    require(decode_campaign_snapshot(s,CampaignRegion::Pal,9,out)==CampaignReadStatus::Ready&&out.travelling,"boarding travel flag may overlap active state");
    s=source;s.flags=VELDIN_CAMPAIGN_READY;
    require(decode_campaign_snapshot(s,CampaignRegion::Pal,9,out)==CampaignReadStatus::Ready&&!out.active,"pure decoder distinguishes READY from ACTIVE");
    for(unsigned size=0;size<80;++size){s=source;s.size=size;rejected(s);}
    for(unsigned size:{81u,176u,std::numeric_limits<unsigned>::max()}){s=source;s.size=size;rejected(s);}
    for(unsigned version:{0u,2u,std::numeric_limits<unsigned>::max()}){s=source;s.version=version;rejected(s);}
    for(unsigned bit=3;bit<32;++bit){s=source;s.flags|=1u<<bit;rejected(s);}
    for(unsigned i=0;i<2;++i){s=source;s.reserved[i]=1;rejected(s);}
    for(unsigned i=0;i<19;++i)for(unsigned byte=2;byte<256;++byte){s=source;s.coordinates[i]=std::uint8_t(byte);rejected(s);}
    for(unsigned i=0;i<19;++i)for(unsigned byte=0;byte<256;++byte){
        s=source;s.visits[i]=std::uint8_t(byte);
        require(decode_campaign_snapshot(s,CampaignRegion::Pal,9,out)==CampaignReadStatus::Ready&&out.visits[i]==byte,"visits accept every native byte value without boolean normalization");
    }
    s=source;s.generation=0;rejected(s);s=source;s.capture_revision=0;rejected(s);
    s=source;s.source_generation=0;rejected(s);s=source;s.source_generation=10;rejected(s);
    for(int level:{-1,19,std::numeric_limits<int>::max()}){s=source;s.source_native_level=level;rejected(s);}
    for(int level=0;level<19;++level){s=source;s.source_native_level=level;require(decode_campaign_snapshot(s,CampaignRegion::Pal,9,out)==CampaignReadStatus::Ready,"source level bounds include 0 and 18");}
    s=source;s.generation=8;rejected(s,CampaignReadStatus::StaleGeneration);
    require(decode_campaign_snapshot(source,CampaignRegion::Pal,0,out)==CampaignReadStatus::StaleGeneration&&cleared(out),"zero expected native generation never authorizes a query");
    for(const auto tag:{"ntsc","PAL","pa","","palx"}){s=source;std::memset(s.region,0,sizeof s.region);std::memcpy(s.region,tag,std::strlen(tag));rejected(s,CampaignReadStatus::WrongRegion);}
    s=source;std::memset(s.region,'x',sizeof s.region);rejected(s,CampaignReadStatus::WrongRegion);
    for(auto region:{CampaignRegion::Ntsc,CampaignRegion::Unknown,static_cast<CampaignRegion>(99)})
        require(decode_campaign_snapshot(source,region,9,out)==CampaignReadStatus::WrongRegion&&cleared(out),"PAL canonical data cannot authorize another region");
    auto none=empty();require(decode_campaign_snapshot(none,CampaignRegion::Pal,9,out)==CampaignReadStatus::NotReady&&cleared(out),"compatible unready query is not progress authority");
    for(unsigned bit=1;bit<3;++bit){s=none;s.flags=1u<<bit;rejected(s);}
    s=none;s.generation=9;rejected(s);s=none;s.source_generation=7;rejected(s);
    s=none;s.capture_revision=1;rejected(s);s=none;s.source_native_level=0;rejected(s);
    for(unsigned i=0;i<19;++i){s=none;s.coordinates[i]=1;rejected(s);s=none;s.visits[i]=255;rejected(s);}
    for(unsigned i=0;i<8;++i){s=none;s.region[i]='x';rejected(s);}

    CampaignProgress progress;
    require(!progress.bound()&&progress.poll(9)==CampaignReadStatus::Unavailable&&!calls,"unbound observer performs no query");
    published=source;progress.bind(query);
    require(progress.bound()&&progress.poll(9)==CampaignReadStatus::Ready&&calls==1,"one read-only poll accepts a coherent active Home");
    std::array<bool,19> native_coordinates{};native_coordinates[18]=true;
    require(progress.copy_coordinates(native_coordinates)&&!native_coordinates[18],"raw Home coordinate18=1 is replaced by canonical18=0");
    require(!progress.observation().coordinates[18],"canonical Home progress does not unlock final Veldin");
    native_coordinates[0]=false;
    require(progress.observation().coordinates[0],"caller mutation of a copied array cannot change observer state");
    std::array<std::uint8_t,19> visits{};
    require(progress.copy_visits(visits)&&visits[18]==source.visits[18],"visit getter returns owned validated bytes");
    const auto provider_before=published;const auto before=allocations;
    for(unsigned i=0;i<10000;++i)require(progress.poll(9)==CampaignReadStatus::Ready,"unchanged capture remains usable");
    require(allocations==before&&std::memcmp(&provider_before,&published,sizeof published)==0,"polls allocate nothing and never change provider-owned source");
    published.flags|=VELDIN_CAMPAIGN_TRAVELLING;
    require(progress.poll(9)==CampaignReadStatus::Ready&&progress.observation().travelling,"flags may change without changing immutable capture revision");
    published.generation=10;
    require(progress.poll(10)==CampaignReadStatus::Ready&&progress.observation().source_generation==7,"current world may advance while canonical checkpoint stays frozen");
    published.generation=9;
    require(progress.poll(9)==CampaignReadStatus::StaleGeneration&&cleared(progress.observation()),"matching caller generation cannot resurrect a previous load");
    published.generation=10;published.coordinates[18]=1;
    require(progress.poll(10)==CampaignReadStatus::InconsistentRevision&&cleared(progress.observation()),"same capture revision cannot replace canonical coordinates with Home RAM");
    published=source;published.generation=10;published.visits[3]^=1;
    require(progress.poll(10)==CampaignReadStatus::InconsistentRevision,"same capture revision also freezes raw visited state");
    published=source;published.generation=10;published.source_generation=8;
    require(progress.poll(10)==CampaignReadStatus::InconsistentRevision,"same capture revision cannot relabel its source world");
    published=source;published.generation=10;published.capture_revision=2;published.source_native_level=3;
    require(progress.poll(10)==CampaignReadStatus::InconsistentRevision,"one source generation cannot name two native worlds");
    published=source;published.generation=10;published.capture_revision=2;published.coordinates[18]=1;
    require(progress.poll(10)==CampaignReadStatus::Ready&&progress.observation().coordinates[18],"fresh canonical capture may legitimately unlock final Veldin");
    published.capture_revision=1;
    require(progress.poll(10)==CampaignReadStatus::StaleRevision&&cleared(progress.observation()),"old capture is rejected even in the current load");
    published.capture_revision=3;published.source_generation=6;
    require(progress.poll(10)==CampaignReadStatus::StaleSourceGeneration,"new revision cannot resurrect an older source generation");
    published=source;published.generation=11;published.source_generation=11;published.source_native_level=8;published.capture_revision=3;
    require(progress.poll(11)==CampaignReadStatus::Ready&&progress.observation().source_native_level==8,"fresh checkpoint after campaign reload accepts new source level and generation");
    published.capture_revision=4;published.flags=VELDIN_CAMPAIGN_READY;
    require(progress.poll(11)==CampaignReadStatus::Inactive&&cleared(progress.observation()),"READY checkpoint outside logical Home is unavailable for Home bypass");
    published.flags|=VELDIN_CAMPAIGN_ACTIVE;
    require(progress.poll(11)==CampaignReadStatus::Ready,"late ACTIVE state becomes usable without replacing checkpoint");
    published=none;
    require(progress.poll(11)==CampaignReadStatus::NotReady&&cleared(progress.observation()),"provider clear removes progress and begins a new capture lifetime");
    published=source;published.generation=1;published.source_generation=1;published.capture_revision=1;published.source_native_level=0;
    require(progress.poll(1)==CampaignReadStatus::Ready,"explicit unready reset permits a fresh campaign with lower counters");
    query_result=0;require(progress.poll(1)==CampaignReadStatus::QueryFailed&&cleared(progress.observation()),"failed query clears last good output");
    native_coordinates.fill(true);visits.fill(255);
    require(!progress.copy_coordinates(native_coordinates)&&!progress.copy_visits(visits),"failed state cannot copy previously validated arrays");
    for(unsigned i=0;i<19;++i)require(!native_coordinates[i]&&!visits[i],"failed getters clear all caller output");
    query_result=2;require(progress.poll(1)==CampaignReadStatus::QueryFailed,"only exact query result1 is success");query_result=1;
    published.size=79;require(progress.poll(1)==CampaignReadStatus::InvalidSnapshot,"short provider snapshot is rejected after full-size caller query");
    progress.bind(throws);require(progress.poll(9)==CampaignReadStatus::QueryFailed&&cleared(progress.observation()),"throwing provider cannot expose cached progress");
    progress.bind(nullptr);const auto old_calls=calls;
    require(!progress.bound()&&progress.poll(9)==CampaignReadStatus::Unavailable&&calls==old_calls,"missing optional export clears state and never queries");
    published=source;progress.bind(query);require(progress.poll(9)==CampaignReadStatus::Ready,"new retained provider lifetime resets stale history");
    progress.unbind();require(!progress.bound()&&cleared(progress.observation())&&progress.status()==CampaignReadStatus::Unavailable,"unbind clears pointer, status and all progress before provider unload");
    CampaignProgress ntsc(CampaignRegion::Ntsc);ntsc.bind(query);
    require(ntsc.poll(9)==CampaignReadStatus::WrongRegion&&cleared(ntsc.observation()),"NTSC ordinary campaign cannot use optional PAL checkpoint service");
    std::printf("campaign_progress: %u ABI, canonical authority, raw visits, stale/fresh capture, lifecycle and no-allocation checks passed (CPU only)\n",checks);
}
