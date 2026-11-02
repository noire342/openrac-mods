// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#include "pause_registry.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <vector>
namespace arsenal_compat {
namespace {
bool bounded(const char* value,std::size_t capacity){return value&&*value&&std::memchr(value,0,capacity);}
bool label(const char (&value)[96]){
 if(!bounded(value,sizeof value))return false;
 const auto* p=reinterpret_cast<const unsigned char*>(value);
 for(std::size_t i=0;p[i];){const auto c=p[i++];if(c<0x20||c==0x7f)return false;if(c<0x80)continue;
  unsigned continuation{};std::uint32_t cp{},minimum{};
  if(c>=0xc2&&c<=0xdf){continuation=1;cp=c&31u;minimum=0x80;}
  else if(c>=0xe0&&c<=0xef){continuation=2;cp=c&15u;minimum=0x800;}
  else if(c>=0xf0&&c<=0xf4){continuation=3;cp=c&7u;minimum=0x10000;}else return false;
  for(unsigned n=0;n<continuation;++n){if(i>=sizeof value||p[i]<0x80||p[i]>0xbf)return false;cp=(cp<<6)|(p[i++]&63u);}
  if(cp<minimum||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff))return false;
 }return true;
}
arsenal_pause_entry_v1 copied(const arsenal_pause_entry_v1& value){
 arsenal_pause_entry_v1 out{};out.size=sizeof out;out.flags=value.flags;out.id=value.id;out.order=value.order;
 std::memcpy(out.label_en,value.label_en,std::strlen(value.label_en)+1);std::memcpy(out.label_it,value.label_it,std::strlen(value.label_it)+1);return out;
}
struct Retain {ModuleLifetime& modules;void* value;~Retain(){if(value)modules.release(value);}};
struct Mutation{bool& busy;explicit Mutation(bool& value):busy(value){busy=true;}~Mutation(){busy=false;}};
}
PauseRegistry::PauseRegistry(ModuleLifetime& modules):modules_(modules),thread_(std::this_thread::get_id()){}
PauseRegistry::~PauseRegistry(){for(auto& entry:entries_)if(entry.module)modules_.release(entry.module);}
bool PauseRegistry::claimed()const noexcept{return std::any_of(owners_.begin(),owners_.end(),[](const auto& item){return item.token!=0;});}
int PauseRegistry::access(bool mutation)const{if(std::this_thread::get_id()!=thread_)return ARSENAL_PAUSE_THREAD;if(busy_||(mutation&&dispatch_depth_))return ARSENAL_PAUSE_BUSY;return 0;}
bool PauseRegistry::owner(std::uint64_t token)const{return token&&std::any_of(owners_.begin(),owners_.end(),[&](const auto& item){return item.token==token;});}
PauseRegistry::Entry* PauseRegistry::find(std::uint64_t token,std::uint64_t owner_token){for(auto& entry:entries_)if(entry.token==token&&token&&entry.owner==owner_token)return &entry;return nullptr;}
const PauseRegistry::Entry* PauseRegistry::find_id(std::uint64_t id)const{for(const auto& entry:entries_)if(entry.token&&entry.descriptor.id==id)return &entry;return nullptr;}
std::uint32_t PauseRegistry::visible_custom()const{return std::uint32_t(std::count_if(entries_.begin(),entries_.end(),[](const auto& e){return e.token&&(e.descriptor.flags&ARSENAL_PAUSE_VISIBLE); }));}
bool PauseRegistry::room(std::uint32_t custom,std::uint32_t stock)const{return !presenter_||(custom<=capacity_.max_custom&&stock<=capacity_.max_visible&&custom<=capacity_.max_visible-stock);}
bool PauseRegistry::descriptor(const arsenal_pause_entry_v1* value){return value&&value->size>=sizeof(*value)&&value->id>=ARSENAL_PAUSE_CUSTOM_ID_MIN&&!(value->flags&~ARSENAL_PAUSE_VISIBLE)&&label(value->label_en)&&label(value->label_it);}
int PauseRegistry::status(arsenal::pause::Status value){using S=arsenal::pause::Status;switch(value){case S::Ok:return 0;case S::Invalid:return ARSENAL_PAUSE_ARGUMENT;case S::Duplicate:return ARSENAL_PAUSE_DUPLICATE;case S::NotFound:return ARSENAL_PAUSE_NOT_FOUND;case S::Exhausted:return ARSENAL_PAUSE_EXHAUSTED;}return ARSENAL_PAUSE_BACKEND;}
int PauseRegistry::acquire(const char* name,std::uint64_t* out){
 if(out)*out=0;if(auto error=access())return error;Mutation mutation{busy_};if(!out||!bounded(name,64))return ARSENAL_PAUSE_ARGUMENT;
 for(const auto& item:owners_)if(item.token&&!std::strcmp(item.name,name))return ARSENAL_PAUSE_DUPLICATE;
 auto slot=std::find_if(owners_.begin(),owners_.end(),[](const auto& item){return !item.token;});
 if(slot==owners_.end()||generation_==std::numeric_limits<std::uint64_t>::max())return ARSENAL_PAUSE_EXHAUSTED;
 *slot={};std::memcpy(slot->name,name,std::strlen(name)+1);slot->token=++generation_;*out=slot->token;return 0;
}
int PauseRegistry::release(std::uint64_t token){
 if(auto error=access())return error;Mutation mutation{busy_};if(!owner(token))return ARSENAL_PAUSE_NOT_FOUND;
 if(revision_==std::numeric_limits<std::uint64_t>::max())return ARSENAL_PAUSE_EXHAUSTED;
 auto staged=model_;for(const auto& entry:entries_)if(entry.token&&entry.owner==token){const auto result=status(staged.remove_entry(entry.descriptor.id));if(result)return result;}
 if(presenter_owner_==token){const auto result=status(staged.compose({}));if(result)return result;}
 model_=std::move(staged);
 for(auto& entry:entries_)if(entry.token&&entry.owner==token){auto* module=entry.module;entry={};if(module)modules_.release(module);}
 for(auto& item:owners_)if(item.token==token)item={};
 if(presenter_owner_==token){presenter_=presenter_owner_=0;capacity_={};stock_visible_=0;}
 ++revision_;return 0;
}
int PauseRegistry::register_entry(std::uint64_t token,const arsenal_pause_entry_v1* value,arsenal_pause_open_v1 open,std::uint64_t context,std::uint64_t* out){
 if(out)*out=0;if(auto error=access())return error;Mutation mutation{busy_};if(!owner(token))return ARSENAL_PAUSE_NOT_FOUND;
 if(!out||!descriptor(value)||!open)return ARSENAL_PAUSE_ARGUMENT;if(find_id(value->id))return ARSENAL_PAUSE_DUPLICATE;
 if(!room(visible_custom()+((value->flags&ARSENAL_PAUSE_VISIBLE)?1u:0u),stock_visible_))return ARSENAL_PAUSE_EXHAUSTED;
 auto slot=std::find_if(entries_.begin(),entries_.end(),[](const auto& entry){return !entry.token;});
 if(slot==entries_.end()||generation_==std::numeric_limits<std::uint64_t>::max()||revision_==std::numeric_limits<std::uint64_t>::max())return ARSENAL_PAUSE_EXHAUSTED;
 Retain retained{modules_,modules_.retain(reinterpret_cast<void*>(open))};if(!retained.value)return ARSENAL_PAUSE_UNSAFE_CALLBACK;
 auto staged=model_;const auto result=status(staged.register_entry({value->id,value->order,bool(value->flags&ARSENAL_PAUSE_VISIBLE)}));if(result)return result;
 const auto entry=copied(*value);model_=std::move(staged);*slot={token,++generation_,context,entry,open,retained.value};retained.value=nullptr;*out=slot->token;++revision_;return 0;
}
int PauseRegistry::unregister_entry(std::uint64_t owner_token,std::uint64_t token){
 if(auto error=access())return error;Mutation mutation{busy_};auto* entry=find(token,owner_token);if(!entry)return ARSENAL_PAUSE_NOT_FOUND;
 if(revision_==std::numeric_limits<std::uint64_t>::max())return ARSENAL_PAUSE_EXHAUSTED;
 auto staged=model_;const auto result=status(staged.remove_entry(entry->descriptor.id));if(result)return result;model_=std::move(staged);
 auto* module=entry->module;*entry={};++revision_;if(module)modules_.release(module);return 0;
}
int PauseRegistry::update_entry(std::uint64_t owner_token,std::uint64_t token,const arsenal_pause_entry_v1* value){
 if(auto error=access())return error;Mutation mutation{busy_};auto* entry=find(token,owner_token);if(!entry)return ARSENAL_PAUSE_NOT_FOUND;
 if(!descriptor(value)||value->id!=entry->descriptor.id)return ARSENAL_PAUSE_ARGUMENT;const auto next=copied(*value);
 if(!std::memcmp(&next,&entry->descriptor,sizeof next))return 0;
 const auto visible=visible_custom()-((entry->descriptor.flags&ARSENAL_PAUSE_VISIBLE)?1u:0u)+((value->flags&ARSENAL_PAUSE_VISIBLE)?1u:0u);
 if(!room(visible,stock_visible_)||revision_==std::numeric_limits<std::uint64_t>::max())return ARSENAL_PAUSE_EXHAUSTED;
 auto staged=model_;if(auto result=status(staged.set_order(value->id,value->order)))return result;
 if(auto result=status(staged.set_visibility(value->id,bool(value->flags&ARSENAL_PAUSE_VISIBLE))))return result;
 model_=std::move(staged);entry->descriptor=next;++revision_;return 0;
}
int PauseRegistry::entry_count(std::uint32_t* count,std::uint64_t* revision)const{
 if(auto error=access(false))return error;if(!count||!revision)return ARSENAL_PAUSE_ARGUMENT;
 *count=std::uint32_t(std::count_if(entries_.begin(),entries_.end(),[](const auto& entry){return entry.token!=0;}));*revision=revision_;return 0;
}
int PauseRegistry::entry_at(std::uint32_t index,arsenal_pause_entry_copy_v1* out)const{
 if(auto error=access(false))return error;if(!out||out->size<sizeof(*out))return ARSENAL_PAUSE_ARGUMENT;
 for(const auto& entry:entries_)if(entry.token){if(index--==0){*out={sizeof(*out),0,entry.token,entry.descriptor};return 0;}}return ARSENAL_PAUSE_NOT_FOUND;
}
int PauseRegistry::claim_presenter(std::uint64_t owner_token,const arsenal_pause_capacity_v1* capacity,std::uint64_t* out){
 if(out)*out=0;if(auto error=access())return error;Mutation mutation{busy_};if(!owner(owner_token))return ARSENAL_PAUSE_NOT_FOUND;
 if(!out||!capacity||capacity->size<sizeof(*capacity)||capacity->reserved||!capacity->max_visible||capacity->max_visible>MaxStock+MaxEntries||!capacity->max_custom||capacity->max_custom>MaxEntries||capacity->max_custom>capacity->max_visible)return ARSENAL_PAUSE_ARGUMENT;
 if(presenter_)return ARSENAL_PAUSE_BUSY;const auto visible=visible_custom();
 if(visible>capacity->max_custom||visible+stock_visible_>capacity->max_visible||generation_==std::numeric_limits<std::uint64_t>::max()||revision_==std::numeric_limits<std::uint64_t>::max())return ARSENAL_PAUSE_EXHAUSTED;
 capacity_=*capacity;capacity_.size=sizeof capacity_;presenter_owner_=owner_token;presenter_=++generation_;*out=presenter_;++revision_;return 0;
}
int PauseRegistry::release_presenter(std::uint64_t owner_token,std::uint64_t token){
 if(auto error=access())return error;Mutation mutation{busy_};if(!token||token!=presenter_||owner_token!=presenter_owner_)return ARSENAL_PAUSE_NOT_FOUND;
 if(revision_==std::numeric_limits<std::uint64_t>::max())return ARSENAL_PAUSE_EXHAUSTED;
 auto staged=model_;const auto result=status(staged.compose({}));if(result)return result;model_=std::move(staged);
 presenter_=presenter_owner_=0;capacity_={};stock_visible_=0;++revision_;return 0;
}
int PauseRegistry::compose(std::uint64_t token,const arsenal_pause_stock_v1* stock,std::uint32_t count){
 if(auto error=access())return error;Mutation mutation{busy_};if(!token||token!=presenter_)return ARSENAL_PAUSE_NOT_FOUND;
 if(count>MaxStock||(count&&!stock))return ARSENAL_PAUSE_ARGUMENT;std::vector<arsenal::pause::Entry> staged;staged.reserve(count);std::uint32_t visible=0;
 for(std::uint32_t i=0;i<count;++i){const auto& entry=stock[i];if(entry.size<sizeof entry||!entry.id||entry.id>=ARSENAL_PAUSE_CUSTOM_ID_MIN||(entry.flags&~ARSENAL_PAUSE_VISIBLE))return ARSENAL_PAUSE_ARGUMENT;visible+=(entry.flags&ARSENAL_PAUSE_VISIBLE)?1u:0u;staged.push_back({entry.id,entry.order,bool(entry.flags&ARSENAL_PAUSE_VISIBLE)});}
 if(!room(visible_custom(),visible)||revision_==std::numeric_limits<std::uint64_t>::max())return ARSENAL_PAUSE_EXHAUSTED;
 const auto before=model_.snapshot().revision;const auto result=status(model_.compose(staged));if(result)return result;stock_visible_=visible;if(model_.snapshot().revision!=before)++revision_;return 0;
}
int PauseRegistry::set_focus(std::uint64_t token,std::uint64_t id){
 if(auto error=access())return error;Mutation mutation{busy_};if(!token||token!=presenter_)return ARSENAL_PAUSE_NOT_FOUND;
 if(revision_==std::numeric_limits<std::uint64_t>::max())return ARSENAL_PAUSE_EXHAUSTED;
 const auto before=model_.snapshot().revision;const auto result=status(model_.set_focus(id));if(result)return result;if(model_.snapshot().revision!=before)++revision_;return 0;
}
int PauseRegistry::snapshot(std::uint64_t token,arsenal_pause_state_v1* out,std::uint64_t* ids,std::uint32_t capacity,std::uint32_t* count)const{
 if(auto error=access(false))return error;if(token&&token!=presenter_)return ARSENAL_PAUSE_NOT_FOUND;
 if(!out||out->size<sizeof(*out)||!count||(!ids&&capacity))return ARSENAL_PAUSE_ARGUMENT;
 const auto snapshot=model_.snapshot();std::uint32_t registered{};std::uint64_t revision{};entry_count(&registered,&revision);
 *out={sizeof(*out),registered,std::uint32_t(snapshot.visible.size()),presenter_?1u:0u,revision,snapshot.focus,capacity_.max_visible,capacity_.max_custom};*count=std::uint32_t(snapshot.visible.size());
 if(!capacity)return 0;if(capacity<snapshot.visible.size())return ARSENAL_PAUSE_EXHAUSTED;std::copy(snapshot.visible.begin(),snapshot.visible.end(),ids);return 0;
}
int PauseRegistry::activate(std::uint64_t token,std::uint64_t id){
 if(auto error=access())return error;if(!token||token!=presenter_)return ARSENAL_PAUSE_NOT_FOUND;
 const auto* entry=find_id(id);if(!entry||!(entry->descriptor.flags&ARSENAL_PAUSE_VISIBLE)||!model_.snapshot().contains(id))return ARSENAL_PAUSE_NOT_FOUND;
 struct Dispatch {unsigned& depth;explicit Dispatch(unsigned& value):depth(value){++depth;}~Dispatch(){--depth;}} dispatch{dispatch_depth_};
 try{return entry->open(entry->context,id);}catch(...){return ARSENAL_PAUSE_BACKEND;}
}
}
