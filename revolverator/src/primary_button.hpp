// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <optional>
namespace revolverator {
// Native pause menus consume processed buttons. Keep the physical hold while
// paused, including genuine releases, and synchronize once on the way back.
class PrimaryButton final {
public:
 bool update(bool paused,bool enabled,std::optional<bool> physical,bool processed){
  if(paused){resuming_=true;if(physical)down_=*physical;return down_;}
  if(!enabled){reset();return false;}
  down_=resuming_?physical.value_or(down_):processed;
  resuming_=false;return down_;
 }
 void reset(){down_=resuming_=false;}
private:
 bool down_{},resuming_{};
};
}
