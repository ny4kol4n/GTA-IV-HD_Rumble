#pragma once
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cerrno>
namespace rn {
inline float parseSetting(const char* text,float fallback,float low,float high){
    char* end=nullptr;errno=0;float value=std::strtof(text,&end);
    if(end==text||errno||!std::isfinite(value))return fallback;
    while(*end==' '||*end=='\t')++end;
    if(*end)return fallback;
    return std::clamp(value,low,high);
}
struct Settings {
    float mainGain=1,profileGain[3]={1,1,1};
    int motorLimit=1500,tyreStrength=300;
    float acceleration=1,braking=1,gear=1,bump=1,turn=1,damage=1,tyres=1;
    float accelThreshold[3]={.06f,.08f,.10f},brakeThreshold[3]={.06f,.08f,.10f};
    float tyreSpeed[3]={2,5,10};
};
}
