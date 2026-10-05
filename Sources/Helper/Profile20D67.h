#include <signal.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sysctl.h>

#define HP_STATE "/var/jb/var/basebanddisabler/original-state.json"

// This profile temporarily clears ONE data-field guard, never an instruction
// or a dispatch pointer. It calls the stock driver's full off/on lifecycle.
// A device with missing PCI/GPIO services or enabled panic policies is refused.
static uint64_t hp_driver, hp_shadow, hp_guard;
static uint32_t hp_cfg;
static unsigned hp_pin;
static io_connect_t hp_port;
static uint64_t (*hp_rp)(uint64_t);
static uint32_t (*hp_r32)(uint64_t);
static uint8_t (*hp_r8)(uint64_t);
static int (*hp_w8)(uint64_t,uint8_t);
static unsigned hp_original_wake, hp_original_force;
static bool hp_touched, hp_restoring;
static volatile sig_atomic_t hp_stop;

static bool hp_snapshot(bool write_file) {
    char boot[64]={0}, saved_boot[64]={0};size_t length=sizeof(boot);
    if(sysctlbyname("kern.bootsessionuuid",boot,&length,NULL,0) || !boot[0])return false;
    unsigned wake=hp_original_wake,force=hp_original_force,cfg=hp_cfg;
    if(write_file) {
        int fd=open(HP_STATE ".tmp",O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW,0600);
        if(fd<0)return false;
        FILE *f=fdopen(fd,"w");if(!f){close(fd);return false;}
        bool ok=fprintf(f,"{\"version\":1,\"boot\":\"%s\",\"wake\":%u,\"force\":%u,\"gpio_cfg\":%u,\"state\":11}\n",boot,wake,force,cfg)>0;
        ok=ok && !fflush(f) && !fsync(fd);if(fclose(f))ok=false;
        return ok && !rename(HP_STATE ".tmp",HP_STATE);
    }
    int fd=open(HP_STATE,O_RDONLY|O_NOFOLLOW);if(fd<0)return false;
    struct stat st;if(fstat(fd,&st) || st.st_uid!=0 || (st.st_mode&022) || !S_ISREG(st.st_mode)){close(fd);return false;}
    FILE *f=fdopen(fd,"r");if(!f){close(fd);return false;}
    int n=fscanf(f,"{\"version\":1,\"boot\":\"%63[^\"]\",\"wake\":%u,\"force\":%u,\"gpio_cfg\":%u,\"state\":11}",saved_boot,&wake,&force,&cfg);
    fclose(f);
    if(n!=4 || strcmp(boot,saved_boot) || wake>1 || force>1 || (cfg&0x27e)!=0x202 || (cfg&~0x27e)!=(hp_cfg&~0x27e))return false;
    hp_original_wake=wake;hp_original_force=force;hp_cfg=cfg;return true;
}


