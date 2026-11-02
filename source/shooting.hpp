#pragma once
#include <algorithm>
#include <cstdint>
namespace rn {
inline uint16_t boostShot(uint16_t value,float gain=1.5f){return uint16_t(std::clamp(float(value)*gain,0.f,65535.f));}
}
