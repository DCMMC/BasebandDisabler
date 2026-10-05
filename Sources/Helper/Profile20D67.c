#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <dlfcn.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Default invocation is read-only. Power commands temporarily clear the
// missing-radio guard and invoke the stock full power lifecycle.
#include "Profile20D67.h"

int BDNativeMain(int argc,char **argv) {
    setbuf(stdout, NULL);
    void *tl = dlopen("/usr/lib/libTelephonyBasebandDynamic.dylib", RTLD_NOW);
    CFTypeRef (*create)(CFAllocatorRef) = tl ? dlsym(tl, "TelephonyBasebandCreateController") : NULL;
    Boolean (*getstate)(CFTypeRef, unsigned *) = tl ? dlsym(tl, "TelephonyBasebandGetBasebandState") : NULL;
    Boolean (*getpower)(CFTypeRef, Boolean *) = tl ? dlsym(tl, "TelephonyBasebandGetPMUExtOn") : NULL;
    if (create && getstate && getpower) {
        CFTypeRef c = create(NULL);
        printf("{\"controller_open\":%s}\n", c ? "true" : "false");
        if (c) {
            unsigned state = ~0u; Boolean power = 0;
            Boolean sr = getstate(c, &state), pr = getpower(c, &power);
            printf("{\"state_read\":%s,\"state\":%u,\"pmu_read\":%s,\"pmu_ext_on\":%s}\n",
                   sr ? "true" : "false", state, pr ? "true" : "false", power ? "true" : "false");
            CFRelease(c);
        }
    }
    void *jb = dlopen("/var/jb/basebin/libjailbreak.dylib", RTLD_NOW);
    int (*checkin)(char **, char **, char **, bool *) = jb ? dlsym(jb, "jbclient_process_checkin") : NULL;
    int (*init)(void) = jb ? dlsym(jb, "jbclient_initialize_primitives") : NULL;
    uint64_t (*self)(void) = jb ? dlsym(jb, "task_self") : NULL;
    uint64_t (*kobject)(uint64_t, mach_port_name_t) = jb ? dlsym(jb, "task_get_ipc_port_kobject") : NULL;
    uint64_t (*readptr)(uint64_t) = jb ? dlsym(jb, "kread_ptr") : NULL;
    uint32_t (*read32)(uint64_t) = jb ? dlsym(jb, "kread32") : NULL;
    uint8_t (*read8)(uint64_t) = jb ? dlsym(jb, "kread8") : NULL;
    bool (*kcall_available)(void) = jb ? dlsym(jb, "is_kcall_available") : NULL;
    if (!checkin || !init || !self || !kobject || !readptr || !read32 || !read8 || !kcall_available) return 2;
    // Commands inspect, change power, then inspect using the same primitives.
    // Dopamine initializes page-table primitives once per process.
    static bool primitives_ready=false;
    int cr=0,ir=0;
    if(!primitives_ready) {
        char *root = NULL, *uuid = NULL, *ext = NULL; bool debug = false;
        cr=checkin(&root,&uuid,&ext,&debug);ir=cr?-1:init();
        free(root);free(uuid);free(ext);
        primitives_ready=!cr && !ir;
    }
    printf("{\"checkin_result\":%d,\"primitive_init_result\":%d}\n", cr, ir);
    if (cr || ir) return 3;
    printf("{\"kcall_available\":%s}\n", kcall_available() ? "true" : "false");
    io_service_t s = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("AppleBasebandM20"));
    io_connect_t port = 0;
    kern_return_t kr = s ? IOServiceOpen(s, mach_task_self(), 0, &port) : kIOReturnNotFound;
    printf("{\"native_open_result\":\"0x%x\"}\n", kr);
    if (kr || !port) return 4;
    uint64_t uc = kobject(self(), port), driver = uc ? readptr(uc + 0xd8) : 0;
    if ((driver >> 40) != 0xfffffe || (driver & 7)) return 5;
    uint64_t vt = readptr(driver), slide = vt - UINT64_C(0xfffffe0007a93d08);
    bool verified = !(slide & 0xfff) && slide < UINT64_C(0x80000000) &&
                    readptr(vt + 0x5f8) == UINT64_C(0xfffffe0008c9accc) + slide;
    printf("{\"layout_verified\":%s,\"slide\":\"0x%llx\"}\n", verified ? "true" : "false", (unsigned long long)slide);
    if (verified) {
        printf("{\"radio_not_found\":%u,\"pcie_enable_retry\":%u,\"power_sequence_index\":%u,\"force_block_wake\":%u,\"radio_power_on_flag\":%u,\"radio_usb_on_flag\":%u}\n",
               read8(driver + 0x2c1), read8(driver + 0x2ec), read8(driver + 0xc5),
               read8(driver + 0x2b8), read8(driver + 0x2b9), read8(driver + 0x2ba));
        printf("{\"pci_driver_ready\":%u,\"panic_on_enable_failure\":%u,\"pcie_power_function_present\":%s,\"pcie_client_present\":%s}\n",read8(driver+0x300),read8(driver+0x2eb),readptr(driver+0x2d0)?"true":"false",readptr(driver+0x2d8)?"true":"false");
        uint64_t bp = readptr(driver + 0x188);
        bool bp_ok = (bp >> 40) == 0xfffffe && !(bp & 7) &&
                     readptr(readptr(bp) + 0x540) == UINT64_C(0xfffffe0008cae048) + slide;
        unsigned count = bp_ok ? read32(bp + 0x88) : ~0u;
        printf("{\"backpower_layout_verified\":%s,\"backpower_pin_count\":%u}\n", bp_ok ? "true" : "false", count);
        uint64_t array = bp_ok ? readptr(bp + 0x90) : 0;
        if ((array >> 40) == 0xfffffe && !(array & 7) && count <= 16) {
            for (unsigned i = 0; i < count; ++i) {
                uint64_t function = readptr(array + i * 8);
                if ((function >> 40) != 0xfffffe || (function & 7)) continue;
                uint64_t fvt = readptr(function);
                uint64_t target=readptr(function+0x28),tvt=((target>>40)==0xfffffe && !(target&7))?readptr(target):0;
                uint32_t pin=read32(function+0x38);
                uint64_t gm=tvt?readptr(tvt+0x5f8):0;
                bool gpio_ok=gm==UINT64_C(0xfffffe0008fa39c4)+slide || gm==UINT64_C(0xfffffe0008fa5bd8)+slide;
                uint32_t pins=gpio_ok?read32(target+0xc8):0;
                uint64_t shadow=gpio_ok?readptr(target+0xc0):0;
                bool shadow_ok=gpio_ok && pin<pins && (shadow>>40)==0xfffffe && !(shadow&3);
                uint32_t cfg=shadow_ok?read32(shadow+pin*4):~0u;
                uint32_t pull=cfg&0xe,input_value=(pull==2 || pull==14)?0x200:(pull|0x200);
                printf("{\"gpio_provider_verified\":%s,\"pin\":%u,\"shadow_verified\":%s,\"shadow_cfg\":\"0x%x\",\"already_in_input_mode\":%s,\"default_mode\":%u,\"default_value\":%u}\n",gpio_ok?"true":"false",pin,shadow_ok?"true":"false",cfg,shadow_ok&&((cfg&0x27e)==input_value)?"true":"false",read8(function+0x3c),read8(function+0x3e));
                printf("{\"backpower_pin\":%u,\"static_vtable\":\"0x%llx\",\"static_call_method\":\"0x%llx\"}\n",
                       i, (unsigned long long)(fvt - slide), (unsigned long long)(readptr(fvt + 0x80) - slide));
            }
        }
        const CFStringRef keys[] = {CFSTR("function-bb_on"), CFSTR("function-pmu_exton_config"), CFSTR("function-pmu_exton")};
        const char *names[] = {"bb_on", "pmu_exton_config", "pmu_exton"};
        for (unsigned i = 0; i < 3; ++i) {
            CFTypeRef v = IORegistryEntrySearchCFProperty(s, kIOServicePlane, keys[i], kCFAllocatorDefault,
                               kIORegistryIterateParents | kIORegistryIterateRecursively);
            if (v && CFGetTypeID(v) == CFDataGetTypeID() && CFDataGetLength(v) <= 64) {
                printf("{\"function\":\"%s\",\"descriptor\":\"", names[i]);
                const UInt8 *bytes = CFDataGetBytePtr(v);
                for (CFIndex j = 0; j < CFDataGetLength(v); ++j) printf("%02x", bytes[j]);
                puts("\"}");
            }
            if (v) CFRelease(v);
        }
    }
    int result=verified?0:6;
    if(argc==3 && !strcmp(argv[1],"hold")) {
        char *end=NULL;unsigned long seconds=strtoul(argv[2],&end,10);
        int (*write8)(uint64_t,uint8_t)=dlsym(jb,"kwrite8");
        if(!verified || !end || *end || seconds<1 || seconds>180)result=30;
        else result=native_power_hold(s,port,driver,slide,readptr,read32,read8,write8,(unsigned)seconds);
    } else if(argc==2 && (!strcmp(argv[1],"disable") || !strcmp(argv[1],"enable"))) {
        int (*write8)(uint64_t,uint8_t)=dlsym(jb,"kwrite8");
        result=verified ? native_power_hold(s,port,driver,slide,readptr,read32,read8,write8,!strcmp(argv[1],"disable")?0:181) : 6;
    } else if(argc>1) result=31;
    if(!hp_touched){IOServiceClose(port); IOObjectRelease(s);}
    return result;
}
