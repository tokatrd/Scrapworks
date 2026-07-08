//
//  UserDefine.h
//  obs-browser
//
//  Created by apple on 2019/5/27.
//

#ifndef UserDefine_h
#define UserDefine_h

#ifndef CHAR
    #define CHAR char
#endif

#ifndef WCHAR
    #define WCHAR wchar_t
#endif

typedef unsigned short  USHORT;
typedef wchar_t *       PCWSTR;
typedef unsigned long   ULONG;
typedef int             HANDLE;
typedef void *          PVOID;
typedef long long       DWORD;
typedef unsigned char   BYTE;
typedef char            TCHAR;
typedef int             HRESULT;

#define FALSE false
#define TRUE true
#define L

#define DEF_DIST_VERSION        "1.0.0"
#define DEF_OBS_PLUGIN_VERSION  "1.0.0"
#define DEF_DIST_FILEPATH       "/tmp/obs_DIST_File.zip"
#define DEF_PLUGIN_FILEPATH     "/tmp/obs_Plugin_File.zip"
#define DEF_UNZIP_DIR           "/tmp/obs_unzip"
#define DEF_UUID_FILE           "/tmp/uuid.txt"
#define  DISTHTMLBaseUrl        "https://apps.lovense.com/app/getUpdate/smartcam/win"
#define  Levese_PluginBaseUrl   "https://apps.lovense.com/app/getUpdate/obs/win"

#define DEF_PLUGINS_NAME       "Lovense Video Feedback"
#define DEF_PLUGINS_ID         "Lovese_browser_source"
#endif /* UserDefine_h */
