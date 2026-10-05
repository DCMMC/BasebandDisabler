#import <Foundation/Foundation.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <fcntl.h>
#include <unistd.h>

extern int BDNativeMain(int argc, char **argv);
static NSString *const State = @"/var/jb/var/basebanddisabler";
static NSString *sysString(const char *name) {
    char value[128]={0}; size_t size=sizeof(value);
    return !sysctlbyname(name,value,&size,NULL,0) ? @(value) : @"unknown";
}
static NSDictionary *config(void) {
    NSData *data=[NSData dataWithContentsOfFile:[State stringByAppendingPathComponent:@"config.json"]];
    id value=data ? [NSJSONSerialization JSONObjectWithData:data options:0 error:nil] : nil;
    return [value isKindOfClass:NSDictionary.class] && [value[@"auto_enabled"] isKindOfClass:NSNumber.class] && [value[@"version"] isEqual:@1] ? value : @{};
}
static BOOL saveAuto(BOOL enabled) {
    NSData *data=[NSJSONSerialization dataWithJSONObject:@{@"version":@1,@"auto_enabled":@(enabled)} options:0 error:nil];
    return [data writeToFile:[State stringByAppendingPathComponent:@"config.json"] options:NSDataWritingAtomic error:nil];
}
static NSArray *nativeRun(NSString *action,int *code) {
    NSString *path=[State stringByAppendingPathComponent:action.length ? @"operation.log" : @"status.log"];
    int fd=open(path.fileSystemRepresentation,O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW,0600);
    if(fd<0){*code=70;return @[];}
    fflush(stdout); int original=dup(STDOUT_FILENO);
    if(original<0 || dup2(fd,STDOUT_FILENO)<0){if(original>=0)close(original);close(fd);*code=70;return @[];}
    char *args[]={"basebandctl",(char *)action.UTF8String,NULL};
    *code=BDNativeMain(action.length?2:1,args);
    fflush(stdout); dup2(original,STDOUT_FILENO);close(original);close(fd);
    NSMutableArray *rows=[NSMutableArray array];
    NSString *text=[NSString stringWithContentsOfFile:path encoding:NSUTF8StringEncoding error:nil];
    for(NSString *line in [text componentsSeparatedByString:@"\n"]){
        NSData *data=[line dataUsingEncoding:NSUTF8StringEncoding];
        id object=data ? [NSJSONSerialization JSONObjectWithData:data options:0 error:nil] : nil;
        if([object isKindOfClass:NSDictionary.class])[rows addObject:object];
    }
    return rows;
}
static NSDictionary *find(NSArray *rows,NSString *key) {
    for(NSDictionary *row in rows)if(row[key])return row;
    return @{};
}
static BOOL supported(void) {
    return [sysString("hw.machine") isEqual:@"iPad13,6"] && [sysString("kern.osversion") isEqual:@"20D67"];
}
static NSDictionary *status(void) {
    int rc=0; NSArray *rows=supported()?nativeRun(@"",&rc):@[];
    NSDictionary *physical=find(rows,@"pmu_read"),*flags=find(rows,@"radio_not_found"),*gpio=find(rows,@"already_in_input_mode");
    BOOL verified=supported() && rc==0 && [find(rows,@"layout_verified")[@"layout_verified"] boolValue] && [gpio[@"gpio_provider_verified"] boolValue];
    BOOL off=verified && [physical[@"pmu_read"] boolValue] && ![physical[@"pmu_ext_on"] boolValue] && [physical[@"state"] unsignedIntValue]==1 && flags[@"radio_power_on_flag"] && ![flags[@"radio_power_on_flag"] boolValue] && [gpio[@"already_in_input_mode"] boolValue];
    BOOL missing=verified && [flags[@"radio_not_found"] unsignedIntValue]==1;
    BOOL on=verified && [physical[@"pmu_read"] boolValue] && [physical[@"pmu_ext_on"] boolValue] && [flags[@"radio_power_on_flag"] boolValue] && ![gpio[@"already_in_input_mode"] boolValue];
    return @{@"model":sysString("hw.machine"),@"build":sysString("kern.osversion"),@"supported":@(verified),@"profile_supported":@(supported()),@"hardware_off":@(off),@"powered_on":@(on),@"power_known":@((BOOL)(on||off)),@"failure_detected":@(missing),@"auto_enabled":@([config()[@"auto_enabled"] boolValue]),@"backend_exit":@(rc),@"version":@"0.1.1"};
}
static NSString *failure(int code) {
    switch(code){case 2:case 3:return @"dopamine_required";case 4:return @"driver_unavailable";case 5:case 6:case 20:case 21:case 22:case 23:case 24:case 25:case 33:return @"unsupported_layout";case 26:case 27:case 32:return @"unsafe_state";case 34:return @"snapshot_missing";case 29:return @"restore_failed";case 36:case 70:return @"storage_failed";default:return @"operation_failed";}
}
static int reply(BOOL ok,NSString *error,NSDictionary *value,int result) {
    NSDictionary *out=@{@"ok":@(ok),@"error":error?:@"",@"status":value?:@{},@"code":@(result)};
    NSData *data=[NSJSONSerialization dataWithJSONObject:out options:NSJSONWritingSortedKeys error:nil];
    fwrite(data.bytes,1,data.length,stdout);fputc('\n',stdout);return ok?0:1;
}
int main(int argc,char **argv) {@autoreleasepool {
    NSString *command=argc>1?@(argv[1]):@"status";
    BOOL rootCaller=getuid()==0;
    BOOL valid=(argc<=2 && [@[@"status",@"disable",@"restore",@"boot",@"migrate-legacy"] containsObject:command]) || (argc==3 && [command isEqual:@"auto"] && [@[@"on",@"off"] containsObject:@(argv[2])]);
    if(!valid)return reply(NO,@"invalid_command",nil,64);
    if((getuid()!=0 && getuid()!=501) || geteuid()!=0 || setgid(0) || setuid(0))return reply(NO,@"root_required",nil,77);
    if(([command isEqual:@"boot"] || [command isEqual:@"migrate-legacy"]) && !rootCaller)return reply(NO,@"root_required",nil,77);
    umask(077);
    if(mkdir(State.fileSystemRepresentation,0700) && errno!=EEXIST)return reply(NO,@"storage_failed",nil,70);
    struct stat directory;
    if(lstat(State.fileSystemRepresentation,&directory) || !S_ISDIR(directory.st_mode) || directory.st_uid || (directory.st_mode&077))return reply(NO,@"storage_failed",nil,70);
    int lock=open([State stringByAppendingPathComponent:@"control.lock"].fileSystemRepresentation,O_RDWR|O_CREAT|O_NOFOLLOW,0600);
    if(lock<0 || flock(lock,LOCK_EX|LOCK_NB))return reply(NO,@"busy",nil,75);
    if([command isEqual:@"status"]){NSDictionary *s=status();return reply(YES,nil,s,0);}
    if([command isEqual:@"boot"] && ![config()[@"auto_enabled"] boolValue])return reply(YES,nil,status(),0);
    if([command isEqual:@"auto"]){
        BOOL enabled=[@(argv[2]) isEqual:@"on"];NSDictionary *s=status();
        if(enabled && ![s[@"failure_detected"] boolValue])return reply(NO,@"unsafe_state",s,65);
        BOOL ok=saveAuto(enabled);return reply(ok,ok?nil:@"storage_failed",status(),ok?0:70);
    }
    if([command isEqual:@"restore"]){
        if(!saveAuto(NO))return reply(NO,@"storage_failed",status(),70);
        NSDictionary *s=status();
        NSData *data=[NSData dataWithContentsOfFile:[State stringByAppendingPathComponent:@"original-state.json"]];
        id snapshot=data?[NSJSONSerialization JSONObjectWithData:data options:0 error:nil]:nil;
        BOOL thisBoot=[snapshot isKindOfClass:NSDictionary.class] && [snapshot[@"boot"] isEqual:sysString("kern.bootsessionuuid")];
        if([s[@"powered_on"] boolValue] || (!thisBoot && ![s[@"hardware_off"] boolValue]))return reply(YES,nil,s,0);
    }
    if(!supported())return reply(NO,@"unsupported_device",status(),65);
    if([command isEqual:@"migrate-legacy"]){
        NSString *destination=[State stringByAppendingPathComponent:@"original-state.json"];
        if(![NSFileManager.defaultManager fileExistsAtPath:destination]){
            int fd=open("/var/jb/var/baseband-control/original-state.json",O_RDONLY|O_NOFOLLOW);
            struct stat st;
            if(fd<0 || fstat(fd,&st) || !S_ISREG(st.st_mode) || st.st_uid || (st.st_mode&022) || st.st_size>4096){if(fd>=0)close(fd);return reply(NO,@"snapshot_missing",status(),34);}
            char data[4096];ssize_t n=read(fd,data,sizeof(data));close(fd);
            if(n<=0 || ![[NSData dataWithBytes:data length:n] writeToFile:destination options:NSDataWritingAtomic error:nil])return reply(NO,@"storage_failed",status(),70);
        }
        BOOL ok=saveAuto(YES);return reply(ok,ok?nil:@"storage_failed",status(),ok?0:70);
    }
    int rc=0;nativeRun([command isEqual:@"restore"]?@"enable":@"disable",&rc);
    if(rc==0 && [command isEqual:@"disable"] && !saveAuto(YES))rc=70;
    NSDictionary *s=status();
    BOOL correct=[command isEqual:@"restore"] ? ![s[@"hardware_off"] boolValue] : [s[@"hardware_off"] boolValue];
    return reply(rc==0 && correct,rc?failure(rc):(correct?nil:@"verification_failed"),s,rc);
}}
