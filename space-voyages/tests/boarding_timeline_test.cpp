// SPDX-License-Identifier: GPL-3.0-or-later
#include "boarding_timeline.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>
using namespace spacevoyages;
namespace {
unsigned checks{};
void require(bool pass,const char* message){++checks;if(!pass){std::fprintf(stderr,"boarding_timeline: %s\n",message);std::exit(1);}}

// Matches the source's ordering: increment cursors, check end/cleanup BEFORE
// camera and actor evaluation, otherwise parse a new bank if necessary.
bool native_tick(boarding::Playback& state){
    ++state.time;++state.frame;
    if(state.time>=state.end)return false;
    if(state.frame>=boarding::bank_ticks){++state.bank;state.frame=0;}
    return true;
}
}
int main(){
    const boarding::Playback first{102,6,1,598,0},last{464,80,4,598,0};
    auto seek=boarding::plan(first,true,false,false);
    require(seek.action==boarding::Action::SeekExterior&&seek.parse_required(),"first console is bypassed by a native bank parse");
    require(seek.time_before_next_tick==242&&seek.bank==2&&seek.frame_before_next_tick==50,"seek cursors anticipate native increment and reach authored tick243");
    auto finish=boarding::plan(last,true,true,false);
    require(finish.action==boarding::Action::FinishBoarding&&!finish.parse_required()&&finish.time_before_next_tick==597,"final dashboard ends through native cleanup without an actor/camera parse");
    require(boarding::plan(first,false,false,false).action==boarding::Action::None,"ineligible departure scope leaves original playback intact");
    require(boarding::plan(first,true,true,false).action==boarding::Action::None&&boarding::plan(last,true,true,true).action==boarding::Action::None,"caller once flags prevent both actions from repeating");

    // Three standard ships and the two special Orxon/Oltanis boarding clips
    // have independently verified identical shot markers, despite different
    // actor counts and poses. No assumption about their meshes is needed.
    for(int clip:{0,1,2,3,8}){
        int variant=clip==0?1:clip==8?2:clip-1;
        boarding::Playback state{0,0,0,598,variant};
        bool exterior_done=false,finish_done=false;
        unsigned parses=0,finishes=0;std::vector<int> drawn;
        for(unsigned guard=0;guard<1000;++guard){
            require(boarding::coherent(state),"every rendered picture retains coherent native time/bank/pose cursors");
            drawn.push_back(state.time);
            require(!(state.time>=103&&state.time<243)&&!(state.time>=465&&state.time<598),"neither console nor its scripted white flash can be presented");
            const auto action=boarding::plan(state,true,exterior_done,finish_done);
            if(action.action==boarding::Action::SeekExterior){
                ++parses;exterior_done=true;
                // Native parse resets relative frame to0 and loads all actor
                // and camera records; the caller then installs the cursors.
                state.frame=0;state.bank=action.bank;
                state.time=action.time_before_next_tick;state.frame=action.frame_before_next_tick;
            }else if(action.action==boarding::Action::FinishBoarding){
                ++finishes;finish_done=true;state.time=action.time_before_next_tick;
            }
            if(!native_tick(state))break;
        }
        require(parses==1&&finishes==1&&state.time==598,"all five archives seek once and execute native end exactly once");
        require(drawn.size()==325&&drawn.front()==0&&drawn.back()==464,"entry, hull exterior and ignition remain intact without the cockpit's white flash");
        for(int tick:{0,60,102,243,300,362,363,420,464}){
            bool present=false;for(int actual:drawn)if(actual==tick)present=true;
            require(present,"every retained authored shot and its boundary remains visible");
        }
        require(drawn[102]==102&&drawn[103]==243,"the console is removed at authored camera cuts rather than by an arbitrary actor pose");
    }
    // Original's transparent cockpit reveals the climb inside this camera
    // shot. Preserve every entry pose while retaining the same native cleanup.
    {
        boarding::Playback state{0,0,0,598,0};
        unsigned finishes=0;
        std::vector<int> drawn;
        for(unsigned guard=0;guard<1000;++guard){
            require(boarding::coherent(state),"complete Original entry keeps native actor and camera cursors coherent");
            drawn.push_back(state.time);
            const auto action=boarding::plan(state,true,false,finishes!=0,boarding::EntryPresentation::PreserveEntry);
            require(action.action!=boarding::Action::SeekExterior,"Original entry arc never seeks over climbing poses");
            if(action.action==boarding::Action::FinishBoarding){++finishes;state.time=action.time_before_next_tick;}
            if(!native_tick(state))break;
        }
        require(finishes==1&&state.time==598&&drawn.size()==465,"Original preserves the full entry and executes native cleanup once before the white flash");
        for(unsigned tick=0;tick<drawn.size();++tick)
            require(drawn[tick]==int(tick),"no authored Original climbing pose is cut or repeated");
    }
    for(int end:{0,498,597,599,600,std::numeric_limits<int>::max()}){
        auto bad=first;bad.end=end;
        require(!boarding::coherent(bad)&&boarding::plan(bad,true,false,false).action==boarding::Action::None,"unknown archives and PAL50 timing retain native playback");
    }
    for(int variant:{-1,3,531,std::numeric_limits<int>::max()}){
        auto bad=first;bad.variant=variant;
        require(!boarding::coherent(bad)&&boarding::plan(bad,true,false,false).action==boarding::Action::None,"unknown ship variants fail closed");
    }
    for(auto bad:std::array<boarding::Playback,7>{{{102,7,1,598,0},{102,6,2,598,0},{-1,0,0,598,0},{598,22,6,598,0},{102,96,1,598,0},{102,6,-1,598,0},{102,6,std::numeric_limits<int>::max(),598,0}}})
        require(!boarding::coherent(bad)&&boarding::plan(bad,true,false,false).action==boarding::Action::None,"corrupt cursors cannot trigger a seek or integer overflow");
    for(int tick:{101,103,242,243,463,465,502,503,557,597}){
        boarding::Playback state{tick,tick%96,tick/96,598,0};
        require(boarding::plan(state,true,false,false).action==boarding::Action::None,"a missed cue never causes a late pose jump after presentation");
    }
    std::printf("Boarding timeline: %u scope, authored-shot, pose-cursor and cleanup checks passed\n",checks);
}
