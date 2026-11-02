// Linux input/rumble bridge: physical SDL GIP -> uinput -> Steam Input.
// No EVIOCGRAB, driver installation, Steam configuration or root required.
#include <SDL3/SDL.h>
#include <linux/uinput.h>
#include <linux/usbdevice_fs.h>
#include <filesystem>
#include <fstream>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>

static volatile sig_atomic_t stopRequested=0;
static void stop(int){stopRequested=1;}
static void fatal(const char* what){fprintf(stderr,"%s: %s\n",what,strerror(errno));}
static uint64_t now(){return SDL_GetTicks();}
struct Effect {ff_effect effect{};bool running=false;uint64_t begins=0,ends=0;};
struct Packet {uint32_t magic;uint16_t left,right;};
static_assert(sizeof(Packet)==8);
static bool event(int fd,unsigned short type,unsigned short code,int value){
    input_event e{};e.type=type;e.code=code;e.value=value;return write(fd,&e,sizeof e)==sizeof e;
}
static int createPad(){
    int fd=open("/dev/uinput",O_RDWR|O_NONBLOCK|O_CLOEXEC);if(fd<0)return -1;
    bool ok=true;auto ctl=[&](unsigned long req,unsigned long arg){if(ioctl(fd,req,arg)<0)ok=false;};
    ctl(UI_SET_EVBIT,EV_KEY);ctl(UI_SET_EVBIT,EV_ABS);ctl(UI_SET_EVBIT,EV_FF);ctl(UI_SET_FFBIT,FF_RUMBLE);
    for(int key:{BTN_SOUTH,BTN_EAST,BTN_NORTH,BTN_WEST,BTN_TL,BTN_TR,BTN_SELECT,BTN_START,BTN_MODE,BTN_THUMBL,BTN_THUMBR,BTN_TRIGGER_HAPPY1})ctl(UI_SET_KEYBIT,key);
    for(int code:{ABS_X,ABS_Y,ABS_RX,ABS_RY,ABS_Z,ABS_RZ,ABS_HAT0X,ABS_HAT0Y}){
        ctl(UI_SET_ABSBIT,code);uinput_abs_setup a{};a.code=code;
        if(code==ABS_Z||code==ABS_RZ){a.absinfo.minimum=0;a.absinfo.maximum=255;}
        else if(code==ABS_HAT0X||code==ABS_HAT0Y){a.absinfo.minimum=-1;a.absinfo.maximum=1;}
        else {a.absinfo.minimum=-32768;a.absinfo.maximum=32767;a.absinfo.flat=0;}
        if(ioctl(fd,UI_ABS_SETUP,&a)<0)ok=false;
    }
    uinput_setup setup{};setup.id.bustype=BUS_USB;setup.id.vendor=0x045e;setup.id.product=0x0b12;setup.id.version=0x050f;
    snprintf(setup.name,sizeof setup.name,"Xbox Series X Controller");setup.ff_effects_max=32;
    if(ioctl(fd,UI_DEV_SETUP,&setup)<0)ok=false;
    if(!ok||ioctl(fd,UI_DEV_CREATE)<0){close(fd);return -1;}return fd;
}
static bool copyInput(int fd,SDL_Gamepad* pad){
    // Xbox 360 evdev/SDL mapping orders X before Y by button code.
    constexpr int codes[]={BTN_SOUTH,BTN_EAST,BTN_NORTH,BTN_WEST,BTN_SELECT,BTN_MODE,BTN_START,BTN_THUMBL,BTN_THUMBR,BTN_TL,BTN_TR,BTN_TRIGGER_HAPPY1};
    constexpr SDL_GamepadButton buttons[]={SDL_GAMEPAD_BUTTON_SOUTH,SDL_GAMEPAD_BUTTON_EAST,SDL_GAMEPAD_BUTTON_WEST,SDL_GAMEPAD_BUTTON_NORTH,SDL_GAMEPAD_BUTTON_BACK,SDL_GAMEPAD_BUTTON_GUIDE,SDL_GAMEPAD_BUTTON_START,SDL_GAMEPAD_BUTTON_LEFT_STICK,SDL_GAMEPAD_BUTTON_RIGHT_STICK,SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,SDL_GAMEPAD_BUTTON_MISC1};
    bool ok=true;for(size_t i=0;i<std::size(codes);++i)ok=event(fd,EV_KEY,codes[i],SDL_GetGamepadButton(pad,buttons[i]))&&ok;
    constexpr int axes[]={ABS_X,ABS_Y,ABS_RX,ABS_RY,ABS_Z,ABS_RZ};
    for(int i=0;i<6;++i){int v=SDL_GetGamepadAxis(pad,SDL_GamepadAxis(i));if(i>=4)v=std::clamp(v,0,32767)*255/32767;ok=event(fd,EV_ABS,axes[i],v)&&ok;}
    ok=event(fd,EV_ABS,ABS_HAT0X,int(SDL_GetGamepadButton(pad,SDL_GAMEPAD_BUTTON_DPAD_RIGHT))-int(SDL_GetGamepadButton(pad,SDL_GAMEPAD_BUTTON_DPAD_LEFT)))&&ok;
    ok=event(fd,EV_ABS,ABS_HAT0Y,int(SDL_GetGamepadButton(pad,SDL_GAMEPAD_BUTTON_DPAD_DOWN))-int(SDL_GetGamepadButton(pad,SDL_GAMEPAD_BUTTON_DPAD_UP)))&&ok;
    return event(fd,EV_SYN,SYN_REPORT,0)&&ok;
}
static void feedback(int fd,std::array<Effect,32>& effects){
    input_event ev{};
    while(read(fd,&ev,sizeof ev)==sizeof ev){
        if(ev.type==EV_UINPUT&&ev.code==UI_FF_UPLOAD){
            uinput_ff_upload u{};u.request_id=ev.value;if(ioctl(fd,UI_BEGIN_FF_UPLOAD,&u)<0)continue;
            int id=u.effect.id;
            if(u.effect.type!=FF_RUMBLE||id<0||id>=int(effects.size()))u.retval=-EINVAL;
            else {auto& e=effects[id];e.effect=u.effect;if(e.running){e.begins=now()+u.effect.replay.delay;e.ends=u.effect.replay.length?e.begins+u.effect.replay.length:0;}u.retval=0;}
            ioctl(fd,UI_END_FF_UPLOAD,&u);
        }else if(ev.type==EV_UINPUT&&ev.code==UI_FF_ERASE){
            uinput_ff_erase u{};u.request_id=ev.value;if(ioctl(fd,UI_BEGIN_FF_ERASE,&u)<0)continue;
            if(u.effect_id<effects.size())effects[u.effect_id]={};
            u.retval=0;ioctl(fd,UI_END_FF_ERASE,&u);
        }else if(ev.type==EV_FF&&ev.code<effects.size()){
            auto& e=effects[ev.code];e.running=ev.value>0;e.begins=now()+e.effect.replay.delay;
            e.ends=e.effect.replay.length?e.begins+uint64_t(e.effect.replay.length)*std::clamp(ev.value,1,32):0;
        }
    }
}
// SDL/libusb does not reliably rebind xpad on this system. Explicitly reconnect
// only the exact physical controller interface after SDL releases it.
struct KernelRestore {
    int fd=-1;
    bool openDevice(){
        for(auto& e:std::filesystem::directory_iterator("/sys/bus/usb/devices")){
            auto read=[&](const char* name){std::ifstream f(e.path()/name);std::string x;f>>x;return x;};
            if(read("idVendor")!="045e"||read("idProduct")!="0b12")continue;
            if(fd>=0){close(fd);fd=-1;return false;} // ambiguous physical devices
            auto bus=read("busnum"),dev=read("devnum");if(bus.empty()||dev.empty())continue;
            char path[80];snprintf(path,sizeof path,"/dev/bus/usb/%03d/%03d",atoi(bus.c_str()),atoi(dev.c_str()));fd=open(path,O_RDWR|O_CLOEXEC);
        }
        return fd>=0;
    }
    ~KernelRestore(){if(fd>=0){usbdevfs_ioctl request{};request.ifno=0;request.ioctl_code=USBDEVFS_CONNECT;
        if(ioctl(fd,USBDEVFS_IOCTL,&request)<0&&errno!=EBUSY&&errno!=ENODEV)perror("Restore xpad");else printf("Kernel interface reconnect requested\n");close(fd);}}
};
int main(int argc,char** argv){
    setvbuf(stdout,nullptr,_IOLBF,0);setvbuf(stderr,nullptr,_IOLBF,0);
    signal(SIGINT,stop);signal(SIGTERM,stop);
    unsigned duration=0;if(argc==3&&strcmp(argv[1],"--seconds")==0)duration=unsigned(atoi(argv[2]));
    const char* runtime=getenv("XDG_RUNTIME_DIR");if(!runtime){fprintf(stderr,"XDG_RUNTIME_DIR missing\n");return 1;}
    int lock=open((std::string(runtime)+"/rumblenuts-bridge.lock").c_str(),O_CREAT|O_RDWR|O_CLOEXEC,0600);
    if(lock<0||flock(lock,LOCK_EX|LOCK_NB)<0){fprintf(stderr,"Bridge already running or lock unavailable\n");return 1;}
    int sock=socket(AF_INET,SOCK_DGRAM|SOCK_NONBLOCK|SOCK_CLOEXEC,0);sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);addr.sin_port=htons(27841);
    if(sock<0||bind(sock,(sockaddr*)&addr,sizeof addr)<0){fatal("bind localhost:27841");return 1;}
    int test=open("/dev/uinput",O_RDWR|O_NONBLOCK|O_CLOEXEC);if(test<0){fatal("uinput access");return 1;}close(test);
    KernelRestore restore;if(!restore.openDevice()){fprintf(stderr,"Cannot access unique physical Xbox USB device\n");return 1;}
    if(argc==2&&strcmp(argv[1],"--restore")==0)return 0;
    // Ignore all HIDAPI devices except this physical Xbox. Do not capture our virtual pad.
    SDL_SetHint("SDL_HIDAPI_LIBUSB","1");SDL_SetHint("SDL_HIDAPI_LIBUSB_WHITELIST","0");
    SDL_SetHint("SDL_JOYSTICK_HIDAPI","1");SDL_SetHint("SDL_JOYSTICK_HIDAPI_XBOX","1");SDL_SetHint("SDL_JOYSTICK_HIDAPI_XBOX_ONE","1");SDL_SetHint("SDL_JOYSTICK_HIDAPI_GIP","1");
    SDL_SetHint("SDL_JOYSTICK_HIDAPI_IGNORE_DEVICES_EXCEPT","0x045e/0x0b12");
    SDL_SetHint("SDL_JOYSTICK_HIDAPI_GIP_RESET_FOR_METADATA","0");SDL_SetHint("SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS","1");
    if(!SDL_Init(SDL_INIT_GAMEPAD)){fprintf(stderr,"SDL: %s\n",SDL_GetError());return 1;}
    SDL_Gamepad* pad=nullptr;uint64_t start=now();
    while(!pad&&!stopRequested&&now()-start<8000){
        SDL_PumpEvents();SDL_FlushEvents(SDL_EVENT_FIRST,SDL_EVENT_LAST);int count=0;auto ids=SDL_GetGamepads(&count);
        for(int i=0;i<count&&!pad;++i)if(SDL_GetGamepadVendorForID(ids[i])==0x045e&&SDL_GetGamepadProductForID(ids[i])==0x0b12)pad=SDL_OpenGamepad(ids[i]);
        SDL_free(ids);SDL_Delay(20);
    }
    std::string readyPath=std::string(runtime)+"/rumblenuts-bridge.ready";
    int ui=-1,result=1;
    if(!pad){fprintf(stderr,"Physical 045e:0b12 unavailable: %s\n",SDL_GetError());}
    else if(!SDL_GetBooleanProperty(SDL_GetGamepadProperties(pad),SDL_PROP_GAMEPAD_CAP_TRIGGER_RUMBLE_BOOLEAN,false)){fprintf(stderr,"Trigger capability absent; refusing to replace input\n");}
    else if((ui=createPad())<0){fatal("create virtual pad");}
    else {
        printf("READY: %s; trigger capability YES; Steam Input remains enabled; virtual Xbox Series created\n",SDL_GetGamepadName(pad));
        printf("Physical path: %s; UDP 127.0.0.1:27841; trigger cap 12.5%%; timeout 250ms\n",SDL_GetGamepadPath(pad));
        std::array<Effect,32> effects{};Packet trigger{};uint64_t received=0,lastSend=0,lastStat=now(),packets=0,nonzero=0,inputs=0;int oldL=-1,oldR=-1,oldTL=-1,oldTR=-1;
        {std::ofstream ready(readyPath);ready<<getpid()<<"\n";if(!ready){fprintf(stderr,"Cannot write readiness file\n");stopRequested=1;}}
        result=0;start=now();
        while(!stopRequested&&(!duration||now()-start<uint64_t(duration)*1000)){
            SDL_PumpEvents();SDL_FlushEvents(SDL_EVENT_FIRST,SDL_EVENT_LAST);
            if(!SDL_GamepadConnected(pad)){fprintf(stderr,"Controller disconnected; restoring kernel input\n");break;}
            if(!copyInput(ui,pad)){fatal("forward input");result=1;break;}++inputs;
            feedback(ui,effects);
            Packet incoming{};ssize_t n;
            // MSG_TRUNC rejects oversized datagrams. Only exact-size local commands count.
            while((n=recv(sock,&incoming,sizeof incoming,MSG_TRUNC))>=0){if(n==sizeof incoming&&incoming.magic==0x354E4252){trigger=incoming;received=now();++packets;if(trigger.left||trigger.right)++nonzero;}}
            int l=0,r=0;for(auto& e:effects)if(e.running&&now()>=e.begins){if(e.ends&&now()>=e.ends){e.running=false;continue;}l=std::min(65535,l+int(e.effect.u.rumble.strong_magnitude));r=std::min(65535,r+int(e.effect.u.rumble.weak_magnitude));}
            int tl=0,tr=0;if(received&&now()-received<=250){tl=std::min(8191,int(trigger.left));tr=std::min(8191,int(trigger.right));}
            if(now()-lastSend>=40){
                if(l!=oldL||r!=oldR||l||r){if(!SDL_RumbleGamepad(pad,uint16_t(l),uint16_t(r),120)){fprintf(stderr,"Main rumble failed: %s\n",SDL_GetError());result=1;break;}oldL=l;oldR=r;}
                if(tl!=oldTL||tr!=oldTR||tl||tr){if(!SDL_RumbleGamepadTriggers(pad,uint16_t(tl),uint16_t(tr),120)){fprintf(stderr,"Trigger rumble failed: %s\n",SDL_GetError());result=1;break;}oldTL=tl;oldTR=tr;}
                lastSend=now();
            }
            if(now()-lastStat>=10000){printf("Stats: input frames=%llu; ASI packets=%llu; nonzero trigger packets=%llu; main=%d/%d triggers=%d/%d\n",(unsigned long long)inputs,(unsigned long long)packets,(unsigned long long)nonzero,l,r,tl,tr);lastStat=now();}
            SDL_Delay(4);
        }
    }
    unlink(readyPath.c_str());
    if(pad){SDL_RumbleGamepad(pad,0,0,0);SDL_RumbleGamepadTriggers(pad,0,0,0);for(int i=0;i<20;++i){SDL_PumpEvents();SDL_FlushEvents(SDL_EVENT_FIRST,SDL_EVENT_LAST);SDL_Delay(5);}}
    if(ui>=0){ioctl(ui,UI_DEV_DESTROY);close(ui);}
    if(pad)SDL_CloseGamepad(pad);
    SDL_Quit();close(sock);close(lock);printf("STOPPED: bridge closed; kernel driver should reattach (reconnect USB if needed)\n");return result;
}
