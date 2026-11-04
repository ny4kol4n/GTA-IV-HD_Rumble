#include "../source/trigger_packet.hpp"
#include <cassert>
int main(){
    rn::TriggerPacket p{},out{};p.right=1000;p.flags=rn::drivingTriggers;
    assert(rn::decodeTriggers(&p,sizeof p,out));
    assert(rn::muteDrivingMain(out,0,0,1000));
    assert(rn::muteDrivingMain(out,250,1000,0));
    assert(!rn::muteDrivingMain(out,251,0,1000));
    assert(!rn::muteDrivingMain(out,0,0,0));
    p.flags=0;assert(!rn::muteDrivingMain(p,0,0,1000)); // on-foot firing
    p.magic=rn::legacyTriggerMagic;p.flags=99;
    assert(rn::decodeTriggers(&p,8,out)&&out.flags==0);
    assert(!rn::muteDrivingMain(out,0,0,1000));
    assert(!rn::decodeTriggers(&p,12,out));
    p.magic=rn::triggerMagic;assert(!rn::decodeTriggers(&p,12,out));
    assert(!rn::decodeTriggers(&p,9,out));assert(!rn::decodeTriggers(&p,13,out));
}
