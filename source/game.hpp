#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>
#include <type_traits>
namespace game {
// GTA IV x86 ABI. Layout cross-checked against Aru's NativeContext/GameTypes.
struct Context {
    void* result; uint32_t count; void* args; uint32_t vectors;
    float* originals[4]; float temporary[4][4];
    uint32_t slots[16]{};
    Context():result(slots),count(0),args(slots),vectors(0),originals{},temporary{}{}
    template<class T> void push(T v){static_assert(sizeof(T)<=4);std::memcpy(&slots[count++],&v,sizeof v);}
};
static_assert(sizeof(void*)==4,"GTA IV requires x86");
static_assert(offsetof(Context,slots)==96);
using Handler=void(__cdecl*)(Context*);
struct Native {uint32_t hash;Handler fn=nullptr;};
inline Native playerId{0x62E319C6},playerIndex{0x5996315E},playerChar{0x511454A9},charExists{0x46531797},charDead{0x6A6B4F18},inCar{0x71184DA3},carUsing{0x1B067237},carExists{0x67A42263},speed{0x16DD2D00},coords{0x2D432EAB},heading{0x46803CFA},health{0x4D417CD3},model{0x5FF84497},tyre{0x1DF623F9},bike{0x57F46B33},boat{0x43CC0913},paused{0x6C4568A7},playing{0x08274BA4},print{0x0CA539D6};
inline Native shooting{0x324D1594}; // IS_CHAR_SHOOTING(Ped) -> boolean
inline Native* all[]={&playerId,&playerIndex,&playerChar,&charExists,&charDead,&inCar,&carUsing,&carExists,&speed,&coords,&heading,&health,&model,&tyre,&bike,&boat,&paused,&playing,&print,&shooting};
template<class R=uint32_t,class... A> R call(Native& n,A... a){
    static_assert(sizeof...(A)<=16);Context c;(c.push(a),...);n.fn(&c);
    for(uint32_t i=0;i<c.vectors && i<4;++i)if(c.originals[i])std::memcpy(c.originals[i],c.temporary[i],12);
    R r{};static_assert(sizeof(R)<=4);std::memcpy(&r,c.slots,sizeof r);return r;
}
inline bool readable(const void* p,size_t bytes){
    if(!p || !bytes)return false;
    auto cur=reinterpret_cast<uintptr_t>(p),end=cur+bytes;if(end<cur)return false;
    while(cur<end){MEMORY_BASIC_INFORMATION m{};if(!VirtualQuery((void*)cur,&m,sizeof m)||m.State!=MEM_COMMIT||(m.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;auto next=reinterpret_cast<uintptr_t>(m.BaseAddress)+m.RegionSize;if(next<=cur)return false;cur=next;}return true;
}
inline bool executable(const void* p){MEMORY_BASIC_INFORMATION m{};return VirtualQuery(p,&m,sizeof m)&&m.State==MEM_COMMIT&&!(m.Protect&PAGE_GUARD)&&(m.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY));}
inline std::vector<uint8_t*> scan(const char* spec){
    std::vector<int> pattern;
    while(*spec){if(*spec==' '){++spec;continue;}if(*spec=='?'){pattern.push_back(-1);while(*spec=='?')++spec;}else{char* e=nullptr;pattern.push_back(int(strtoul(spec,&e,16)));if(e==spec)return {};spec=e;}}
    std::vector<uint8_t*> found;auto base=(uint8_t*)GetModuleHandleW(nullptr);
    auto dos=(IMAGE_DOS_HEADER*)base;auto nt=(IMAGE_NT_HEADERS*)(base+dos->e_lfanew);auto sec=IMAGE_FIRST_SECTION(nt);
    for(unsigned s=0;s<nt->FileHeader.NumberOfSections;++s){if(!(sec[s].Characteristics&IMAGE_SCN_MEM_EXECUTE))continue;
        size_t size=sec[s].Misc.VirtualSize;auto start=base+sec[s].VirtualAddress;if(size<pattern.size()||!readable(start,size))continue;
        for(size_t i=0;i<=size-pattern.size();++i){bool match=true;for(size_t j=0;j<pattern.size();++j)if(pattern[j]>=0&&start[i+j]!=pattern[j]){match=false;break;}if(match)found.push_back(start+i);}}
    return found;
}
template<class T> T operand(uint8_t* p,size_t offset){T value{};std::memcpy(&value,p+offset,sizeof value);return value;}
// Each lookup function has its own table. Select by required native contents,
// not by equality of globals belonging to two different lookup functions.
inline uint32_t *tableSize=nullptr,**table=nullptr;
inline bool *userPause=nullptr,*scriptPause=nullptr;
inline const char* discoveryStatus="not started";
inline size_t sizeMatches=0,tableMatches=0,pauseMatches=0;
inline bool resolve(){
    if(!readable(tableSize,4)||!readable(table,4))return false;
    uint32_t size=*tableSize;if(size<100||size>100000||!readable(*table,size*8))return false;
    Handler resolved[sizeof(all)/sizeof(all[0])]{};size_t k=0;
    for(auto n:all){
        for(uint32_t i=0;i<size;++i)if((*table)[2*i]==n->hash){auto fn=(Handler)(*table)[2*i+1];if(executable((void*)fn))resolved[k]=fn;break;}
        if(!resolved[k])return false;
        ++k;
    }
    k=0;for(auto n:all)n->fn=resolved[k++];return true;
}
inline bool discover(){
    auto sizes=scan("8B 35 ? ? ? ? 85 F6 75 06 33 C0 5E C2 04 00 53 57 8B 7C 24 10");
    if(sizes.empty())sizes=scan("8B 3D ? ? ? ? 85 FF 75 04 33 C0 5F C3");
    auto tables=scan("8B 1D ? ? ? ? 8B CF 8B 04 D3 3B C7 74 19 8D 64 24 00 85 C0");
    if(tables.empty())tables=scan("8B 1D ? ? ? ? 8B CE 8B 04 D3 3B C6 74 17 85 C0");
    sizeMatches=sizes.size();tableMatches=tables.size();
    auto p=scan("0F B6 0D ? ? ? ? 0F B6 C0 0B C1");
    if(p.size()==1){userPause=operand<bool*>(p[0],3);scriptPause=operand<bool*>(p[0],15);}
    else {p=scan("0F B6 0D ? ? ? ? 0F B6 15 ? ? ? ? 33 C0 0B C1");if(p.size()==1){userPause=operand<bool*>(p[0],3);scriptPause=operand<bool*>(p[0],10);}}
    pauseMatches=p.size();
    if(p.size()!=1||!readable(userPause,1)||!readable(scriptPause,1)){discoveryStatus="pause signature not ready";return false;}
    if(sizes.empty()||tables.empty()||sizes.size()>4||tables.size()>4){discoveryStatus="lookup signatures missing/ambiguous";return false;}
    uint32_t *selectedSize=nullptr,**selectedTable=nullptr;
    for(auto size:sizes)for(auto candidate:tables){
        // Size load and table load must be in the same short lookup routine.
        auto distance=intptr_t(candidate)-intptr_t(size);if(distance<=0||distance>96)continue;
        tableSize=operand<uint32_t*>(size,2);table=operand<uint32_t**>(candidate,2);
        if(resolve()){
            if(selectedTable && selectedTable!=table){discoveryStatus="multiple tables contain all required natives";table=nullptr;return false;}
            selectedSize=tableSize;selectedTable=table;
        }
    }
    tableSize=selectedSize;table=selectedTable;
    if(!table){discoveryStatus="waiting for all 20 natives in one table";return false;}
    if(!resolve())return false;
    discoveryStatus="ready";return true;
}
inline bool supportedVersion(){
    wchar_t path[32768]{};if(!GetModuleFileNameW(nullptr,path,32768))return false;
    DWORD size=GetFileVersionInfoSizeW(path,nullptr);if(!size)return false;std::vector<uint8_t> data(size);
    if(!GetFileVersionInfoW(path,0,size,data.data()))return false;
    VS_FIXEDFILEINFO* v=nullptr;UINT len=0;if(!VerQueryValueW(data.data(),L"\\",(void**)&v,&len)||len<sizeof(*v))return false;
    return v->dwSignature==0xFEEF04BD&&v->dwFileVersionMS==MAKELONG(2,1)&&v->dwFileVersionLS==MAKELONG(59,0);
}
}
