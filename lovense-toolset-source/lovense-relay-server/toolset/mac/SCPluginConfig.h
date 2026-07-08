//
//  SCPluginConfig.h
//  SourceExample
//
//  Created by custom on 2021/7/18.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN



@interface SCPluginConfig : NSObject

@property (nonatomic,copy) NSString *pluginVerion;
@property (nonatomic,copy) NSString *htmlVersion;
@property (nonatomic,copy) NSString *localHtmlFile;
@property (nonatomic,copy) NSString *htmlUpdateUrl;
@property (nonatomic,copy) NSString *pluginUpdateUrl;

/*
 ~/Application Support/SplitCam/Plugins/data/lovense/cert
 ~/Application Support/SplitCam/Plugins/data/lovense/version.plist
 ~/Application Support/SplitCam/Plugins/data/lovense/resource/dist
 **/

+ (NSString *)GetLocalHtmlFilePath;
+ (NSString *)GetLocalConfigureFilePath;
+ (NSString *)GetPluginInstallDir;
+ (NSString *)GetPluginConfigureDir;


+ (NSString *)GetPluginVersion;
+ (NSString *)GetHtmlVersion;
+ (NSString *)GetHtmlUpdateUrl;
+ (NSString *)GetPluginUpdateUrl;
+ (NSString *)GetPcid;
+ (NSInteger)GetWSPort;
+ (NSInteger)GetWSSPort;
+ (NSString *)GetCrtFile;
+ (NSString *)GetPemFile;
+ (void)UpdateHtmlVersion:(NSString *)version;
+ (void)UpdatePluginVersion:(NSString *)version;
+ (NSString *)HtmlZipDownloadPath;
+ (NSString *)PluginPkgDownloadPath;

@end

NS_ASSUME_NONNULL_END
