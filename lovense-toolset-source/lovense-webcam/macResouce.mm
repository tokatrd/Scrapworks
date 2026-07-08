#include "macResouce.h"
#import <Cocoa/Cocoa.h>
std::string GetBoundleResoucePath(const std::string &name){
    std::string fileName;
    NSString *tempPath =  [[NSBundle mainBundle] builtInPlugInsPath];
#if defined(__aarch64__) & defined(DEF_TOOLSETS)
    NSString *bundlePath = [tempPath stringByAppendingPathComponent:@"lovense-webcam-arm64.plugin"];
#else
    NSString *bundlePath = [tempPath stringByAppendingPathComponent:@"lovense-webcam.plugin"];
#endif
    NSBundle *bundle = [NSBundle bundleWithPath:bundlePath];
    
    NSString *bundleResPath = [bundle resourcePath];
    tempPath = [bundleResPath stringByAppendingPathComponent:[NSString stringWithUTF8String:name.c_str()]];
    return [tempPath UTF8String];
    NSString * fullName = [[NSBundle mainBundle] pathForResource:[tempPath stringByDeletingPathExtension] ofType:@"png"];
    if(fullName)
        return [fullName UTF8String];
    return fileName;
}

