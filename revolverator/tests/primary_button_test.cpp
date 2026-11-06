// SPDX-License-Identifier: GPL-3.0-or-later
#include "primary_button.hpp"
#include <cstdio>
#include <cstdlib>
using namespace revolverator;
void require(bool value,const char* description){if(!value){std::fprintf(stderr,"primary button: %s\n",description);std::exit(1);}}
int main(){
 PrimaryButton input;
 require(input.update(false,true,true,true),"ordinary gameplay uses the native processed fire button");
 require(input.update(true,true,true,false),"pause filtering cannot turn a physical hold into release");
 require(input.update(false,true,true,false),"the first resumed report preserves the physical hold despite a consumed processed bit");
 require(!input.update(false,true,false,false),"a genuine live release immediately reaches the controller");
 input.update(false,true,true,true);input.update(true,true,true,false);
 require(!input.update(true,true,false,false),"a release while paused is remembered");
 require(!input.update(false,true,false,false),"releasing in pause drops the captive on resume");
 input.update(false,true,true,true);input.update(true,true,std::nullopt,false);
 require(input.update(false,true,std::nullopt,false),"an unreadable resume report does not fabricate a release");
 require(!input.update(false,true,false,false),"the next live report remains authoritative");
 input.update(false,true,true,true);input.update(true,true,true,false);
 require(!input.update(false,false,true,false),"unequip or acquisition cancels a pending resume hold");
 require(!input.update(false,true,true,false),"a later equip does not invent a processed press");
 input.update(false,true,true,true);input.reset();
 require(!input.update(true,true,std::nullopt,false),"level reset clears the cached physical input");
 std::puts("primary button: pause/resume and genuine release checks passed");
}
