#include "../source/rumble_levels.hpp"
#include <cassert>
int main() {
    assert(gipMainLevel(0)==0);
    for(int v=1;v<=65535;++v){
        auto out=gipMainLevel(v);
        assert(out/655>0);
        assert(out==(v<655?655:v));
    }
    // Observed simultaneous tyre/acceleration and trigger output.
    assert(gipMainLevel(510)/655==1);
    assert(gipMainLevel(385)/655==1);
    assert(gipMainLevel(65535)==65535);
    assert(gipMainLevel(70000)==65535);
}
