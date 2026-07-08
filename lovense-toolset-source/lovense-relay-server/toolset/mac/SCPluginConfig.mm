//
//  SCPluginConfig.m
//  SourceExample
//
//  Created by custom on 2021/7/18.
//

#import "SCPluginConfig.h"
#import <CommonCrypto/CommonDigest.h>
#include <string>

NSString * SCPLUGIN_VERSION_KEY  = @"PluginVersion";
NSString * SCPLUGIN_HTML_VERSION_KEY = @"DistVersion";
NSString * SCPLUGIN_HTML_UPDATE_URL_KEY = @"DistUpdateURL";
NSString * SCPLUGIN_UPDATE_URL_KEY = @"PluginUpdateURL";

const NSInteger WS_SOCKET_PORT = 11001;
const NSInteger WSS_SOCKET_PORT = 11003;

@implementation SCPluginConfig

+ (NSString *)GetPluginInstallDir
{
    NSString *installDir = @"~/Library/Application Support/obs-studio/";
    return [installDir stringByExpandingTildeInPath];
}

+ (NSString *)GetPluginConfigureDir
{
    NSString *dataDir = @"~/Library/Application Support/obs-studio/";
    NSFileManager *fm = [NSFileManager defaultManager];
    if(![fm fileExistsAtPath:dataDir])
    {
        [fm createDirectoryAtPath:[dataDir stringByExpandingTildeInPath] withIntermediateDirectories:YES attributes:nil error:nil];
    }
    return [dataDir stringByExpandingTildeInPath];
}

+ (NSString *)GetLocalHtmlFilePath
{
    NSString *file = [[[self class] GetPluginConfigureDir] stringByAppendingPathComponent:@"/dist/index.html"];
    NSFileManager *fm = [NSFileManager defaultManager];
    if([fm fileExistsAtPath:file])
        return file;
    return @"";
}

+ (NSString *)GetLocalConfigureFilePath
{
    return [[[self class] GetPluginConfigureDir] stringByAppendingPathComponent:@"/PluginsInfo.plist"];
}

+ (NSString *)GetPluginConfigValue:(NSString *)key
{
    NSString *config = [[self class] GetLocalConfigureFilePath];
    NSFileManager *fm = [NSFileManager defaultManager];
    if([fm fileExistsAtPath:config])
    {
        NSDictionary *dic = [NSDictionary dictionaryWithContentsOfFile:config];
        if(dic)
        {
            if([dic objectForKey:key])
            {
                return [dic valueForKey:key];
            }
        }
    }
    return @"2.1.4";
}

+ (void)SetPluginConfigValue:(NSString *)key value:(NSString *)value
{
    if(!key || !value)
        return;
    NSString *config = [[self class] GetLocalConfigureFilePath];
    NSFileManager *fm = [NSFileManager defaultManager];
    if([fm fileExistsAtPath:config])
    {
        NSMutableDictionary *dic = [NSMutableDictionary dictionaryWithContentsOfFile:config];
        if(dic)
        {
            if([dic objectForKey:key])
            {
                [dic setValue:value forKey:key];
                [dic writeToFile:config atomically:YES];
            }
        }
    }
    return;
}

+ (NSString *)GetPluginVersion
{
    return [[self class] GetPluginConfigValue:SCPLUGIN_VERSION_KEY];
}

+ (NSString *)GetHtmlVersion
{
    return [[self class] GetPluginConfigValue:SCPLUGIN_HTML_VERSION_KEY];
}

+ (NSString *)GetHtmlUpdateUrl
{
    NSString *url = [[self class] GetPluginConfigValue:SCPLUGIN_HTML_UPDATE_URL_KEY];
    if(url)
    {
        NSString *version = [[self class] GetHtmlVersion];
        if(version)
        {
            url = [url stringByAppendingString:version];
            return url;
        }
    }
    return @"";
}

