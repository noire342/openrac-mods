// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "arsenal_state.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace arsenal;
namespace {
int checks;
void require(bool pass,const char* message){++checks;if(!pass){std::fprintf(stderr,"arsenal: %s\n",message);std::exit(1);}}
Descriptor revolverator(){return {RevolveratorKey,"noire342.revolverator",18,60037,10,15000,12};}
void seed(Registry& registry){
    require(registry.register_weapon(revolverator()),"register a distinct logical Revolverator");
    require(registry.set_walloper({true,7,2,0,5}),"capture native Walloper flags without native memory access");
    require(registry.set_state(RevolveratorKey,{true,10,39}),"grant independent custom ammunition and XP");
}
bool same(const Snapshot& a,const Snapshot& b){
    if(a.format!=b.format||a.equipped!=b.equipped||a.quick!=b.quick||a.records.size()!=b.records.size())return false;
    if(a.walloper.owned!=b.walloper.owned||a.walloper.unlocked!=b.walloper.unlocked||
       a.walloper.acquired!=b.walloper.acquired||a.walloper.ammo!=b.walloper.ammo||a.walloper.text_variant!=b.walloper.text_variant)return false;
    for(std::size_t i=0;i<a.records.size();++i){const auto& x=a.records[i];const auto& y=b.records[i];
        if(x.key!=y.key||x.stable_id!=y.stable_id||x.state.owned!=y.state.owned||x.state.ammo!=y.state.ammo||x.state.xp!=y.state.xp)return false;}
    return true;
}
}
int main(){
    {Registry r;require(!r.owned(18)&&!r.equip(18),"unowned native Walloper cannot be equipped");
     require(r.register_weapon(revolverator())&&!r.owned(RevolveratorKey)&&!r.equip(RevolveratorKey),"registration alone never grants a custom weapon");
     require(!r.assign_quick(0,RevolveratorKey),"unowned custom weapon cannot enter favourites");
     require(r.equip(36)&&r.equip(8)&&!r.equip(37),"only bounded native IDs bypass the adapter's external ownership check");}
    {Registry r;seed(r);const auto native=r.walloper();
     require(r.equip(RevolveratorKey)&&r.equipped()==RevolveratorKey,"equipping retains custom identity rather than its carrier");
     require(r.set_state(RevolveratorKey,{true,3,3500}),"spend custom ammunition independently");
     require(r.walloper().ammo==native.ammo&&r.walloper().unlocked==7&&r.walloper().acquired==2&&r.walloper().text_variant==5,
             "custom progression never contaminates Walloper state");
     require(r.assign_quick(0,18)&&r.assign_quick(1,RevolveratorKey),"both logical weapons occupy independent favourites");
     require(r.quick()[0]==18&&r.quick()[1]==RevolveratorKey&&r.native_slots()[0]==18&&r.native_slots()[1]==18,
             "native duplicate carriers do not collapse distinct logical favourites");
     require(r.assign_quick(7,RevolveratorKey)&&r.quick()[1]==0&&r.quick()[7]==RevolveratorKey&&r.quick()[0]==18,
             "moving a duplicate logical key preserves the independent Walloper favourite");
     const auto revision=r.revision();require(r.equip(RevolveratorKey)&&r.revision()==revision,"idempotent equip does not churn revisions");
     require(!r.assign_quick(8,18)&&!r.assign_quick(0,65535),"invalid slot and reserved logical key are rejected");
     require(r.set_state(RevolveratorKey,{false,3,3500})&&r.equipped()==8&&r.quick()[7]==0&&r.quick()[0]==18,
             "removing a custom weapon unbinds only that logical identity");
     require(r.set_walloper({false,7,2,0,5})&&r.quick()[0]==0&&!r.equip(18),"native Walloper removal keeps equip and favourite invariants");}
    {Registry r;seed(r);r.assign_quick(0,18);r.assign_quick(1,RevolveratorKey);r.equip(RevolveratorKey);r.seal();
     const auto saved=r.snapshot();Registry restored;require(restored.register_weapon(revolverator()),"restore registry is described independently");restored.seal();
     require(restored.restore(saved)&&same(saved,restored.snapshot()),"snapshot round trip preserves both inventories, favourites and equipped identity");
     const auto revision=restored.revision();require(restored.restore(saved)&&restored.revision()==revision,"identical restores do not churn revisions");
     const auto reject=[&](Snapshot bad,const char* why){const auto baseline=restored.snapshot();const auto rev=restored.revision();
        require(!restored.restore(bad)&&same(restored.snapshot(),baseline)&&restored.revision()==rev,why);};
     auto bad=saved;bad.format=2;reject(bad,"unsupported format fails atomically");
     bad=saved;bad.records[0].state.ammo=11;reject(bad,"over-cap ammunition cannot be silently clamped or accepted");
     bad=saved;bad.records[0].state.ammo=std::numeric_limits<unsigned>::max();reject(bad,"serialized negative/wrapped custom ammunition is rejected");
     bad=saved;bad.walloper.ammo=-1;reject(bad,"negative native ammunition fails atomically");
     bad=saved;bad.records[0].state.xp=Registry::MaximumXp+1;reject(bad,"XP beyond the storage contract is rejected");
     bad=saved;bad.records.push_back(bad.records[0]);reject(bad,"duplicate custom key/state cannot be restored twice");
     bad=saved;bad.records[0].stable_id="other.owner";reject(bad,"a persisted key cannot masquerade as another weapon");
     bad=saved;bad.records[0].key=0xffff;reject(bad,"reserved keys cannot enter persisted custom state");
     bad=saved;bad.quick[2]=RevolveratorKey;reject(bad,"duplicate logical favourite is rejected while carrier duplicates remain valid");
     bad=saved;bad.records[0].state.owned=false;reject(bad,"unowned equipped custom weapon cannot be restored");
     bad=saved;bad.quick[7]=0x12345;reject(bad,"orphan references require a matching retained state record");
     bad=saved;bad.records.resize(Registry::MaxSnapshotRecords+1);reject(bad,"oversized record input is bounded before mutation");}
    {Registry absent;Snapshot saved;saved.records={{RevolveratorKey,"noire342.revolverator",{true,4,92}}};
     saved.quick[3]=RevolveratorKey;saved.equipped=RevolveratorKey;
     require(absent.restore(saved)&&same(absent.snapshot(),saved),"removed mod state and logical favourite positions survive losslessly");
     require(absent.equipped()==8&&absent.native_slots()[3]==0&&!absent.owned(RevolveratorKey)&&!absent.equip(RevolveratorKey),
             "uninstalled descriptors cannot project unsafe carriers or become selectable");
     require(absent.register_weapon(revolverator())&&absent.equipped()==RevolveratorKey&&absent.native_slots()[3]==18&&absent.state(RevolveratorKey)->ammo==4,
             "reinstalling a descriptor reactivates its preserved state without inventing ammunition");
     absent.seal();auto d=revolverator();d.key+=1;d.stable_id="other";
     require(!absent.register_weapon(d),"descriptor registry cannot mutate after the session is sealed");}
    {Registry r;auto d=revolverator();d.key=18;require(!r.register_weapon(d),"custom registration cannot steal a native item ID");
     d=revolverator();d.carrier=9;require(!r.register_weapon(d),"unimplemented carrier adapters are rejected explicitly");
     d=revolverator();d.capacity=65536;require(!r.register_weapon(d),"capacity must fit native metadata without truncation");
     d=revolverator();d.price=0xffffffffu;require(!r.register_weapon(d),"price cannot wrap a native signed bolt amount");
     d=revolverator();d.first_level=19;require(!r.register_weapon(d),"sale level must exist on the supported game");
     d=revolverator();d.stable_id="bad id";require(!r.register_weapon(d),"stable identity is not an arbitrary display label");
     require(r.register_weapon(revolverator()),"valid descriptor remains registerable after invalid attempts");
     d=revolverator();d.key+=2;require(!r.register_weapon(d),"duplicate stable ID is rejected even with a different logical key");
     d=revolverator();d.stable_id="other";require(!r.register_weapon(d),"duplicate logical key is rejected even with a new stable ID");}
    {Registry r;for(unsigned i=0;i<Registry::MaxDescriptors;++i){auto d=revolverator();d.key=0x20000+i;d.stable_id="test."+std::to_string(i);
        require(r.register_weapon(d),"bounded registry accepts every supported distinct descriptor");}
     auto d=revolverator();require(!r.register_weapon(d)&&r.definitions().size()==64,"descriptor count is capped at64 without losing earlier registrations");}
    std::printf("arsenal: %d independent-state, logical-favourite, collision and atomic-restore checks passed\n",checks);
}
