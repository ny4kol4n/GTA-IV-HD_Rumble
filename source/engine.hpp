#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstddef>
#include "groups.hpp"
#include "config.hpp"
namespace rn {
struct Sample { uint32_t vehicle=0,model=0; float speed=0,z=0,heading=0; int health=0; bool bike=false,boat=false; bool tyres[6]{}; };
struct Output { bool acceleration=false,braking=false; uint16_t left=0,right=0; int damage=0; const char* event="None"; const char* group="Missing!"; };
inline const char* profileName(int p){return p==0?"Strong":p==1?"Medium":"Light";}
class Engine {
    Sample prev{}; bool valid=false; uint64_t motion=0,slow=0,last=0;
    uint64_t gear=0,bump=0,turn=0;
    bool slowActive=false;
public:
    int profile=1;
    Settings settings{};
    void reset(){ valid=false; slowActive=false; }
    Output step(const Sample& s,uint64_t now){
        Output o; o.group=group(s.model);
        if(!valid || prev.vehicle!=s.vehicle || prev.model!=s.model || now-last>500){
            prev=s;valid=true;motion=now;slowActive=false;last=now;
            gear=bump=turn=0;return o;
        }
        const float sampleScale=70.f/float(std::max(uint64_t(1),now-last));
        last=now;
        const float ds=s.speed-prev.speed, dz=s.z-prev.z;
        const float angle=std::abs(std::remainder(s.heading-prev.heading,360.f));
        o.damage=std::max(0,prev.health-s.health); prev=s;
        if(std::abs(ds)>=.06f) motion=now;
        if(s.speed<1.f){if(!slowActive){slow=now;slowActive=true;}}
        else slowActive=false;
        if(slowActive && now-slow>500) return o;
        constexpr float factors[]={.35f,.35f,.35f},gt[]={-.25f,-.40f,-.50f},zt[]={.4f,.6f,.8f},tt[]={10,15,20};
        constexpr int gc[]={300,400,600},zc[]={1000,1500,2000},tc[]={300,500,800};
        int p=std::clamp(profile,0,2), turnCool=tc[p]; float f=factors[p];
        float a=.4f,b=.6f,g=.3f,t=.4f,z=.3f;
        auto is=[&](const char* x){return std::strcmp(o.group,x)==0;};
        if(is("MotoRacer")){a=.3f;b=.6f;g=.3f;t=2.2f;z=1;turnCool=150;}
        else if(is("MotoChopper")){a=.8f;b=1;g=.7f;t=.4f;z=1.2f;turnCool=550;}
        else if(is("Sports")){a=.8f;b=.5f;g=.8f;t=.4f;z=.4f;}
        else if(is("Muscle")){a=.9f;b=.8f;g=.8f;t=.5f;z=.5f;}
        else if(is("Truck")){a=.3f;b=.6f;g=.3f;t=.5f;z=.2f;}
        else if(is("Hatchback")){a=.3f;b=1.1f;g=.3f;t=1.1f;z=.2f;}
        else if(is("Sedan")){a=.7f;b=.85f;g=.7f;t=.35f;z=.3f;}
        else if(is("Boat")){a=0;b=.6f;g=0;t=0;z=0;}
        int l=0,r=0;
        auto add=[&](float x,float y){l=std::min(65535*8,l+int(x));r=std::min(65535*8,r+int(y));};
        // Continuous weak tyre rumble while moving; no stacking per wheel.
        if(settings.tyres>0 && !s.boat && s.speed>settings.tyreSpeed[p]){
            bool left=false,right=false;
            if(s.bike){left=right=s.tyres[0]||s.tyres[4];}
            else {left=s.tyres[0]||s.tyres[2]||s.tyres[4];right=s.tyres[1]||s.tyres[3]||s.tyres[5];}
            // Convert configured final tyre strength to the internal /8 scale.
            add(left?float(settings.tyreStrength*8)*settings.tyres:0.f,right?float(settings.tyreStrength*8)*settings.tyres:0.f);
            if(left||right)o.event="Burst tyre";
        }
        if(settings.damage>0&&o.damage>0){int n=int(std::clamp(int64_t(o.damage)*1000,int64_t(10000),int64_t(65535)));add(float(n)*settings.damage,float(n)*settings.damage);o.event="Damage";}
        if(settings.acceleration>0 && ds*sampleScale>settings.accelThreshold[p]){add(12000*a*f*settings.acceleration,22000*a*f*settings.acceleration);o.event="Accel";o.acceleration=true;}
        if(settings.gear>0 && ds<gt[p] && now-gear>uint64_t(gc[p])){add(20000*g*f*settings.gear,24000*g*f*settings.gear);gear=now;o.event="Gear";}
        if(settings.braking>0 && ds*sampleScale<-settings.brakeThreshold[p]){add(18000*b*f*settings.braking,15000*b*f*settings.braking);o.event="Brake";o.braking=true;}
        if(settings.bump>0 && dz>zt[p] && now-bump>uint64_t(zc[p])){add(14000*z*f*settings.bump,14000*z*f*settings.bump);bump=now;o.event="Curb/Bump";}
        if(settings.turn>0 && angle>tt[p] && now-turn>uint64_t(turnCool)){add(18000*t*f*settings.turn,25000*t*f*settings.turn);turn=now;o.event="Turn";}
        // Percent gains apply before the configurable per-motor ceiling.
        l=int((l/8)*settings.mainGain*settings.profileGain[p]);
        r=int((r/8)*settings.mainGain*settings.profileGain[p]);
        l=std::clamp(l,0,settings.motorLimit);r=std::clamp(r,0,settings.motorLimit);
        o.left=uint16_t(l);o.right=uint16_t(r);return o;
    }
};
}
