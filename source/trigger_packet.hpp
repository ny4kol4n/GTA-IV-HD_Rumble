#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
namespace rn {
constexpr uint32_t triggerMagic=0x364E4252,legacyTriggerMagic=0x354E4252;
constexpr uint32_t drivingTriggers=1;
struct TriggerPacket {uint32_t magic=triggerMagic;uint16_t left=0,right=0;uint32_t flags=0;};
static_assert(sizeof(TriggerPacket)==12);
inline bool decodeTriggers(const void* data,size_t size,TriggerPacket& out){
    if(size!=8&&size!=sizeof(TriggerPacket))return false;
    TriggerPacket p{};std::memcpy(&p,data,size);
    if(size==8){if(p.magic!=legacyTriggerMagic)return false;p.flags=0;}
    else if(p.magic!=triggerMagic||(p.flags&~drivingTriggers))return false;
    out=p;return true;
}
inline bool muteDrivingMain(const TriggerPacket& p,uint64_t ageMs,int left,int right){
    return ageMs<=250&&(p.flags&drivingTriggers)&&(left>0||right>0);
}
}
