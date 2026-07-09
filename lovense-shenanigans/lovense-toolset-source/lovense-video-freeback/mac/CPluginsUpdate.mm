//
//  CPluginsUpdate.cpp
//  obs-browser
//
//  Created by apple on 2019/5/30.
//
#import <Cocoa/Cocoa.h>
#import <AppKit/AppKit.h>

#include <thread>
#include "CPluginsUpdate.hpp"
#include "UserDefine.h"

#define DEF_VISION_FILE @"PluginsInfo.plist"

#define DEF_DIST_VESION_KEY     @"DistVersion"
#define DEF_PLUGIN_VESION_KEY   @"PluginVersion"

#define DEF_JSONKEY_HSAUPDATE   @"hasUpdate"
#define DEF_JSONKEY_URL         @"url"
#define DEF_JSONKEY_VERSION     @"version"
#define DEF_FIRST_RUN           @"fristRun"

bool path_file_exit(const char *path)
{
    if(!path || strlen(path) == 0)
        return false;
    NSString *strPath = [NSString stringWithUTF8String:path];
    NSFileManager *fm = [NSFileManager defaultManager];
    return [fm fileExistsAtPath:strPath];
}

string getFrameworkspath(const char *path)
{
    if(!path || strlen(path) == 0)
        return "";
    NSString *string = [NSString stringWithUTF8String:path];
    string = [string stringByDeletingLastPathComponent];
    string = [string stringByDeletingLastPathComponent];
    return [string UTF8String];
}


string getDistVersion()
{
    NSString * appsuport = @"~/Library/Application Support/obs-studio/";
    appsuport = [appsuport stringByStandardizingPath];
    appsuport = [appsuport stringByAppendingPathComponent:@"PluginsInfo.plist"];
    NSFileManager *fm = [NSFileManager defaultManager];
    if(![fm fileExistsAtPath:appsuport])
        return "1.6.0";
    NSDictionary *dic = [NSDictionary dictionaryWithContentsOfFile:appsuport];
    if(dic)
    {
        NSString *version = [dic valueForKey:@"DistVersion"];
        if(version)
            return [version UTF8String];
    }
    return "1.6.0";
}

string getPluginVersion()
{
    NSString * appsuport = @"~/Library/Application Support/obs-studio/";
    appsuport = [appsuport stringByStandardizingPath];
    appsuport = [appsuport stringByAppendingPathComponent:@"PluginsInfo.plist"];
    NSFileManager *fm = [NSFileManager defaultManager];
    if(![fm fileExistsAtPath:appsuport])
        return "1.6.0";
    NSDictionary *dic = [NSDictionary dictionaryWithContentsOfFile:appsuport];
    if(dic)
    {
        NSString *version = [dic valueForKey:@"PluginVersion"];
        if(version)
            return [version UTF8String];
    }
    return "1.7.0";
}

int getUUID(char uuid[256])
{
    NSTask *task;
    task = [[NSTask alloc] init];
    [task setLaunchPath: @"/usr/sbin/ioreg"];
    
    //ioreg -rd1 -c IOPlatformExpertDevice | grep -E '(UUID)'
    
    NSArray *arguments;
    arguments = [NSArray arrayWithObjects: @"-rd1", @"-c",@"IOPlatformExpertDevice",nil];
    [task setArguments: arguments];
    
    NSPipe *pipe;
    pipe = [NSPipe pipe];
    [task setStandardOutput: pipe];
    
    NSFileHandle *file;
    file = [pipe fileHandleForReading];
    
    [task launch];
    
    NSData *data;
    data = [file readDataToEndOfFile];
    if(data)
    {
        NSString *string;
        string = [[NSString alloc] initWithData: data encoding: NSUTF8StringEncoding];
        
        //NSLog (@"grep returned:n%@", string);
        
        NSString *key = @"IOPlatformUUID";
        NSRange range = [string rangeOfString:key];
        if(range.location != NSNotFound)
        {
            NSInteger location = range.location + [key length] + 5;
            NSInteger length = 32 + 4;
            range.location = location;
            range.length = length;
            
            NSString *UUID = [string substringWithRange:range];
            NSLog(@"UUID = %@",UUID);

            if(UUID)
            {
                memset(uuid, 0, sizeof(uuid));
                memcpy(uuid, [UUID UTF8String], UUID.length);
                return UUID.length;
            }
        }
        
    }
    NSLog(@"Get UUID failed!");
    return 0;
}