+ (NSString *)GetPluginUpdateUrl
{
    NSString *url = [[self class] GetPluginConfigValue:SCPLUGIN_UPDATE_URL_KEY];
    if(url)
    {
        NSString *version = [[self class] GetPluginVersion];
        if(version)
        {
            url = [url stringByAppendingString:version];
            return url;
        }
    }
    return @"";
}

+ (NSString *)GetPcid
{
    char uuid[256] = {0};
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
                unsigned char result[CC_MD5_DIGEST_LENGTH];
                NSInteger len = UUID.length;
                unsigned char buf[16] = {0};
                CC_MD5( uuid, len, buf );
                
                const char HEX[16] =
                {
                    '0', '1', '2', '3',
                    '4', '5', '6', '7',
                    '8', '9', 'a', 'b',
                    'c', 'd', 'e', 'f'
                };
                std::string temp;
                temp.reserve( 16 << 1 );
                for( size_t i = 0; i < 16; i++ )
                {
                    int t = buf[i];
                    int a = t / 16;
                    int b = t % 16;
                    temp.append( 1, HEX[a] );
                    temp.append( 1, HEX[b] );
                }
                NSLog(@"UUID:%s",temp.c_str());
                return [NSString stringWithUTF8String:temp.c_str()];
            }
        }
        
    }
    NSLog(@"Get UUID failed!");
    return @"";;
}

+ (NSInteger)GetWSPort
{
    return WS_SOCKET_PORT;
}

+ (NSInteger)GetWSSPort
{
    return  WSS_SOCKET_PORT;;
}

+ (NSString *)GetCrtFile
{
    NSString *file = [[[self class] GetPluginConfigureDir] stringByAppendingPathComponent:@"/cert/star.lovense.club.all.crt"];
    NSFileManager *fm = [NSFileManager defaultManager];
    if([fm fileExistsAtPath:file])
        return file;
    return @"";
}

+ (NSString *)GetPemFile
{
    NSString *file = [[[self class] GetPluginConfigureDir] stringByAppendingPathComponent:@"/cert/STAR_lovense_club.pem"];
    NSFileManager *fm = [NSFileManager defaultManager];
    if([fm fileExistsAtPath:file])
        return file;
    return @"";
}


+ (void)UpdateHtmlVersion:(NSString *)version
{
    [[self class] SetPluginConfigValue:SCPLUGIN_HTML_VERSION_KEY value:version];
}

+ (void)UpdatePluginVersion:(NSString *)version
{
    [[self class] SetPluginConfigValue:SCPLUGIN_VERSION_KEY value:version];
}

+ (NSString *)HtmlZipDownloadPath
{
    NSString *zip = [[[self class] GetPluginConfigureDir] stringByAppendingPathComponent:@"/dist.zip"];
    NSFileManager *fm = [NSFileManager defaultManager];
    if([fm fileExistsAtPath:zip])
    {
        [fm removeItemAtPath:zip error:nil];
    }
    return zip;
}

+ (NSString *)PluginPkgDownloadPath
{
    NSString *pkg = [[[self class] GetPluginConfigureDir] stringByAppendingPathComponent:@"data/lovense/update.pkg"];
    NSFileManager *fm = [NSFileManager defaultManager];
    if([fm fileExistsAtPath:pkg])
    {
        [fm removeItemAtPath:pkg error:nil];
    }
    return pkg;
}

@end

std::string UntilGetPcid(){
    
    return [[SCPluginConfig GetPcid] UTF8String];
}

std::string UntilGetDistVersion(){
    
    return [[SCPluginConfig GetHtmlVersion] UTF8String];
}

std::string UntilGetCrtFile()
{
    return [[SCPluginConfig GetCrtFile] UTF8String];
}
std::string UntilGetPemFile()
{
    return [[SCPluginConfig GetPemFile] UTF8String];
}

std::string UntilGetToolsetVersion(){
    return [[SCPluginConfig GetPluginVersion] UTF8String];
}
