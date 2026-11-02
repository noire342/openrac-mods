#include "robot_salvage_api.h"
#include <stddef.h>
int robot_salvage_c_header_test(void){
 robot_salvage_inventory_entry_v1 entry={0};
 entry.size=(uint32_t)sizeof(entry);entry.part=UINT64_C(0xffffffffffffffff);
 return ROBOT_SALVAGE_API_VERSION==1u&&entry.part==UINT64_MAX&&
  offsetof(robot_salvage_api_v1,read_events)>offsetof(robot_salvage_api_v1,apply_changes);
}
