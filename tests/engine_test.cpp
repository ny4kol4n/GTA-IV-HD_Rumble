#include "../source/engine.hpp"
#include <cassert>
#include "../source/shooting.hpp"
#include <iostream>
int main(){
    using namespace rn;
    assert(boostShot(0)==0&&boostShot(10000)==15000);
    assert(boostShot(40000)==60000&&boostShot(50000)==65535&&boostShot(65535)==65535);
    Sample s;s.vehicle=1;s.model=0xC1E908D2;s.health=1000;s.speed=20;
    Engine e;auto o=e.step(s,10000);assert(o.left==0&&o.right==0); // baseline, no spawn impact
    s.speed=21;o=e.step(s,10070);assert(o.left==420&&o.right==770&&o.acceleration&&!o.braking); // Medium sports acceleration
    s.speed=20;o=e.step(s,10140);assert(o.left==1093&&o.right==1168&&o.braking&&!o.acceleration); // gear + braking
    s.health=900;o=e.step(s,10210);assert(o.left==1500&&o.right==1500); // saturation
    s.vehicle=2;s.z=100;s.heading=180;o=e.step(s,10280);assert(o.left==0&&o.right==0); // vehicle switch
    s.health=1000;o=e.step(s,10350);assert(o.damage==0); // repair is not damage
    e.reset();s.speed=20;s.tyres[4]=true;e.step(s,11000);o=e.step(s,11070);assert(o.left==300&&o.right==0);
    o=e.step(s,11140);assert(o.left==300&&o.right==0); // tyre rumble stays continuous
    s.tyres[5]=true;e.reset();e.step(s,11140);o=e.step(s,11210);assert(o.left==300&&o.right==300);
    s.bike=true;s.tyres[5]=false;e.reset();e.step(s,11210);o=e.step(s,11280);assert(o.left==300&&o.right==300);
    s.boat=true;o=e.step(s,11350);assert(o.left==0&&o.right==0);
    s.boat=false;s.bike=false;s.tyres[4]=false;s.speed=0;e.reset();e.step(s,12000);
    for(uint64_t n=12070;n<=12630;n+=70)o=e.step(s,n);
    assert(o.left==0&&o.right==0);
    s.health=500;o=e.step(s,12700);assert(o.left==0&&o.right==0); // original slow cutoff
    e.reset();s.speed=20;s.heading=359;e.step(s,13000);s.heading=1;o=e.step(s,13070);assert(o.left==0&&o.right==0); // wrap
    s.heading=30;o=e.step(s,13140);assert(o.left==315&&o.right==437);
    s.z+=1;o=e.step(s,13210);assert(o.left==245&&o.right==245);
    s.z+=1;o=e.step(s,13280);assert(o.left==0&&o.right==0); // cooldown
    s.speed=100;s.z=10000;o=e.step(s,15000);assert(o.left==0&&o.right==0); // stale samples reset
    e.reset();e.profile=2;s.speed=20;e.step(s,16000);s.speed+=.05f;o=e.step(s,16070);assert(o.left==0); // Light threshold
    s.speed+=1;o=e.step(s,16140);assert(o.left==420&&o.right==770);
    for(int profile=0;profile<3;++profile){
        Engine tyres;tyres.profile=profile;
        Sample car;car.vehicle=1;car.speed=20;car.health=1000;
        for(bool& tyre:car.tyres)tyre=true;
        tyres.step(car,20000);int pulses=0;
        const int strength[]={300,300,300},count[]={20,20,20},caps[]={1500,1500,1500};
        for(int i=1;i<=20;++i){auto out=tyres.step(car,20000+i*70);
            if(out.left||out.right){assert(out.left==strength[profile]&&out.right==strength[profile]);++pulses;}}
        assert(pulses==count[profile]); // continuous output, no stacking even with all six tyres burst
        for(bool& tyre:car.tyres)tyre=false;
        auto repaired=tyres.step(car,21435);assert(repaired.left==0&&repaired.right==0);
        for(bool& tyre:car.tyres)tyre=true;
        car.health=500;car.speed=30;car.z=2;car.heading=90;
        auto combined=tyres.step(car,21470);assert(combined.left==caps[profile]&&combined.right==caps[profile]);
    }
    // Gentle acceleration must trigger at different sampling intervals;
    // steady speed and tiny drift must not produce engine rumble.
    for(int profile=0;profile<3;++profile){
        for(uint64_t interval:{70u,140u}){
            Engine gentle;gentle.profile=profile;
            Sample car;car.vehicle=1;car.model=0xC1E908D2;car.health=1000;car.speed=10;
            gentle.step(car,30000);
            car.speed+=.12f*float(interval)/70.f;
            auto out=gentle.step(car,30000+interval);
            assert(out.left==420&&out.right==770);
            out=gentle.step(car,30000+interval*2);assert(out.left==0&&out.right==0);
            car.speed+=.02f;
            out=gentle.step(car,30000+interval*3);assert(out.left==0&&out.right==0);
        }
    }
    // Sustained 90 km/h -> 0 braking: no gaps, all profiles and frame rates.
    for(int profile=0;profile<3;++profile){
        for(uint64_t interval:{70u,140u}){
            Engine braking;braking.profile=profile;
            Sample car;car.vehicle=1;car.model=0xC1E908D2;car.health=1000;car.speed=25;
            uint64_t time=40000;braking.step(car,time);
            while(car.speed>0){
                car.speed=std::max(0.f,car.speed-.25f*float(interval)/70.f);time+=interval;
                auto out=braking.step(car,time);
                assert(out.braking&&!out.acceleration&&out.left>0&&out.right>0);
            }
            auto stopped=braking.step(car,time+interval);
            assert(!stopped.braking&&stopped.left==0&&stopped.right==0);
        }
        Engine drift;drift.profile=profile;
        Sample car;car.vehicle=1;car.health=1000;car.speed=25;drift.step(car,50000);
        car.speed-=.02f;auto out=drift.step(car,50070);assert(!out.braking);
    }
    assert(parseSetting("nan",100,0,400)==100);
    assert(parseSetting("oops",100,0,400)==100);
    assert(parseSetting("50abc",100,0,400)==100);
    assert(parseSetting("500",100,0,400)==400);
    assert(parseSetting("-1",100,0,400)==0);
    assert(parseSetting("0.08",1,0,10)==.08f);
    {
        Sample car;car.vehicle=1;car.model=0xC1E908D2;car.health=1000;car.speed=20;
        Engine muted;muted.settings.acceleration=0;muted.step(car,60000);
        car.speed+=1;auto out=muted.step(car,60070);
        assert(!out.acceleration&&out.left==0&&out.right==0);
        Engine half;half.settings.mainGain=.5f;half.step(car,60000);car.speed+=1;
        out=half.step(car,60070);assert(out.left==210&&out.right==385);
        Engine cap;cap.settings.motorLimit=100;cap.step(car,60000);car.speed+=1;
        out=cap.step(car,60070);assert(out.left==100&&out.right==100);
        Engine tyres;tyres.settings.tyreStrength=150;car.tyres[4]=true;tyres.step(car,60000);
        out=tyres.step(car,60070);assert(out.left==150&&out.right==0);
        tyres.settings.tyres=0;out=tyres.step(car,60140);assert(out.left==0&&out.right==0);
        Engine allOff;auto& c=allOff.settings;
        c.acceleration=c.braking=c.gear=c.bump=c.turn=c.damage=c.tyres=0;
        allOff.step(car,60000);car.speed-=2;car.z+=2;car.heading+=40;car.health-=100;
        out=allOff.step(car,60070);assert(out.left==0&&out.right==0&&!out.braking);
    }
    assert(boostShot(10000,1)==10000&&boostShot(10000,0)==0&&boostShot(40000,2)==65535);
    std::cout<<"PASS: baseline, acceleration, gear+brake, saturation, vehicle switch, repair, rear tyres, motorcycle, boat, slow cutoff, angle wrap, turn, bump, cooldown, stale sample, profiles\n";
}