static bool hp_power_read(bool *value) {
    uint64_t out=0; uint32_t count=1;
    kern_return_t r=IOConnectCallScalarMethod(hp_port,11,NULL,0,&out,&count);
    if(r || count!=1 || out>1)return false;
    *value=out!=0;return true;
}
static unsigned hp_wake_enabled(void) {
    uint64_t src=hp_rp(hp_driver+0xe8);
    return (src>>40)==0xfffffe && !(src&7) ? hp_r8(src+0x28) : 255;
}
static kern_return_t hp_power(bool on) {
    if(hp_r8(hp_guard)!=1) {puts("{\"refused\":\"absence flag changed externally\"}");return kIOReturnNotReady;}
    int w=hp_w8(hp_guard,0);
    unsigned after=hp_r8(hp_guard);
    printf("{\"temporary_absence_guard_clear\":%s,\"write_result\":%d}\n",!w && !after?"true":"false",w);
    if(w || after) {hp_w8(hp_guard,1);return kIOReturnNotWritable;}
    uint64_t input=on;
    hp_touched=true;
    kern_return_t r=IOConnectCallScalarMethod(hp_port,1,&input,1,NULL,NULL);
    int restored=hp_w8(hp_guard,1);
    printf("{\"native_power_request\":%s,\"result\":\"0x%x\",\"absence_guard_restored\":%s,\"radio_power_on_flag\":%u}\n",on?"true":"false",r,!restored && hp_r8(hp_guard)==1?"true":"false",hp_r8(hp_driver+0x2b9));
    return r ? r : (restored || hp_r8(hp_guard)!=1 ? kIOReturnNotWritable : 0);
}
static bool hp_recover(void) {
    if(!hp_touched || hp_restoring)return !hp_touched;
    hp_restoring=true;
    if(hp_r8(hp_guard)!=1)hp_w8(hp_guard,1);
    if(hp_r8(hp_driver+0x2b9)==0)hp_power(true);
    if(hp_wake_enabled()!=hp_original_wake) {
        uint64_t wake=hp_original_wake;
        kern_return_t r=IOConnectCallScalarMethod(hp_port,21,&wake,1,NULL,NULL);
        printf("{\"wake_restore_result\":\"0x%x\"}\n",r);
    }
    if(hp_r8(hp_driver+0x2b8)!=hp_original_force)hp_w8(hp_driver+0x2b8,hp_original_force);
    bool power=false,pr=hp_power_read(&power);
    // Restore the original crashed state only AFTER original physical-on and
    // GPIO configuration are verified. This is never used as proof of off.
    bool gpio=hp_r32(hp_shadow+hp_pin*4)==hp_cfg;
    bool state_restored=false;
    if(pr && power && hp_r8(hp_driver+0x2b9)==1 && gpio) {
        uint64_t state=11;
        kern_return_t r=IOConnectCallScalarMethod(hp_port,27,&state,1,NULL,NULL);
        printf("{\"original_crashed_state_restore_result\":\"0x%x\"}\n",r);
        uint64_t observed=0;uint32_t count=1;
        state_restored=!r && !IOConnectCallScalarMethod(hp_port,26,NULL,0,&observed,&count) && count==1 && observed==11;
    }
    bool ok=pr && power && state_restored && hp_r8(hp_guard)==1 && hp_r8(hp_driver+0x2b9)==1 && gpio &&
            hp_wake_enabled()==hp_original_wake && hp_r8(hp_driver+0x2b8)==hp_original_force;
    printf("{\"restore_verified\":%s,\"pmu_ext_on\":%s,\"gpio_cfg\":\"0x%x\",\"absence_flag\":%u}\n",ok?"true":"false",power?"true":"false",hp_r32(hp_shadow+hp_pin*4),hp_r8(hp_guard));
    hp_touched=!ok;hp_restoring=false;return ok;
}
static void hp_exit_restore(void) { if(hp_touched)hp_recover(); }
static void hp_signal(int sig) {hp_stop=sig;}
static int native_power_hold(io_service_t service,io_connect_t port,uint64_t driver,uint64_t slide,
    uint64_t (*rp)(uint64_t),uint32_t (*r32)(uint64_t),uint8_t (*r8)(uint64_t),int (*w8)(uint64_t,uint8_t),unsigned seconds) {
    uint64_t vt=rp(driver),bp=rp(driver+0x188);
    bool layout=rp(vt+0x550)==UINT64_C(0xfffffe0008c9a52c)+slide &&
                rp(vt+0x6b8)==UINT64_C(0xfffffe0008c9c514)+slide &&
                rp(vt+0x6c0)==UINT64_C(0xfffffe0008c9c67c)+slide &&
                (bp>>40)==0xfffffe && rp(rp(bp)+0x540)==UINT64_C(0xfffffe0008cae048)+slide && r32(bp+0x88)==1;
    char build[64]={0};size_t build_size=sizeof(build);
    if(sysctlbyname("kern.osversion",build,&build_size,NULL,0) || strcmp(build,"20D67"))return 33;
    if(!layout || !w8 || seconds>181)return 20;
    uint64_t arr=rp(bp+0x90);if((arr>>40)!=0xfffffe || (arr&7))return 21;
    uint64_t function=rp(arr);
    if((function>>40)!=0xfffffe || rp(function)!=UINT64_C(0xfffffe0007b336f0)+slide || rp(rp(function)+0x80)!=UINT64_C(0xfffffe0008fa8300)+slide)return 22;
    uint64_t target=rp(function+0x28);if((target>>40)!=0xfffffe || (target&7))return 23;
    uint64_t gm=rp(rp(target)+0x5f8);
    if(gm!=UINT64_C(0xfffffe0008fa39c4)+slide && gm!=UINT64_C(0xfffffe0008fa5bd8)+slide)return 24;
    unsigned pin=r32(function+0x38),pins=r32(target+0xc8);
    uint64_t shadow=rp(target+0xc0);
    if(pin>=pins || pins>4096 || (shadow>>40)!=0xfffffe || (shadow&3))return 25;
    uint32_t cfg=r32(shadow+pin*4);
    unsigned default_output=(r8(function+0x3e)^r8(function+0x3d)^1)&1;
    bool ready=r8(driver+0x300)==1 && rp(driver+0x2d0) && rp(driver+0x2d8) && rp(driver+0x2f8);
    bool panics_off=!r8(driver+0x2ea) && !r8(driver+0x2eb) && !r8(driver+0x2ee);
    bool state=r8(driver+0x2c1)==1 && r8(driver+0x2b9)<=1 && !r8(driver+0x2ba) && !r8(driver+0xc5);
    bool default_matches=r8(function+0x3c)==1 && (cfg&0x27e)==0x202 && (cfg&1)==default_output;
    CFTypeRef data=IORegistryEntrySearchCFProperty(service,kIOServicePlane,CFSTR("function-bb_on"),kCFAllocatorDefault,kIORegistryIterateParents|kIORegistryIterateRecursively);
    const uint8_t expected[]={0x61,0,0,0,0x34,0x57,0x4b,0x70,0x37,0x31,0x50,0x67,0,0,0x80,0};
    bool descriptor=data && CFGetTypeID(data)==CFDataGetTypeID() && CFDataGetLength(data)==sizeof(expected) && !memcmp(CFDataGetBytePtr(data),expected,sizeof(expected));
    if(data)CFRelease(data);
    printf("{\"hold_prerequisites\":{\"pci_ready\":%s,\"panic_policies_disabled\":%s,\"original_state_matches\":%s,\"gpio_default_matches\":%s,\"supply_descriptor_matches\":%s}}\n",ready?"true":"false",panics_off?"true":"false",state?"true":"false",default_matches?"true":"false",descriptor?"true":"false");
    unsigned cfg_pull=cfg&0xe,cfg_input=(cfg_pull==2 || cfg_pull==14)?0x200:(cfg_pull|0x200);
    bool initially_input=(cfg&0x27e)==cfg_input;
    if(!ready || !panics_off || !state || (!default_matches && !initially_input) || !descriptor)return 26;
    hp_driver=driver;hp_port=port;hp_guard=driver+0x2c1;hp_shadow=shadow;hp_pin=pin;hp_cfg=cfg;
    hp_rp=rp;hp_r32=r32;hp_r8=r8;hp_w8=w8;hp_original_wake=hp_wake_enabled();hp_original_force=r8(driver+0x2b8);
    bool initial=false;
    if(hp_original_wake>1 || hp_original_force>1 || !hp_power_read(&initial))return 27;
    uint64_t initial_state=~UINT64_C(0);uint32_t state_count=1;
    kern_return_t sr=IOConnectCallScalarMethod(port,26,NULL,0,&initial_state,&state_count);
    if(sr || state_count!=1)return 32;
    if(seconds==181 && initial && r8(driver+0x2b9)==1 && default_matches && initial_state==11) {
        puts("{\"already_restored_verified\":true}");return 0;
    }
    if(!initial && r8(driver+0x2b9)==0 && initially_input && initial_state==1) {
        if(!hp_snapshot(false)){puts("{\"refused\":\"no matching restoration snapshot\"}");return 34;}
        if(seconds==0){puts("{\"already_off_verified\":true}");return 0;}
        if(seconds!=181)return 35;
        atexit(hp_exit_restore);signal(SIGTERM,hp_signal);signal(SIGINT,hp_signal);signal(SIGHUP,hp_signal);
        hp_touched=true;return hp_recover()?0:29;
    }
    if(seconds==181 || !initial || r8(driver+0x2b9)!=1 || !default_matches || initial_state!=11)return 32;
    if(seconds==0 && !hp_snapshot(true)){puts("{\"refused\":\"could not save original state\"}");return 36;}
    atexit(hp_exit_restore);signal(SIGTERM,hp_signal);signal(SIGINT,hp_signal);signal(SIGHUP,hp_signal);
    kern_return_t off=hp_power(false);
    bool value=true,read=hp_power_read(&value);
    uint32_t current=r32(shadow+pin*4),pull=current&0xe,input=(pull==2 || pull==14)?0x200:(pull|0x200);
    bool pin_input=(current&0x27e)==input;
    printf("{\"off_observation\":{\"pmu_read\":%s,\"pmu_ext_on\":%s,\"gpio_now_input\":%s,\"gpio_cfg\":\"0x%x\"}}\n",read?"true":"false",value?"true":"false",pin_input?"true":"false",current);
    uint64_t off_state=0;uint32_t off_count=1;
    bool state_off=!IOConnectCallScalarMethod(hp_port,26,NULL,0,&off_state,&off_count) && off_count==1 && off_state==1;
    if(off || !read || value || r8(driver+0x2b9)!=0 || r8(hp_guard)!=1 || !pin_input || !state_off || hp_stop){hp_recover();return 28;}
    if(seconds==0){hp_touched=false;puts("{\"off_committed_verified\":true}");return 0;}
    puts("HOLD_STARTED");
    struct timespec start,now;clock_gettime(CLOCK_MONOTONIC,&start);
    while(!hp_stop){clock_gettime(CLOCK_MONOTONIC,&now);if(now.tv_sec-start.tv_sec>=seconds)break;struct timespec wait={1,0};nanosleep(&wait,NULL);}
    bool recovered=hp_recover();
    return recovered?0:29;
}