int popSaveService(const char * message)
{
    __block int ret = 0;
    if([NSThread currentThread] == [NSThread mainThread])
    {
        NSString *mainBundle = [[NSBundle mainBundle] bundlePath];
        mainBundle = [mainBundle stringByDeletingLastPathComponent];
        NSAlert *alert = [[NSAlert alloc] init];
        alert.icon = [[NSImage alloc] initWithContentsOfFile:[mainBundle stringByAppendingPathComponent:@"OBS.icns"]];
        [alert addButtonWithTitle:@"YES"];
        [alert addButtonWithTitle:@"NO"];
        alert.messageText = [NSString stringWithUTF8String:message];
        ret = [alert runModal];
        if(ret == 1000)
            ret = 1;
    }
    else
    {
        dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
        
        dispatch_async(dispatch_get_main_queue(), ^{
            NSString *mainBundle = [[NSBundle mainBundle] bundlePath];
            mainBundle = [mainBundle stringByDeletingLastPathComponent];
            NSAlert *alert = [[NSAlert alloc] init];
            alert.icon = [[NSImage alloc] initWithContentsOfFile:[mainBundle stringByAppendingPathComponent:@"OBS.icns"]];
            [alert addButtonWithTitle:@"YES"];
            [alert addButtonWithTitle:@"NO"];
            alert.messageText = [NSString stringWithUTF8String:message];
            ret = [alert runModal];
            if(ret == 1000)
                ret = 1;
            dispatch_semaphore_signal(semaphore);
        });
        dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
    }
    return ret;
}

bool shouldAddSourceToScene(const char *configPath)
{
    NSString *cfgPath = [NSString stringWithUTF8String:configPath];
    
    NSString *obsConfigPath = cfgPath;
    NSFileManager *fm = [NSFileManager defaultManager];
    NSMutableDictionary * dicBrowseInfo = nil;
    NSString *browsePath = [obsConfigPath stringByAppendingPathComponent:@"PluginsInfo.plist"];
    if([fm fileExistsAtPath:browsePath])
    {
        dicBrowseInfo = [NSMutableDictionary dictionaryWithContentsOfFile:browsePath];
        if(dicBrowseInfo)
        {
            if([dicBrowseInfo valueForKey:DEF_FIRST_RUN])
            {
                BOOL isFrist = [[dicBrowseInfo valueForKey:DEF_FIRST_RUN] boolValue];
                if(!isFrist)
                {
                    return false;
                }
            }
            return true;
        }
    }
    return false;
}

void setupSourceHaveAdded(const char *configPath)
{
    if(!configPath || strlen(configPath) == 0)
        return;
    NSString *cfgPath = [NSString stringWithUTF8String:configPath];
    
    NSString *obsConfigPath = cfgPath;
    NSFileManager *fm = [NSFileManager defaultManager];
    NSMutableDictionary * dicBrowseInfo = nil;
    NSString *browsePath = [obsConfigPath stringByAppendingPathComponent:@"PluginsInfo.plist"];
    if([fm fileExistsAtPath:browsePath])
    {
        dicBrowseInfo = [NSMutableDictionary dictionaryWithContentsOfFile:browsePath];
        if(dicBrowseInfo)
        {
            [dicBrowseInfo setObject:@NO forKey:DEF_FIRST_RUN];
            [dicBrowseInfo writeToFile:browsePath atomically:YES];
        }
    }
}
