#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include "game.hpp"
#include "engine.hpp"
#include "shooting.hpp"
#include "trigger_packet.hpp"
#include <xinput.h>
#include <cstdio>
#include <string>
#include <atomic>

static HMODULE self;
static std::wstring folder;
static CRITICAL_SECTION lock;
static XINPUT_VIBRATION desired{};
static ULONGLONG freshness=0;
static bool owned=false;
static ULONGLONG yieldUntil=0;
// Hand off without a zero command: that would cancel the game's shot rumble.
static void yieldToGame(ULONGLONG now){
    EnterCriticalSection(&lock);yieldUntil=now+650;owned=false;desired={};LeaveCriticalSection(&lock);
}
static std::atomic<bool> quitting{false};
static DWORD controller=0;
static bool triggerEnabled=true;
static unsigned triggerPercent=5;
static bool triggerAccel=true,triggerBrake=true,triggerShotFoot=true,triggerShotVehicle=false;
static float triggerDriveGain=1,triggerShotGain=1,shotFootGain=1.5f,shotVehicleGain=1.5f;
static uint16_t triggerLeft=0,triggerRight=0,shotTrigger=0;
static ULONGLONG triggerFreshness=0,shotFreshness=0;
static void publishTriggers(uint16_t l=0,uint16_t r=0){EnterCriticalSection(&lock);triggerLeft=l;triggerRight=r;triggerFreshness=GetTickCount64();LeaveCriticalSection(&lock);}
using TriggerPacket=rn::TriggerPacket;
static bool saveHashes=true;
static void append(const wchar_t* name,const char* text){
    HANDLE f=CreateFileW((folder+name).c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(f!=INVALID_HANDLE_VALUE){DWORD n;WriteFile(f,text,DWORD(strlen(text)),&n,nullptr);WriteFile(f,"\r\n",2,&n,nullptr);CloseHandle(f);}
}
static bool foreground(){DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);return pid==GetCurrentProcessId();}
static void publish(uint16_t l=0,uint16_t r=0,bool active=false){EnterCriticalSection(&lock);desired={l,r};freshness=GetTickCount64();owned=active;LeaveCriticalSection(&lock);}
using SetState=DWORD(WINAPI*)(DWORD,XINPUT_VIBRATION*);
static SetState setState=nullptr;
static std::atomic<SetState> gameSetState{nullptr};
static std::atomic<ULONGLONG> boostUntil{0};
static std::atomic<bool> onFoot{false};
static decltype(&GetProcAddress) previousResolver=nullptr;
static DWORD WINAPI gameRumble(DWORD index,XINPUT_VIBRATION* vibration){
    auto original=gameSetState.load();
    if(!original)return ERROR_DEVICE_NOT_CONNECTED;
    if(!vibration)return original(index,vibration);
    XINPUT_VIBRATION v=*vibration;
    if(index==controller&&GetTickCount64()<boostUntil.load()){
        static std::atomic<bool> reported{false};
        if((v.wLeftMotorSpeed||v.wRightMotorSpeed)&&!reported.exchange(true))
            append(L"GTA-IV-HD_Rumble.log","First nonzero game rumble boosted during shooting (configured gain).");
        float shotGain=onFoot.load()?shotFootGain:shotVehicleGain;
        v.wLeftMotorSpeed=rn::boostShot(v.wLeftMotorSpeed,shotGain);
        v.wRightMotorSpeed=rn::boostShot(v.wRightMotorSpeed,shotGain);
        EnterCriticalSection(&lock);
        shotTrigger=(onFoot.load()?triggerShotFoot:triggerShotVehicle)?uint16_t(std::min(float(std::max(v.wLeftMotorSpeed,v.wRightMotorSpeed))/8*triggerShotGain,float(65535*triggerPercent/100))):0;
        shotFreshness=GetTickCount64();
        LeaveCriticalSection(&lock);
    }
    return original(index,&v);
}
static FARPROC WINAPI resolveGameProc(HMODULE module,LPCSTR name){
    FARPROC proc=previousResolver(module,name);
    if(proc&&uintptr_t(name)>0xFFFF&&strcmp(name,"XInputSetState")==0){
        gameSetState.store(reinterpret_cast<SetState>(proc));
        append(L"GTA-IV-HD_Rumble.log","Game XInputSetState captured: configurable shooting gain ready.");
        return reinterpret_cast<FARPROC>(gameRumble);
    }
    return proc;
}
static bool installResolver(){
    auto base=reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto nt=reinterpret_cast<IMAGE_NT_HEADERS*>(base+dos->e_lfanew);
    auto rva=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if(!rva)return false;
    for(auto desc=reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base+rva);desc->Name;++desc){
        if(_stricmp(reinterpret_cast<char*>(base+desc->Name),"KERNEL32.DLL")||!desc->OriginalFirstThunk)continue;
        auto names=reinterpret_cast<IMAGE_THUNK_DATA*>(base+desc->OriginalFirstThunk);
        auto slots=reinterpret_cast<IMAGE_THUNK_DATA*>(base+desc->FirstThunk);
        for(;names->u1.AddressOfData;++names,++slots){
            if(IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal))continue;
            auto name=reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base+names->u1.AddressOfData);
            if(strcmp(reinterpret_cast<char*>(name->Name),"GetProcAddress"))continue;
            DWORD protection=0;
            if(!VirtualProtect(&slots->u1.Function,sizeof(uintptr_t),PAGE_READWRITE,&protection))return false;
            previousResolver=reinterpret_cast<decltype(previousResolver)>(slots->u1.Function);
            InterlockedExchange(reinterpret_cast<LONG*>(&slots->u1.Function),LONG(reinterpret_cast<uintptr_t>(&resolveGameProc)));
            DWORD ignored;VirtualProtect(&slots->u1.Function,sizeof(uintptr_t),protection,&ignored);
            return true;
        }
    }
    return false;
}
static DWORD WINAPI outputThread(void*){
    bool driving=false;DWORD previous=~DWORD(0);
    SOCKET udp=INVALID_SOCKET;WSADATA wsa{};
    sockaddr_in destination{};destination.sin_family=AF_INET;destination.sin_port=htons(27841);destination.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    bool network=triggerEnabled&&WSAStartup(MAKEWORD(2,2),&wsa)==0;
    if(network){udp=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);if(udp!=INVALID_SOCKET){u_long nonblocking=1;ioctlsocket(udp,FIONBIO,&nonblocking);append(L"GTA-IV-HD_Rumble.log","Trigger UDP sender ready: 127.0.0.1:27841; requires Linux bridge with Steam Input ON.");}}

    while(!quitting.load()){
        XINPUT_VIBRATION v{};bool active;
        // Keep the decision and XInput write under one lock so a queued
        // driving command cannot run after yieldToGame has returned.
        EnterCriticalSection(&lock);
        bool yielding=GetTickCount64()<yieldUntil;
        active=owned&&GetTickCount64()-freshness<=250&&foreground();v=desired;
        DWORD result=previous;bool sent=false;
        if(yielding){driving=false;}
        else {
            if(active||driving){if(!active)v={};result=setState(controller,&v);sent=true;}
            driving=active;
        }
        LeaveCriticalSection(&lock);
        if(sent&&result!=previous){char b[120];snprintf(b,sizeof b,"XInput slot %lu: status %lu (0=OK, 1167=disconnected)",controller,result);append(L"GTA-IV-HD_Rumble.log",b);previous=result;}
        if(udp!=INVALID_SOCKET){TriggerPacket packet;
            EnterCriticalSection(&lock);
            ULONGLONG t=GetTickCount64();
            if(t-triggerFreshness<=150){packet.left=triggerLeft;packet.right=triggerRight;if(packet.left||packet.right)packet.flags=rn::drivingTriggers;}
            if((onFoot.load()?triggerShotFoot:triggerShotVehicle)&&t-shotFreshness<=100)packet.right=std::max(packet.right,shotTrigger);
            LeaveCriticalSection(&lock);
            if(!foreground())packet.left=packet.right=0;
            sendto(udp,(const char*)&packet,sizeof packet,0,(sockaddr*)&destination,sizeof destination);
        }
        Sleep(20);
    }
    XINPUT_VIBRATION stop{};setState(controller,&stop);
    if(udp!=INVALID_SOCKET){TriggerPacket packet;sendto(udp,(const char*)&packet,sizeof packet,0,(sockaddr*)&destination,sizeof destination);closesocket(udp);}
    if(network)WSACleanup();
    return 0;
}
static rn::Engine engine;
static bool overlay=false;
static rn::Output lastNonzero{};
static ULONGLONG lastNonzeroAt=0;
static std::string notice;
static ULONGLONG noticeUntil=0;
static void message(const char* s){notice=s;noticeUntil=GetTickCount64()+1600;}
static bool pressed(int key,int i){static bool was[3]{};bool down=(GetAsyncKeyState(key)&0x8000)!=0;bool hit=down&&!was[i];was[i]=down;return hit;}
static void show(const char* text){game::call(game::print,"STRING",text,uint32_t(120),uint32_t(1));}
static void tick(){
    static bool ready=false;static ULONGLONG retry=0,last=0;static uint32_t* oldTable=nullptr;
    static std::string lastDiagnostic;
    ULONGLONG now=GetTickCount64();
    if(ready && oldTable!=*game::table){ready=false;engine.reset();publish();}
    if(!ready){
        if(now<retry)return;
        retry=now+1000;
        ready=game::discover();
        char diagnostic[240];snprintf(diagnostic,sizeof diagnostic,"Discovery: %s; size matches=%u table matches=%u pause matches=%u",game::discoveryStatus,unsigned(game::sizeMatches),unsigned(game::tableMatches),unsigned(game::pauseMatches));
        if(lastDiagnostic!=diagnostic){append(L"GTA-IV-HD_Rumble.log",diagnostic);lastDiagnostic=diagnostic;}
        if(!ready)return;
        oldTable=*game::table;
        append(L"GTA-IV-HD_Rumble.log","All 20 native handlers resolved; game callback active.");
        message("GTA-IV-HD_Rumble: P=Profile O=Overlay F10=Hash");
    }
    if(*game::userPause||*game::scriptPause||!foreground()){boostUntil.store(0);publish();publishTriggers();engine.reset();return;}
    if(game::call<bool>(game::paused)){publish();publishTriggers();engine.reset();return;}
    bool p=pressed('P',0),o=pressed('O',1),hash=pressed(VK_F10,2);
    if(p){lastNonzeroAt=0;engine.profile=(engine.profile+1)%3;std::string s="Profile: ";s+=rn::profileName(engine.profile);message(s.c_str());}
    if(o){overlay=!overlay;message(overlay?"Overlay ON":"Overlay OFF");}
    uint32_t ped=0,car=0;
    uint32_t player=game::call(game::playerIndex,game::call(game::playerId));
    bool valid=game::call<bool>(game::playing,player)!=0;
    if(valid){game::call(game::playerChar,player,&ped);valid=ped&&game::call<bool>(game::charExists,ped)&&!game::call<bool>(game::charDead,ped);}
    bool firing=valid&&game::call<uint32_t>(game::shooting,ped);
    if(firing)boostUntil.store(now+150);
    bool inVehicle=valid&&game::call<bool>(game::inCar,ped);
    onFoot.store(valid&&!inVehicle);
    valid=inVehicle;
    if(valid){game::call(game::carUsing,ped,&car);valid=car&&game::call<bool>(game::carExists,car);}
    static ULONGLONG shootingUntil=0;
    if(firing){
        shootingUntil=now+650;yieldToGame(now);
    }
    bool shooting=now<shootingUntil;
    static rn::Sample sample{};static rn::Output output{};
    if(!valid){lastNonzeroAt=0;publish();publishTriggers();engine.reset();if(hash)message("Not in vehicle.");}
    else {
        if(now-last>=70 || hash){
            rn::Sample s;s.vehicle=car;float x=0,y=0;uint32_t health=0;
            game::call(game::model,car,&s.model);game::call(game::speed,car,&s.speed);game::call(game::coords,car,&x,&y,&s.z);game::call(game::heading,car,&s.heading);game::call(game::health,car,&health);s.health=int(health);
            s.bike=game::call<bool>(game::bike,s.model)!=0;s.boat=game::call<bool>(game::boat,s.model)!=0;
            if(!s.boat)for(int i=0;i<6;++i){if(s.bike&&i!=0&&i!=4)continue;s.tyres[i]=game::call<bool>(game::tyre,car,uint32_t(i))!=0;}
            if(std::isfinite(s.speed)&&std::isfinite(s.z)&&std::isfinite(s.heading)){
                if(now-last>=70){output=engine.step(s,now);
                    unsigned triggerLevel=unsigned(std::min(float(std::max(output.left,output.right))*2*triggerDriveGain,float(65535*triggerPercent/100)));
                    publishTriggers(!shooting&&triggerBrake&&output.braking?uint16_t(triggerLevel):0,!shooting&&triggerAccel&&output.acceleration?uint16_t(triggerLevel):0);if(output.left||output.right){lastNonzero=output;lastNonzeroAt=now;}if(shooting){output={};output.event="Shooting: game rumble";lastNonzeroAt=0;publish();}else publish(output.left,output.right,true);last=now;}sample=s;
            }else{publish();engine.reset();}
        }
        if(hash){char b[160];snprintf(b,sizeof b,"%s,%08X",rn::group(sample.model),sample.model);if(saveHashes)append(L"SavedVehicleHashes.txt",b);message(b);}
    }
    if(now<noticeUntil)show(notice.c_str());
    else if(overlay&&valid){
        bool recent=lastNonzeroAt&&now-lastNonzeroAt<1500;
        char b[280];snprintf(b,sizeof b,"%s | %s | %.0f km/h | Now %u/%u | Last %s %u/%u",rn::profileName(engine.profile),rn::group(sample.model),sample.speed*3.6f,output.left,output.right,recent?lastNonzero.event:"None",recent?lastNonzero.left:0,recent?lastNonzero.right:0);show(b);
    }
}
static void(__cdecl* originalProcess)()=nullptr;
static void __cdecl processHook(){originalProcess();tick();}
extern "C" __declspec(dllexport) void InitializeASI(){
    static bool initialized=false;if(initialized)return;initialized=true;
    wchar_t path[32768]{};GetModuleFileNameW(self,path,32768);folder=path;folder.resize(folder.find_last_of(L"/\\")+1);
    append(L"GTA-IV-HD_Rumble.log","GTA-IV-HD_Rumble 1.0 configurable triggers: initialization");
    if(!game::supportedVersion()){append(L"GTA-IV-HD_Rumble.log","DISABLED: requires GTAIV.exe file version 1.2.0.59.");return;}
    auto hits=game::scan("E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? B9 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? B9");
    if(hits.size()!=1){append(L"GTA-IV-HD_Rumble.log","DISABLED: CGameProcess call signature missing or ambiguous.");return;}
    auto call=hits[0];originalProcess=(void(__cdecl*)())(call+5+game::operand<int32_t>(call,1));
    if(!game::executable((void*)originalProcess)){append(L"GTA-IV-HD_Rumble.log","DISABLED: original process target is not executable.");return;}
    for(auto name:{L"xinput1_4.dll",L"xinput1_3.dll",L"xinput9_1_0.dll"}){HMODULE h=LoadLibraryW(name);if(h){setState=(SetState)GetProcAddress(h,"XInputSetState");if(setState)break;FreeLibrary(h);}}
    if(!setState){append(L"GTA-IV-HD_Rumble.log","DISABLED: XInputSetState not available.");return;}
    auto ini=folder+L"GTA-IV-HD_Rumble.ini";
    auto setting=[&](const wchar_t* section,const wchar_t* key,float fallback,float low,float high){
        wchar_t raw[128]{};GetPrivateProfileStringW(section,key,L"",raw,128,ini.c_str());
        char ascii[128]{};for(unsigned i=0;i<127&&raw[i];++i)ascii[i]=raw[i]<128?char(raw[i]):'?';
        return rn::parseSetting(ascii,fallback,low,high);
    };
    auto gain=[&](const wchar_t* section,const wchar_t* key,float fallback=100.f){return setting(section,key,fallback,0,400)/100;};
    auto flag=[&](const wchar_t* key,bool fallback){return setting(L"Triggers",key,fallback?1.f:0.f,0,1)>=.5f;};
    controller=DWORD(setting(L"GTA-IV-HD_Rumble",L"ControllerIndex",0,0,3));
    engine.profile=int(setting(L"GTA-IV-HD_Rumble",L"Profile",1,0,2));
    engine.settings.mainGain=gain(L"GTA-IV-HD_Rumble",L"MainStrengthPercent");
    engine.settings.motorLimit=int(setting(L"GTA-IV-HD_Rumble",L"MotorLimit",1500,0,65535));
    engine.settings.tyreStrength=int(setting(L"GTA-IV-HD_Rumble",L"TyreStrength",300,0,65535));
    engine.settings.acceleration=gain(L"Effects",L"Acceleration");
    engine.settings.braking=gain(L"Effects",L"Braking");
    engine.settings.gear=gain(L"Effects",L"Gear");
    engine.settings.bump=gain(L"Effects",L"Bumps");
    engine.settings.turn=gain(L"Effects",L"Turns");
    engine.settings.damage=gain(L"Effects",L"Damage");
    engine.settings.tyres=gain(L"Effects",L"Tyres");
    const wchar_t* sections[]={L"Strong",L"Medium",L"Light"};
    for(int i=0;i<3;++i){
        engine.settings.profileGain[i]=gain(sections[i],L"StrengthPercent");
        engine.settings.accelThreshold[i]=setting(sections[i],L"AccelerationThreshold",engine.settings.accelThreshold[i],.001f,10);
        engine.settings.brakeThreshold[i]=setting(sections[i],L"BrakingThreshold",engine.settings.brakeThreshold[i],.001f,10);
        engine.settings.tyreSpeed[i]=setting(sections[i],L"TyreMinSpeedKmh",engine.settings.tyreSpeed[i]*3.6f,0,360)/3.6f;
    }
    shotFootGain=gain(L"Shooting",L"OnFootPercent",150);
    shotVehicleGain=gain(L"Shooting",L"InVehiclePercent",150);
    triggerEnabled=flag(L"Enabled",true);
    triggerPercent=unsigned(setting(L"Triggers",L"StrengthPercent",5,0,12));
    triggerAccel=flag(L"Acceleration",true);triggerBrake=flag(L"Braking",true);
    triggerShotFoot=flag(L"ShootingOnFoot",true);triggerShotVehicle=flag(L"ShootingInVehicle",false);
    triggerDriveGain=gain(L"Triggers",L"DrivingPercent");triggerShotGain=gain(L"Triggers",L"ShootingPercent");
    saveHashes=setting(L"GTA-IV-HD_Rumble",L"SaveHashes",1,0,1)>=.5f;
    InitializeCriticalSection(&lock);
    append(L"GTA-IV-HD_Rumble.log",installResolver()?"Game GetProcAddress hook installed; waiting for XInputSetState resolution.":"WARNING: game resolver hook unavailable; shooting boost inactive.");
    // Loaded once at process startup by UAL. Pin: unloading a live call hook is unsupported.
    HMODULE pinned=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,(LPCWSTR)&InitializeASI,&pinned);
    HANDLE worker=CreateThread(nullptr,0,outputThread,nullptr,0,nullptr);
    if(!worker){append(L"GTA-IV-HD_Rumble.log","DISABLED: cannot start XInput watchdog.");return;}CloseHandle(worker);
    DWORD old=0;if(!VirtualProtect(call,5,PAGE_EXECUTE_READWRITE,&old)){quitting=true;append(L"GTA-IV-HD_Rumble.log","DISABLED: cannot install process callback.");return;}
    int32_t relative=int32_t((uintptr_t)&processHook-(uintptr_t)(call+5));std::memcpy(call+1,&relative,4);
    DWORD ignored;VirtualProtect(call,5,old,&ignored);FlushInstructionCache(GetCurrentProcess(),call,5);
    append(L"GTA-IV-HD_Rumble.log","Process callback installed (existing call target chained). Awaiting playable game.");
}
BOOL WINAPI DllMain(HMODULE module,DWORD reason,LPVOID){
    if(reason==DLL_PROCESS_ATTACH){self=module;DisableThreadLibraryCalls(module);}
    // No loader-lock waits, file IO or game calls in DllMain.
    if(reason==DLL_PROCESS_DETACH)quitting=true;
    return TRUE;
}
