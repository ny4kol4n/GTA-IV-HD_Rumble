#pragma once
#include <cstdint>
namespace rn {
inline constexpr uint32_t SportsHashes[] = {0xC1E908D2,0x3F637729,0x067BC037,0xBE9075F1,0x18F25AC7,0xEE6024BC,0x6C9962A9,0x8EF34547};
inline constexpr uint32_t MuscleHashes[] = {0xD756460C,0x2B26F456,0xF26CEFF9,0xE53C7459,0x4B5D021E,0x9B909C94,0x72A4C31E,0xCEC6B9B7,0x973141FC,0x85D07492,0x779B4F2D,0x81A9CDDF,0x81634188,0x6D19CCBC,0xE2504942};
inline constexpr uint32_t TruckHashes[] = {0x4020325C,0x779F23AA,0xD0EB2BE5,0x1D06D681,0x4BA4E8DC,0xCFCFEB3B,0x04F48FC4,0x52DB01E0,0x04F48FC4};
inline constexpr uint32_t HatchbackHashes[] = {0xEB70965F,0x255FC509,0x7836CE2F,0x50732C82,0x5B73F5B7};
inline constexpr uint32_t SedanHashes[] = {0x4B5C5320,0xFBFD5B62,0x86FE0B60,0xBC993509,0xD7278283,0x8FC3AADC,0xEF7ED55D,0x3A196CEA,0xEB9F21D3,0xB3206692,0x34DD8AA1,0xFDCAF758,0x4DC293EA,0xB4D8797E,0x07D10BDC,0x5208A519,0x8FB66F9B,0x8B0D2BA6,0x2560B2FC,0xECC96C3F,0x50249008,0x66B4FC45,0x8B13F083,0xDD3BD501,0x69F06B57,0x737DAEC2};
inline constexpr uint32_t MotoRacerHashes[] = {0x737DAEC2,0xCADD5D2D,0x9C669788,0x9229E4EB,0x0350D1AB,0x4B6C568A,0x47B9138A,0xC9CEAF06,0xF79A00F7};
inline constexpr uint32_t MotoChopperHashes[] = {0x92E56A2C,0x22DC8E7F,0x11F76C14,0xFDEFAEC3,0x2EF89E46,0xDE05FB87};
inline constexpr uint32_t BoatHashes[] = {0x3D8FA25C,0xAF922754,0xC1CE1183,0x664C0A2C,0xAB52CEC7,0x3978C13B,0xC8D9F8D2,0x52DC4088,0x78F4E882,0xE84E3695};
template<size_t N> bool contains(const uint32_t (&a)[N],uint32_t h){for(auto x:a) if(x==h)return true;return false;}
inline const char* group(uint32_t h){
if(contains(SportsHashes,h)) return "Sports";
if(contains(MuscleHashes,h)) return "Muscle";
if(contains(TruckHashes,h)) return "Truck";
if(contains(HatchbackHashes,h)) return "Hatchback";
if(contains(SedanHashes,h)) return "Sedan";
if(contains(MotoRacerHashes,h)) return "MotoRacer";
if(contains(MotoChopperHashes,h)) return "MotoChopper";
if(contains(BoatHashes,h)) return "Boat";
return "Missing!";
}
}
