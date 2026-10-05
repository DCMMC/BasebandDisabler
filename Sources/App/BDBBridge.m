#import "BDBBridge.h"
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
void BDBRequest(NSArray<NSString *> *arguments,void (^completion)(NSDictionary *)) {
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0),^{
        NSDictionary *result=@{@"ok":@NO,@"error":@"helper_unavailable"};
        int output[2];
        if(!pipe(output)){
            posix_spawn_file_actions_t actions;posix_spawn_file_actions_init(&actions);
            posix_spawn_file_actions_adddup2(&actions,output[1],STDOUT_FILENO);
            posix_spawn_file_actions_addclose(&actions,output[0]);
            posix_spawn_file_actions_addclose(&actions,output[1]);
            const char *helper="/var/jb/usr/libexec/basebanddisabler/basebandctl";
            char *argv[5]={(char *)helper,NULL,NULL,NULL,NULL};
            for(NSUInteger i=0;i<MIN(arguments.count,3);i++)argv[i+1]=(char *)arguments[i].UTF8String;
            pid_t pid=0;int error=posix_spawn(&pid,helper,&actions,NULL,argv,environ);
            posix_spawn_file_actions_destroy(&actions);close(output[1]);
            NSMutableData *data=[NSMutableData data];char buffer[4096];ssize_t count;
            while((count=read(output[0],buffer,sizeof(buffer)))>0){if(data.length<65536)[data appendBytes:buffer length:count];}
            close(output[0]);int status=0;if(!error)waitpid(pid,&status,0);
            if(!error){id object=[NSJSONSerialization JSONObjectWithData:data options:0 error:nil];if([object isKindOfClass:NSDictionary.class])result=object;}
        }
        dispatch_async(dispatch_get_main_queue(),^{completion(result);});
    });
}
