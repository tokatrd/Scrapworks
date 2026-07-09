#include <objbase.h>

#include <obs-module.h>
#include <obs.hpp>
#include <util/dstr.hpp>
#include <util/platform.h>
#include <util/windows/WinHandle.hpp>
#include <util/threading.h>
#include "libdshowcapture/dshowcapture.hpp"
#include "ffmpeg-decode.h"
#include "encode-dstr.hpp"

#include <algorithm>
#include <limits>
#include <set>
#include <string>
#include <vector>
#include "json11/json11.hpp"

#include <obs-frontend-api.h>
#include <util/config-file.h>
#include "PixelFormatConverter.h"

#include <iostream> 
//#include <opencv2/core/core.hpp> //  included in most hpp automatically
#include <opencv2/calib3d.hpp> //solvePnP()
#include <opencv2/highgui/highgui.hpp>  //imread()
#include <opencv2/imgproc.hpp> //circle()
//#include <opencv2/opencv.hpp> //contains all the modules 
#include "DynamicModuleLoader.h"
#include "OBSVideoAdapter.h"
using namespace zs;
/*
 *
 *   - handle disconnections and reconnections
 *   - if device not present, wait for device to be plugged in
 */

#undef min
#undef max

using namespace std;
using namespace DShow;
using namespace cv;
using namespace DynamicModuleLoaderSpace;

extern void OnAIDetectData(const std::string& json);
extern bool ShouldDetectFace();
extern void OnAIDetectData(const std::string& json);
extern void AiSourceDidAddToScene(bool isCreate);
extern void AiSourceDidRemoveToScene();
/* clang-format off */

/* settings defines that will cause errors if there are typos */
#define VIDEO_DEVICE_ID   "video_device_id"
#define RES_TYPE          "res_type"
#define RESOLUTION        "resolution"
#define FRAME_INTERVAL    "frame_interval"
#define VIDEO_FORMAT      "video_format"
#define LAST_VIDEO_DEV_ID "last_video_device_id"
#define LAST_RESOLUTION   "last_resolution"
#define BUFFERING_VAL     "buffering"
#define FLIP_IMAGE        "flip_vertically"
#define AUDIO_OUTPUT_MODE "audio_output_mode"
#define USE_CUSTOM_AUDIO  "use_custom_audio_device"
#define AUDIO_DEVICE_ID   "audio_device_id"
#define COLOR_SPACE       "color_space"
#define COLOR_RANGE       "color_range"
#define DEACTIVATE_WNS    "deactivate_when_not_showing"

#define TEXT_INPUT_NAME     obs_module_text("VideoCaptureDevice")
#define TEXT_DEVICE         obs_module_text("Device")
#define TEXT_CONFIG_VIDEO   obs_module_text("ConfigureVideo")
#define TEXT_CONFIG_XBAR    obs_module_text("ConfigureCrossbar")
#define TEXT_RES_FPS_TYPE   obs_module_text("ResFPSType")
#define TEXT_CUSTOM_RES     obs_module_text("ResFPSType.Custom")
#define TEXT_PREFERRED_RES  obs_module_text("ResFPSType.DevPreferred")
#define TEXT_FPS_MATCHING   obs_module_text("FPS.Matching")
#define TEXT_FPS_HIGHEST    obs_module_text("FPS.Highest")
#define TEXT_RESOLUTION     obs_module_text("Resolution")
#define TEXT_VIDEO_FORMAT   obs_module_text("VideoFormat")
#define TEXT_FORMAT_UNKNOWN obs_module_text("VideoFormat.Unknown")
#define TEXT_BUFFERING      obs_module_text("Buffering")
#define TEXT_BUFFERING_AUTO obs_module_text("Buffering.AutoDetect")
#define TEXT_BUFFERING_ON   obs_module_text("Buffering.Enable")
#define TEXT_BUFFERING_OFF  obs_module_text("Buffering.Disable")
#define TEXT_FLIP_IMAGE     obs_module_text("FlipVertically")
#define TEXT_AUDIO_MODE     obs_module_text("AudioOutputMode")
#define TEXT_MODE_CAPTURE   obs_module_text("AudioOutputMode.Capture")
#define TEXT_MODE_DSOUND    obs_module_text("AudioOutputMode.DirectSound")
#define TEXT_MODE_WAVEOUT   obs_module_text("AudioOutputMode.WaveOut")
#define TEXT_CUSTOM_AUDIO   obs_module_text("UseCustomAudioDevice")
#define TEXT_AUDIO_DEVICE   obs_module_text("AudioDevice")
#define TEXT_ACTIVATE       obs_module_text("Activate")
#define TEXT_DEACTIVATE     obs_module_text("Deactivate")
#define TEXT_COLOR_SPACE    obs_module_text("ColorSpace")
#define TEXT_COLOR_DEFAULT  obs_module_text("ColorSpace.Default")
#define TEXT_COLOR_RANGE    obs_module_text("ColorRange")
#define TEXT_RANGE_DEFAULT  obs_module_text("ColorRange.Default")
#define TEXT_RANGE_PARTIAL  obs_module_text("ColorRange.Partial")
#define TEXT_RANGE_FULL     obs_module_text("ColorRange.Full")
#define TEXT_DWNS           obs_module_text("DeactivateWhenNotShowing")

#define MIN2(a, b) ((a) < (b) ? (a) : (b))
#define MAX2(a, b) ((a) > (b) ? (a) : (b))
#define CLIP3(x, a, b) MIN2(MAX2(a,x), b)

/* clang-format on */
#define MAX_SW_RES_INT (1920 * 1080)
static int g_dshow_source_count = 0;

enum ResType {
	ResType_Preferred,
	ResType_Custom,
};

enum class BufferingType : int64_t {
	Auto,
	On,
	Off,
};

class FacePoint {
public:
	int x = 0;
	int y = 0;
};

class Eyes {
public:
	FacePoint right;
	FacePoint left;
};

class Mouth {
public:
	FacePoint right;
	FacePoint left;
};
struct PoseInfo {
	float x;
	float y;
};
//keep same as face_tracking_detect.cpp
struct FaceCalcResult {
	//int faceEulerAngles[3] = { 1000,1000,1000 };
	std::vector<vector<PoseInfo>> faceLandmarks;
};
//class FaceDetectData {
//public:
//	float score = 0.0f;
//	Bbox bBox;
//	Eyes eyes;
//	FacePoint nose;
//	Mouth mouth;
//	int roll = 0;
//	int yaw = 0;
//	int pitch = 0;
//};
class FaceDetectData2 {
public: //all info belong to one face
	int yaw = 1000, pitch = 1000, roll = 1000; //initiate to no-detected values by default, ie. 1000
	FacePoint rightSpecialEffectPoint; //direction is based on image other than self face
	FacePoint leftSpecialEffectPoint;
	FacePoint lm4;	//pattern: lm+index in 3d face model
	FacePoint lm50;
	FacePoint lm280;
	FacePoint lm61;
	FacePoint lm291;
	FacePoint lm130, lm155, lm362, lm359;

	// eyes, mouth, tip,...
};

void ffmpeg_log(void* bla, int level, const char* msg, va_list args)
{
	DStr str;
	if (level == AV_LOG_WARNING) {
		dstr_copy(str, "warning: ");
	}
	else if (level == AV_LOG_ERROR) {
		/* only print first of this message to avoid spam */
		static bool suppress_app_field_spam = false;
		if (strcmp(msg, "unable to decode APP fields: %s\n") == 0) {
			if (suppress_app_field_spam)
				return;

			suppress_app_field_spam = true;
		}

		dstr_copy(str, "error:   ");
	}
	else if (level < AV_LOG_ERROR) {
		dstr_copy(str, "fatal:   ");
	}
	else {
		return;
	}

	dstr_cat(str, msg);
	if (dstr_end(str) == '\n')
		dstr_resize(str, str->len - 1);

	blogva(LOG_WARNING, str, args);
	av_log_default_callback(bla, level, msg, args);
}

class Decoder {
	struct ffmpeg_decode decode;

public:
	inline Decoder() { memset(&decode, 0, sizeof(decode)); }
	inline ~Decoder() { ffmpeg_decode_free(&decode); }

	inline operator ffmpeg_decode* () { return &decode; }
	inline ffmpeg_decode* operator->() { return &decode; }
};

class CriticalSection {
	CRITICAL_SECTION mutex;

public:
	inline CriticalSection() { InitializeCriticalSection(&mutex); }
	inline ~CriticalSection() { DeleteCriticalSection(&mutex); }

	inline operator CRITICAL_SECTION* () { return &mutex; }
};

class CriticalScope {
	CriticalSection& mutex;

	CriticalScope() = delete;
	CriticalScope& operator=(CriticalScope& cs) = delete;

public:
	inline CriticalScope(CriticalSection& mutex_) : mutex(mutex_)
	{
		EnterCriticalSection(mutex);
	}

	inline ~CriticalScope() { LeaveCriticalSection(mutex); }
};

enum class Action {
	None,
	Activate,
	ActivateBlock,
	Deactivate,
	Shutdown,
	ConfigVideo,
	ConfigAudio,
	ConfigCrossbar1,
	ConfigCrossbar2,
};

static DWORD CALLBACK DShowThread(LPVOID ptr);

struct DShowInput {
	obs_source_t* source;
	OBSDevice device;
	bool deactivateWhenNotShowing = false;
	bool deviceHasAudio = false;
	bool deviceHasSeparateAudioFilter = false;
	bool flip = false;
	bool active = false;

	Decoder audio_decoder;
	Decoder video_decoder;

	VideoConfig videoConfig;
	AudioConfig audioConfig;

	video_range_type range;
	obs_source_frame2 frame;
	obs_source_audio audio;
	long lastRotation = 0;

	WinHandle semaphore;
	WinHandle activated_event;
	WinHandle thread;
	CriticalSection mutex;
	vector<Action> actions;

	inline void QueueAction(Action action)
	{
		CriticalScope scope(mutex);
		actions.push_back(action);
		ReleaseSemaphore(semaphore, 1, nullptr);
	}

	inline void QueueActivate(obs_data_t* settings)
	{
		bool block =
			obs_data_get_bool(settings, "synchronous_activate");
		QueueAction(block ? Action::ActivateBlock : Action::Activate);
		if (block) {
			obs_data_erase(settings, "synchronous_activate");
			WaitForSingleObject(activated_event, INFINITE);
		}
	}

	inline DShowInput(obs_source_t* source_, obs_data_t* settings)
		: source(source_), device(InitGraph::False)
	{
		memset(&audio, 0, sizeof(audio));
		memset(&frame, 0, sizeof(frame));

		av_log_set_level(AV_LOG_WARNING);
		av_log_set_callback(ffmpeg_log);

		semaphore = CreateSemaphore(nullptr, 0, 0x7FFFFFFF, nullptr);
		if (!semaphore)
			throw "Failed to create semaphore";

		activated_event = CreateEvent(nullptr, false, false, nullptr);
		if (!activated_event)
			throw "Failed to create activated_event";

		thread =
			CreateThread(nullptr, 0, DShowThread, this, 0, nullptr);
		if (!thread)
			throw "Failed to create thread";

		deactivateWhenNotShowing =
			obs_data_get_bool(settings, DEACTIVATE_WNS);

		if (obs_data_get_bool(settings, "active")) {
			bool showing = obs_source_showing(source);
			if (!deactivateWhenNotShowing || showing)
				QueueActivate(settings);

			active = true;
		}
	}

	inline ~DShowInput()
	{
		{
			CriticalScope scope(mutex);
			actions.resize(1);
			actions[0] = Action::Shutdown;
		}

		ReleaseSemaphore(semaphore, 1, nullptr);

		WaitForSingleObject(thread, INFINITE);
	}

	void OnEncodedVideoData(enum AVCodecID id, unsigned char* data,
		size_t size, long long ts);
	void OnEncodedAudioData(enum AVCodecID id, unsigned char* data,
		size_t size, long long ts);

	void OnVideoData(const VideoConfig& config, unsigned char* data,
		size_t size, long long startTime, long long endTime,
		long rotation);

	bool OnDetectFaceWithFrame(const VideoConfig& videoConfig, unsigned char* data, const size_t &size, const int &fWidth,const int &fHeight, std::vector <std::shared_ptr<FaceDetectData2>> &result);
	/*int YUV2RGB(void *pYUV, void *pRGB, int width, int height, bool alphaYUV,
		bool alphaRGB);*/
		/*int RGB2YUV(void *pRGB, void *pYUV, int width, int height, bool alphaYUV,
			bool alphaRGB);*/
	void OnAudioData(const AudioConfig& config, unsigned char* data,
		size_t size, long long startTime, long long endTime);

	bool UpdateVideoConfig(obs_data_t* settings);
	bool UpdateAudioConfig(obs_data_t* settings);
	void SetActive(bool active);
	inline enum video_colorspace GetColorSpace(obs_data_t* settings) const;
	inline enum video_range_type GetColorRange(obs_data_t* settings) const;
	inline bool Activate(obs_data_t* settings);
	inline void Deactivate();

	inline void SetupBuffering(obs_data_t* settings);

	void DShowLoop();
};

static DWORD CALLBACK DShowThread(LPVOID ptr)
{
	DShowInput* dshowInput = (DShowInput*)ptr;

	os_set_thread_name("win-dshow: DShowThread");

	CoInitialize(nullptr);
	dshowInput->DShowLoop();
	CoUninitialize();
	return 0;
}

static inline void ProcessMessages()
{
	MSG msg;
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

/* Always keep directshow in a single thread for a given device */
void DShowInput::DShowLoop()
{
	while (true) {
		DWORD ret = MsgWaitForMultipleObjects(1, &semaphore, false,
			INFINITE, QS_ALLINPUT);
		if (ret == (WAIT_OBJECT_0 + 1)) {
			ProcessMessages();
			continue;
		}
		else if (ret != WAIT_OBJECT_0) {
			break;
		}

		Action action = Action::None;
		{
			CriticalScope scope(mutex);
			if (actions.size()) {
				action = actions.front();
				actions.erase(actions.begin());
			}
		}

		switch (action) {
		case Action::Activate:
		case Action::ActivateBlock: {
			bool block = action == Action::ActivateBlock;

			obs_data_t* settings;
			settings = obs_source_get_settings(source);
			if (!Activate(settings)) {
				obs_source_output_video2(source, nullptr);
			}
			if (block)
				SetEvent(activated_event);
			obs_data_release(settings);
			break;
		}

		case Action::Deactivate:
			Deactivate();
			break;

		case Action::Shutdown:
			device.ShutdownGraph();
			return;

		case Action::ConfigVideo:
			device.OpenDialog(nullptr, DialogType::ConfigVideo);
			break;

		case Action::ConfigAudio:
			device.OpenDialog(nullptr, DialogType::ConfigAudio);
			break;

		case Action::ConfigCrossbar1:
			device.OpenDialog(nullptr, DialogType::ConfigCrossbar);
			break;

		case Action::ConfigCrossbar2:
			device.OpenDialog(nullptr, DialogType::ConfigCrossbar2);
			break;

		case Action::None:;
		}
	}
}

#define FPS_HIGHEST 0LL
#define FPS_MATCHING -1LL

template<typename T, typename U, typename V>
static bool between(T&& lower, U&& value, V&& upper)
{
	return value >= lower && value <= upper;
}

static bool ResolutionAvailable(const VideoInfo& cap, int cx, int cy)
{
	return between(cap.minCX, cx, cap.maxCX) &&
		between(cap.minCY, cy, cap.maxCY);
}

#define DEVICE_INTERVAL_DIFF_LIMIT 20

static bool FrameRateAvailable(const VideoInfo& cap, long long interval)
{
	return interval == FPS_HIGHEST || interval == FPS_MATCHING ||
		between(cap.minInterval - DEVICE_INTERVAL_DIFF_LIMIT, interval,
			cap.maxInterval + DEVICE_INTERVAL_DIFF_LIMIT);
}

static long long FrameRateInterval(const VideoInfo& cap,
	long long desired_interval)
{
	return desired_interval < cap.minInterval
		? cap.minInterval
		: min(desired_interval, cap.maxInterval);
}

static inline video_format ConvertVideoFormat(VideoFormat format)
{
	switch (format) {
	case VideoFormat::ARGB:
		return VIDEO_FORMAT_BGRA;
	case VideoFormat::XRGB:
		return VIDEO_FORMAT_BGRX;
	case VideoFormat::I420:
		return VIDEO_FORMAT_I420;
	case VideoFormat::YV12:
		return VIDEO_FORMAT_I420;
	case VideoFormat::NV12:
		return VIDEO_FORMAT_NV12;
	case VideoFormat::Y800:
		return VIDEO_FORMAT_Y800;
	case VideoFormat::YVYU:
		return VIDEO_FORMAT_YVYU;
	case VideoFormat::YUY2:
		return VIDEO_FORMAT_YUY2;
	case VideoFormat::UYVY:
		return VIDEO_FORMAT_UYVY;
	case VideoFormat::HDYC:
		return VIDEO_FORMAT_UYVY;
	default:
		return VIDEO_FORMAT_NONE;
	}
}

static inline audio_format ConvertAudioFormat(AudioFormat format)
{
	switch (format) {
	case AudioFormat::Wave16bit:
		return AUDIO_FORMAT_16BIT;
	case AudioFormat::WaveFloat:
		return AUDIO_FORMAT_FLOAT;
	default:
		return AUDIO_FORMAT_UNKNOWN;
	}
}

static inline enum speaker_layout convert_speaker_layout(uint8_t channels)
{
	switch (channels) {
	case 0:
		return SPEAKERS_UNKNOWN;
	case 1:
		return SPEAKERS_MONO;
	case 2:
		return SPEAKERS_STEREO;
	case 3:
		return SPEAKERS_2POINT1;
	case 4:
		return SPEAKERS_4POINT0;
	case 5:
		return SPEAKERS_4POINT1;
	case 6:
		return SPEAKERS_5POINT1;
	case 8:
		return SPEAKERS_7POINT1;
	default:
		return SPEAKERS_UNKNOWN;
	}
}

#include <memory>
#include <string>
#include <vector>
#include <iostream>

#define USE_SHELL_OPEN
#ifndef nullptr
#define nullptr 0
#endif
#if defined(_MSC_VER)
//#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#else

#include <unistd.h>

#endif
//ref:https://github.com/nothings/stb
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION

#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "stb_image_write.h"
#include <stdint.h>
#include "timing.h"

#ifndef _MAX_DRIVE
#define _MAX_DRIVE 3
#endif
#ifndef _MAX_FNAME
#define _MAX_FNAME 256
#endif
#ifndef _MAX_EXT
#define _MAX_EXT 256
#endif
#ifndef _MAX_DIR
#define _MAX_DIR 256
#endif


typedef int (*Func_Mediapipe_Face_Tracking_Init)(const char *model_path);
//typedef int (*Func_Mediapipe_Face_Tracking_Detect_Frame)(int image_index, int image_width, int image_height, void* image_data);
typedef int (*Func_Mediapipe_Face_Tracking_Detect_Frame_Direct)(
	int image_width, int image_height, void *image_data,
	FaceCalcResult &gesture_result);
//typedef int (*Func_Mediapipe_Face_Tracking_Detect_Video)(const char* video_path, int show_image);
typedef int (*Func_Mediapipe_Face_Tracking_Release)();

Func_Mediapipe_Face_Tracking_Init Mediapipe_Face_Tracking_Init = nullptr;
//Func_Mediapipe_Face_Tracking_Detect_Frame Mediapipe_Face_Tracking_Detect_Frame = nullptr;
Func_Mediapipe_Face_Tracking_Detect_Frame_Direct
	Mediapipe_Face_Tracking_Detect_Frame_Direct = nullptr;
//Func_Mediapipe_Face_Tracking_Detect_Video Mediapipe_Face_Tracking_Detect_Video = nullptr;
Func_Mediapipe_Face_Tracking_Release Mediapipe_Face_Tracking_Release = nullptr;

DynamicModuleLoader dllLoader;

//initiate once before call detect(). return 1 in case success, else other
static std::string GetPluginPath()
{
	std::string model_path = obs_get_module_binary_path(obs_current_module());
	model_path = model_path.substr(0, model_path.find_last_of('/'));
	return model_path;
}

int init(string dllPath, string pbtxtPath)
{

	std::string plugin_path =  GetPluginPath();
	std::string dll_path = plugin_path;
#ifdef _DEBUG
	dll_path +="/Face_Tracking_dlld.dll"; //must place it into obs-studio/bin/64bit
#else
	dll_path += "/Face_Tracking_dll.dll"; //must place it into obs-studio/bin/64bit
#endif


	if (dllLoader.IsFileExist(dll_path)) {

		if (dllLoader.LoadDynamicModule(dll_path)) {
			void *p_Mediapipe_Face_Tracking_Init =
				dllLoader.GetFunction(
					"Mediapipe_Face_Tracking_Init");
			if (p_Mediapipe_Face_Tracking_Init != nullptr) {
				Mediapipe_Face_Tracking_Init =
					(Func_Mediapipe_Face_Tracking_Init(
						p_Mediapipe_Face_Tracking_Init));
				if (Mediapipe_Face_Tracking_Init != nullptr) {
				}
			} else {
			}

			void *p_Mediapipe_Face_Tracking_Detect_Frame_Direct =
				dllLoader.GetFunction(
					"Mediapipe_Face_Tracking_Detect_Frame_Direct");
			if (p_Mediapipe_Face_Tracking_Detect_Frame_Direct !=
			    nullptr) {
				Mediapipe_Face_Tracking_Detect_Frame_Direct =
					(Func_Mediapipe_Face_Tracking_Detect_Frame_Direct)(p_Mediapipe_Face_Tracking_Detect_Frame_Direct);
				if (Mediapipe_Face_Tracking_Detect_Frame_Direct !=
				    nullptr) {
				}
			} else {
			}

			void *p_Mediapipe_Face_Tracking_Release =
				dllLoader.GetFunction(
					"Mediapipe_Face_Tracking_Release");
			if (p_Mediapipe_Face_Tracking_Release != nullptr) {
				Mediapipe_Face_Tracking_Release =
					(Func_Mediapipe_Face_Tracking_Release(
						p_Mediapipe_Face_Tracking_Release));
				if (Mediapipe_Face_Tracking_Release !=
				    nullptr) {
				}
			} else {
			}

		} else {
			return -1;
		}
	} else {
		std::cout << "dll nonexists!" << std::endl;
		return -1;
	}

	std::string mediapipe_face_tracking_model_path = plugin_path;
	mediapipe_face_tracking_model_path +="/face_mesh_desktop_live.pbtxt"; //must place it into obs-studio/bin/64bit


	if (Mediapipe_Face_Tracking_Init(mediapipe_face_tracking_model_path.c_str())) {
		std::cout << "** initiated model success." << std::endl;
	} else {
		std::cout << "failed to initiate model." << std::endl;
		return -1;
	}
	return 1;
}

int LoadModel() {
	// Loading model files fails in non-main thread
	blog(LOG_INFO, "Start initiating detect model...");
	int rst = init("", ""); 
	if (rst == 1) {
		blog(LOG_INFO, " initiated detect model success!");
		return 1;
	}
	else {
		blog(LOG_ERROR, " failed to initiate detect model !");
		return -1;
	}
}

/* called when use camera collector */
//the process memory used in running is about 219M in Lam in Debug/Prod mode in case drawBoxes = true, flipImage = true, resolution 720p
float detect_cost_ms = 0.;
float pure_detect_cost_ms = 0;
int face_handled = 0;
float preprocess_cost_ms = 0.;
float function_cost_ms = 0;
float yuyv2rgb_cost_ms = 0;
long frame_num = 0;	//start from 0
bool done_once_tasks = false;
bool detect_face = true;
bool write_faces = false;
const bool SMOOTH_ANGLES_LANDMARKS = true;// true; // whether smoothen all the landmarks used to calculate angles
const bool SMOOTH_ANGLES = false; //smooth angles, false in v1 since the UI use their roll only instead of here
const bool flipImage = false;// flip vertically,cost about 7ms in Lam pc for prod mode, so consider enable this IFF the speed is fast enough!!
const bool READ_FROM_FILE = false;// true;	//todo:set to false in prod; read from file or camera
#ifdef _DEBUG
bool drawBboxes = true;
#else
bool drawBboxes = false;
#endif
//max: 250, min:30 for 1080 x 1920;
const int max_detect_spatial = 250;//250 1280;// 0; //DIFFERENT VALUES WILL NOT REMARKABLY CHNAGE THE ANGELS
int miniFace = 30; //40 by default; NOTE: 30 is best fit for 350
const int DetectInterval = 1;//2; //1; // frames interval to detect.1 by default. the larger the faster,but less responses to downstream application
#ifdef _DEBUG
const int LogFrameInterval = 2;// 2;
#else
const int LogFrameInterval = 10;
#endif
bool enableLogPerFrame = true; //how many frames elapsed to output once detection info
int faces_frame_detected = 0;
#define PI 3.141592653589793

//copied from:https://www.programcreek.com/cpp/?code=PolygonTek%2FBlueshiftEngine%2FBlueshiftEngine-master%2FSource%2FRuntime%2FPrivate%2FImage%2FImageResize.cpp
//template <typename T>
static void ResizeImageNearest(const unsigned char* src, int srcWidth, int srcHeight,
	unsigned char* dst, int dstWidth, int dstHeight,
	int numComponents)
{
	int srcPitch = srcWidth * numComponents;

	float ratioX = (float)srcWidth / dstWidth;
	float ratioY = (float)srcHeight / dstHeight;

	for (int y = 0; y < dstHeight; y++) {
		int iY = y * ratioY;

		int offsetY = iY * srcPitch;

		for (int x = 0; x < dstWidth; x++) {
			int iX = x * ratioX;

			int offsetX = iX * numComponents;

			const unsigned char* srcPtrY = &src[offsetY];

			for (int i = 0; i < numComponents; i++) {
				*dst++ = srcPtrY[offsetX + i];
			}
		}
	}
}

static int YUV2RGB(void* pYUV, void* pRGB, int width, int height,bool alphaYUV, bool alphaRGB)
{
	int y;
	int cr;
	int cb;

	double r;
	double g;
	double b;
	unsigned char* pYUVData = (unsigned char*)pYUV;
	unsigned char* pRGBData = (unsigned char*)pRGB;

	for (int i = 0, j = 0; i < width * height * 3; i += 6, j += 4) {
		//first pixel
		y = *(pYUVData + j);
		cb = *(pYUVData + j + 1);
		cr = *(pYUVData + j + 3);

		r = y + (1.4065 * (cr - 128));
		g = y - (0.3455 * (cb - 128)) - (0.7169 * (cr - 128));
		b = y + (1.7790 * (cb - 128));

		//This prevents colour distortions in your rgb image
		if (r < 0)
			r = 0;
		else if (r > 255)
			r = 255;
		if (g < 0)
			g = 0;
		else if (g > 255)
			g = 255;
		if (b < 0)
			b = 0;
		else if (b > 255)
			b = 255;

		*(pRGBData + i) = (unsigned char)b;
		*(pRGBData + i + 1) = (unsigned char)g;
		*(pRGBData + i + 2) = (unsigned char)r;

		//second pixel
		y = *(pYUVData + j + 2);
		cb = *(pYUVData + j + 1);
		cr = *(pYUVData + j + 3);

		r = y + (1.4065 * (cr - 128));
		g = y - (0.3455 * (cb - 128)) - (0.7169 * (cr - 128));
		b = y + (1.7790 * (cb - 128));

		if (r < 0)
			r = 0;
		else if (r > 255)
			r = 255;
		if (g < 0)
			g = 0;
		else if (g > 255)
			g = 255;
		if (b < 0)
			b = 0;
		else if (b > 255)
			b = 255;

		*(pRGBData + i + 3) = (unsigned char)b;
		*(pRGBData + i + 4) = (unsigned char)g;
		*(pRGBData + i + 5) = (unsigned char)r;
	}
	return 0;
}

//RGB2YUV
//pRGB			point to the RGB data
//pYUV			point to the YUV data
//width		width of the picture
//height		height of the picture
//alphaYUV		is there an alpha channel in YUV
//alphaRGB		is there an alpha channel in RGB
static int RGB2YUV(void* pRGB, void* pYUV, int width, int height, bool alphaYUV, bool alphaRGB)
{
	if (NULL == pRGB) {
		return -1;
	}
	unsigned char* pRGBData = (unsigned char*)pRGB;
	unsigned char* pYUVData = (unsigned char*)pYUV;
	if (NULL == pYUVData) {
		if (alphaYUV) {
			pYUVData = new unsigned char[width * height * 3];
		}
		else
			pYUVData = new unsigned char[width * height * 2];
	}
	int R1, G1, B1, R2, G2, B2, Y1, U1, Y2, V1;
	int alpha1, alpha2;
	if (alphaYUV) {
		if (alphaRGB) {
			for (int i = 0; i < height; ++i) {
				for (int j = 0; j < width / 2; ++j) {
					B1 = *(pRGBData +
						(height - i - 1) * width * 4 +
						j * 8);
					G1 = *(pRGBData +
						(height - i - 1) * width * 4 +
						j * 8 + 1);
					R1 = *(pRGBData +
						(height - i - 1) * width * 4 +
						j * 8 + 2);
					alpha1 =
						*(pRGBData +
							(height - i - 1) * width * 4 +
							j * 8 + 3);
					B2 = *(pRGBData +
						(height - i - 1) * width * 4 +
						j * 8 + 4);
					G2 = *(pRGBData +
						(height - i - 1) * width * 4 +
						j * 8 + 5);
					R2 = *(pRGBData +
						(height - i - 1) * width * 4 +
						j * 8 + 6);
					alpha2 =
						*(pRGBData +
							(height - i - 1) * width * 4 +
							j * 8 + 7);
					Y1 = (((66 * R1 + 129 * G1 + 25 * B1 +
						128) >>
						8) +
						16) > 255
						? 255
						: (((66 * R1 + 129 * G1 +
							25 * B1 + 128) >>
							8) +
							16);
					U1 = ((((-38 * R1 - 74 * G1 + 112 * B1 +
						128) >>
						8) +
						((-38 * R2 - 74 * G2 + 112 * B2 +
							128) >>
							8)) / 2 +
						128) > 255
						? 255
						: ((((-38 * R1 - 74 * G1 +
							112 * B1 + 128) >>
							8) +
							((-38 * R2 - 74 * G2 +
								112 * B2 + 128) >>
								8)) / 2 +
							128);
					Y2 = (((66 * R2 + 129 * G2 + 25 * B2 +
						128) >>
						8) +
						16) > 255
						? 255
						: ((66 * R2 + 129 * G2 +
							25 * B2 + 128) >>
							8) + 16;
					V1 = ((((112 * R1 - 94 * G1 - 18 * B1 +
						128) >>
						8) +
						((112 * R2 - 94 * G2 - 18 * B2 +
							128) >>
							8)) / 2 +
						128) > 255
						? 255
						: ((((112 * R1 - 94 * G1 -
							18 * B1 + 128) >>
							8) +
							((112 * R2 - 94 * G2 -
								18 * B2 + 128) >>
								8)) / 2 +
							128);
					*(pYUVData + i * width * 3 + j * 6) =
						Y1;
					*(pYUVData + i * width * 3 + j * 6 +
						1) = U1;
					*(pYUVData + i * width * 3 + j * 6 +
						2) = Y2;
					*(pYUVData + i * width * 3 + j * 6 +
						3) = V1;
					*(pYUVData + i * width * 3 + j * 6 +
						4) = alpha1;
					*(pYUVData + i * width * 3 + j * 6 +
						5) = alpha2;
				}
			}
		}
		else {
			unsigned char alpha = 255;
			for (int i = 0; i < height; ++i) {
				for (int j = 0; j < width / 2; ++j) {
					B1 = *(pRGBData +
						(height - i - 1) * width * 3 +
						j * 6);
					G1 = *(pRGBData +
						(height - i - 1) * width * 3 +
						j * 6 + 1);
					R1 = *(pRGBData +
						(height - i - 1) * width * 3 +
						j * 6 + 2);
					B2 = *(pRGBData +
						(height - i - 1) * width * 3 +
						j * 6 + 3);
					G2 = *(pRGBData +
						(height - i - 1) * width * 3 +
						j * 6 + 4);
					R2 = *(pRGBData +
						(height - i - 1) * width * 3 +
						j * 6 + 5);
					Y1 = ((66 * R1 + 129 * G1 + 25 * B1 +
						128) >>
						8) +
						16;
					U1 = ((-38 * R1 - 74 * G1 + 112 * B1 +
						128) >>
						8 + (-38 * R2 - 74 * G2 +
							112 * B2 + 128) >>
						8) / 2 +
						128;
					Y2 = ((66 * R2 + 129 * G2 + 25 * B2 +
						128) >>
						8) +
						16;
					V1 = ((112 * R1 - 94 * G1 - 18 * B1 +
						128) >>
						8 + (112 * R2 - 94 * G2 -
							18 * B2 + 128) >>
						8) / 2 +
						128;
					Y1 = (((66 * R1 + 129 * G1 + 25 * B1 +
						128) >>
						8) +
						16) > 255
						? 255
						: (((66 * R1 + 129 * G1 +
							25 * B1 + 128) >>
							8) +
							16);
					U1 = ((((-38 * R1 - 74 * G1 + 112 * B1 +
						128) >>
						8) +
						((-38 * R2 - 74 * G2 + 112 * B2 +
							128) >>
							8)) / 2 +
						128) > 255
						? 255
						: ((((-38 * R1 - 74 * G1 +
							112 * B1 + 128) >>
							8) +
							((-38 * R2 - 74 * G2 +
								112 * B2 + 128) >>
								8)) / 2 +
							128);
					Y2 = (((66 * R2 + 129 * G2 + 25 * B2 +
						128) >>
						8) +
						16) > 255
						? 255
						: ((66 * R2 + 129 * G2 +
							25 * B2 + 128) >>
							8) + 16;
					V1 = ((((112 * R1 - 94 * G1 - 18 * B1 +
						128) >>
						8) +
						((112 * R2 - 94 * G2 - 18 * B2 +
							128) >>
							8)) / 2 +
						128) > 255
						? 255
						: ((((112 * R1 - 94 * G1 -
							18 * B1 + 128) >>
							8) +
							((112 * R2 - 94 * G2 -
								18 * B2 + 128) >>
								8)) / 2 +
							128);
					*(pYUVData + i * width * 3 + j * 6) =
						Y1;
					*(pYUVData + i * width * 3 + j * 6 +
						1) = U1;
					*(pYUVData + i * width * 3 + j * 6 +
						2) = Y2;
					*(pYUVData + i * width * 3 + j * 6 +
						3) = V1;
					*(pYUVData + i * width * 3 + j * 6 +
						4) = alpha;
					*(pYUVData + i * width * 3 + j * 6 +
						5) = alpha;
				}
			}
		}
	}
	else {
		if (alphaRGB) {
			for (int i = 0; i < height; ++i) {
				for (int j = 0; j < width / 2; ++j) {
					B1 = *(pRGBData +
						(height - i - 1) * width * 4 +
						j * 8);
					G1 = *(pRGBData +
						(height - i - 1) * width * 4 +
						j * 8 + 1);
					R1 = *(pRGBData +
						(height - i - 1) * width * 4 +
						j * 8 + 2);
					B2 = *(pRGBData +
						(height - i - 1) * width * 4 +
						j * 8 + 4);
					G2 = *(pRGBData +
						(height - i - 1) * width * 4 +
						j * 8 + 5);
					R2 = *(pRGBData +
						(height - i - 1) * width * 4 +
						j * 8 + 6);
					Y1 = (((66 * R1 + 129 * G1 + 25 * B1 +
						128) >>
						8) +
						16) > 255
						? 255
						: (((66 * R1 + 129 * G1 +
							25 * B1 + 128) >>
							8) +
							16);
					U1 = ((((-38 * R1 - 74 * G1 + 112 * B1 +
						128) >>
						8) +
						((-38 * R2 - 74 * G2 + 112 * B2 +
							128) >>
							8)) / 2 +
						128) > 255
						? 255
						: ((((-38 * R1 - 74 * G1 +
							112 * B1 + 128) >>
							8) +
							((-38 * R2 - 74 * G2 +
								112 * B2 + 128) >>
								8)) / 2 +
							128);
					Y2 = (((66 * R2 + 129 * G2 + 25 * B2 +
						128) >>
						8) +
						16) > 255
						? 255
						: ((66 * R2 + 129 * G2 +
							25 * B2 + 128) >>
							8) + 16;
					V1 = ((((112 * R1 - 94 * G1 - 18 * B1 +
						128) >>
						8) +
						((112 * R2 - 94 * G2 - 18 * B2 +
							128) >>
							8)) / 2 +
						128) > 255
						? 255
						: ((((112 * R1 - 94 * G1 -
							18 * B1 + 128) >>
							8) +
							((112 * R2 - 94 * G2 -
								18 * B2 + 128) >>
								8)) / 2 +
							128);
					*(pYUVData + i * width * 2 + j * 4) =
						Y1;
					*(pYUVData + i * width * 2 + j * 4 +
						1) = U1;
					*(pYUVData + i * width * 2 + j * 4 +
						2) = Y2;
					*(pYUVData + i * width * 2 + j * 4 +
						3) = V1;
				}
			}
		}
		else {
			for (int i = height - 1; i >= 0; --i) {
				for (int j = 0; j < width / 2; ++j) {
					B1 = *(pRGBData +
						(height - i - 1) * width * 3 +
						j * 6);
					G1 = *(pRGBData +
						(height - i - 1) * width * 3 +
						j * 6 + 1);
					R1 = *(pRGBData +
						(height - i - 1) * width * 3 +
						j * 6 + 2);
					B2 = *(pRGBData +
						(height - i - 1) * width * 3 +
						j * 6 + 3);
					G2 = *(pRGBData +
						(height - i - 1) * width * 3 +
						j * 6 + 4);
					R2 = *(pRGBData +
						(height - i - 1) * width * 3 +
						j * 6 + 5);

					Y1 = (((66 * R1 + 129 * G1 + 25 * B1 +
						128) >>
						8) +
						16) > 255
						? 255
						: (((66 * R1 + 129 * G1 +
							25 * B1 + 128) >>
							8) +
							16);
					U1 = ((((-38 * R1 - 74 * G1 + 112 * B1 +
						128) >>
						8) +
						((-38 * R2 - 74 * G2 + 112 * B2 +
							128) >>
							8)) / 2 +
						128) > 255
						? 255
						: ((((-38 * R1 - 74 * G1 +
							112 * B1 + 128) >>
							8) +
							((-38 * R2 - 74 * G2 +
								112 * B2 + 128) >>
								8)) / 2 +
							128);
					Y2 = (((66 * R2 + 129 * G2 + 25 * B2 +
						128) >>
						8) +
						16) > 255
						? 255
						: ((66 * R2 + 129 * G2 +
							25 * B2 + 128) >>
							8) + 16;
					V1 = ((((112 * R1 - 94 * G1 - 18 * B1 +
						128) >>
						8) +
						((112 * R2 - 94 * G2 - 18 * B2 +
							128) >>
							8)) / 2 +
						128) > 255
						? 255
						: ((((112 * R1 - 94 * G1 -
							18 * B1 + 128) >>
							8) +
							((112 * R2 - 94 * G2 -
								18 * B2 + 128) >>
								8)) / 2 +
							128);
					*(pYUVData + (height - i - 1) * width * 2 + j * 4) =
						Y1;
					*(pYUVData + (height - i - 1) * width * 2 + j * 4 +
						1) = U1;
					*(pYUVData + (height - i - 1) * width * 2 + j * 4 +
						2) = Y2;
					*(pYUVData + (height - i - 1) * width * 2 + j * 4 +
						3) = V1;
				}
			}
		}
	}
	return 0;
}

static void drawPoint(unsigned char* bits, int width, int depth, int x, int y, const uint8_t* color)
{
	for (int i = 0; i < min(depth, 3); ++i) {
		bits[(y * width + x) * depth + i] = color[i];
		bits[(y * width + x + 1) * depth + i] = color[i]; // bold the point, Lam
		/*if (bolden){
			bits[(y * width + x - 1) * depth + i] = color[i];
		}*/
	}
}

static void drawLine(unsigned char* bits, int width, int depth, int startX, int startY, int endX, int endY, const uint8_t* col)
{
	if (endX == startX) {
		if (startY > endY) {
			int a = startY;
			startY = endY;
			endY = a;
		}
		for (int y = startY; y <= endY; y++) {
			drawPoint(bits, width, depth, startX, y, col);
		}
	}
	else {
		float m = 1.0f * (endY - startY) / (endX - startX);
		int y = 0;
		if (startX > endX) {
			int a = startX;
			startX = endX;
			endX = a;
		}
		for (int x = startX; x <= endX; x++) {
			y = (int)(m * (x - startX) + startY);
			drawPoint(bits, width, depth, x, y, col);
		}
	}
}

static void drawRectangle(unsigned char* bits, int width, int depth, int x1, int y1, int x2, int y2, const uint8_t* col)
{
	drawLine(bits, width, depth, x1, y1, x2, y1, col);
	drawLine(bits, width, depth, x2, y1, x2, y2, col);
	drawLine(bits, width, depth, x2, y2, x1, y2, col);
	drawLine(bits, width, depth, x1, y2, x1, y1, col);
}

static int convert_yuv_to_rgb_pixel(int y, int u, int v)
{
	unsigned int pixel32 = 0;
	unsigned char* pixel = (unsigned char*)&pixel32;
	int r, g, b;
	r = y + (1.370705 * (v - 128));
	g = y - (0.698001 * (v - 128)) - (0.337633 * (u - 128));
	b = y + (1.732446 * (u - 128));
	if (r > 255) r = 255;
	if (g > 255) g = 255;
	if (b > 255) b = 255;
	if (r < 0) r = 0;
	if (g < 0) g = 0;
	if (b < 0) b = 0;
	pixel[0] = r;
	pixel[1] = g;
	pixel[2] = b;
	return pixel32;
}
/*transform planar I422 to rgb. https://blog.csdn.net/weixin_33898233/article/details/86102656 */
static int convert_yuv_to_rgb_buffer(unsigned char* yuv, unsigned char* rgb, unsigned int width, unsigned int height)
{
	unsigned int in, out = 0;
	unsigned int pixel_16;
	unsigned char pixel_24[3];
	unsigned int pixel32;
	int y0, u, y1, v;

	for (in = 0; in < width * height * 2; in += 4)
	{
		pixel_16 =
			yuv[in + 3] << 24 |
			yuv[in + 2] << 16 |
			yuv[in + 1] << 8 |
			yuv[in + 0];
		y0 = (pixel_16 & 0x000000ff);
		u = (pixel_16 & 0x0000ff00) >> 8;
		y1 = (pixel_16 & 0x00ff0000) >> 16;
		v = (pixel_16 & 0xff000000) >> 24;
		pixel32 = convert_yuv_to_rgb_pixel(y0, u, v);

		pixel_24[0] = (pixel32 & 0x000000ff);
		pixel_24[1] = (pixel32 & 0x0000ff00) >> 8;
		pixel_24[2] = (pixel32 & 0x00ff0000) >> 16;
		rgb[out++] = pixel_24[0];
		rgb[out++] = pixel_24[1];
		rgb[out++] = pixel_24[2];
		pixel32 = convert_yuv_to_rgb_pixel(y1, u, v);

		pixel_24[0] = (pixel32 & 0x000000ff);
		pixel_24[1] = (pixel32 & 0x0000ff00) >> 8;
		pixel_24[2] = (pixel32 & 0x00ff0000) >> 16;
		rgb[out++] = pixel_24[0];
		rgb[out++] = pixel_24[1];
		rgb[out++] = pixel_24[2];
	}
	return 0;
}

void DShowInput::OnEncodedVideoData(enum AVCodecID id, unsigned char* data,
	size_t size, long long ts)
{
	/* If format changes, free and allow it to recreate the decoder */
	if (ffmpeg_decode_valid(video_decoder) &&
		video_decoder->codec->id != id) {
		ffmpeg_decode_free(video_decoder);
	}

	if (!ffmpeg_decode_valid(video_decoder)) {
		/* Only use MJPEG hardware decoding on resolutions higher
		 * than 1920x1080.  The reason why is because we want to strike
		 * a reasonable balance between hardware and CPU usage. */
		bool useHW = videoConfig.format != VideoFormat::MJPEG ||
			(videoConfig.cx * videoConfig.cy_abs) >
			MAX_SW_RES_INT;
		if (ffmpeg_decode_init(video_decoder, id, useHW) < 0) {
			blog(LOG_WARNING, "Could not initialize video decoder");
			return;
		}
	}

	bool got_output;
	bool success = ffmpeg_decode_video(video_decoder, data, size, &ts,
		range, &frame, &got_output);
	if (!success) {
		blog(LOG_WARNING, "Error decoding video");
		return;
	}

	if (got_output) {
		frame.timestamp = (uint64_t)ts * 100;
		if (flip)
			frame.flip = !frame.flip;
#if LOG_ENCODED_VIDEO_TS
		blog(LOG_DEBUG, "video ts: %llu", frame.timestamp);
#endif
		// TODO: Try https://blog.csdn.net/runner668/article/details/80459712
		// Or convert mjpeg to rgb instead of converting to I422
		//detectVideo(source,&frame, false); //added by Lam
		obs_source_output_video2(source, &frame);
	}
}


static void ResetFaceData(std::vector<std::shared_ptr<FaceDetectData2>>& vData, int width, int height)
{
	config_t* basicConfig = obs_frontend_get_profile_config();
	if (basicConfig && width != 0 && height != 0)
	{
		int base_width = (uint32_t)config_get_uint(basicConfig, "Video", "BaseCX");
		int base_height = (uint32_t)config_get_uint(basicConfig, "Video", "BaseCY");
		float width_scale = base_width / (float)width;
		float height_scale = base_height / (float)height;
		for (auto item : vData)
		{
			//v2
			item->leftSpecialEffectPoint.x = item->leftSpecialEffectPoint.x * width_scale;
			item->leftSpecialEffectPoint.y = item->leftSpecialEffectPoint.y * height_scale;
			item->rightSpecialEffectPoint.x = item->rightSpecialEffectPoint.x * width_scale;
			item->rightSpecialEffectPoint.y = item->rightSpecialEffectPoint.y * height_scale;

			item->lm130.x *= width_scale;
			item->lm130.y *= height_scale;
			item->lm155.x *= width_scale;
			item->lm155.y *= height_scale;
			item->lm280.x *= width_scale;
			item->lm280.y *= height_scale;
			item->lm291.x *= width_scale;
			item->lm291.y *= height_scale;
			item->lm359.x *= width_scale;
			item->lm359.y *= height_scale;
			item->lm362.x *= width_scale;
			item->lm362.y *= height_scale;
			item->lm4.x *= width_scale;
			item->lm4.y *= height_scale;
			item->lm50.x *= width_scale;
			item->lm50.y *= height_scale;
			item->lm61.x *= width_scale;
			item->lm61.y *= height_scale;

			//v1
			// Scale proportionally or scale equidistantly?
			//item->bBox.x1 = item->bBox.x1 * width_scale;
			//item->bBox.y1 = item->bBox.y1 * height_scale;
			//item->bBox.x2 = item->bBox.x2 * width_scale;
			//item->bBox.y2 = item->bBox.y2 * height_scale;

			//item->eyes.left.x *= width_scale;
			//item->eyes.left.y *= height_scale;

			//item->eyes.right.x *= width_scale;
			//item->eyes.right.y *= height_scale;

			//item->mouth.left.x *= width_scale;
			//item->mouth.left.y *= height_scale;

			//item->mouth.right.x *= width_scale;
			//item->mouth.right.y *= height_scale;

			//item->nose.x *= width_scale;
			//item->nose.y *= height_scale;
		}
	}
}

/*
{"function", "InvokeOBSAICameraHandle"},
											{"command", cmd},
*/
static void FaceDataToJson(std::vector<std::shared_ptr<FaceDetectData2>>& vData, json11::Json& root)
{
	json11::Json::object rootObject;
	rootObject["type"] = "109";
	json11::Json::object jsonData = json11::Json::object{
											{"function", "InvokeOBSAICameraHandle"},
											{"command", "AIDetectData"},
											{"version", "0.1.2"},
	};
	json11::Json::array dataArray;
	for (const auto &item : vData)
	{
		int roll = 0;
		int yaw = 0;
		int pitch = 0;
		json11::Json::object arrayItem;

		arrayItem["roll"] = item->roll;
		arrayItem["yaw"] = item->yaw;
		arrayItem["pitch"] = item->pitch;

		//v2
		arrayItem["headSpecialEffectPoints"] = json11::Json(json11::Json::object{
			{"left_se_x", item->leftSpecialEffectPoint.x},
			{"left_se_y", item->leftSpecialEffectPoint.y},
			{"right_se_x", item->rightSpecialEffectPoint.x},
			{"right_se_y", item->rightSpecialEffectPoint.y}

			,{"lm4_se_x", item->lm4.x}
			,{"lm4_se_y", item->lm4.y}
			,{"lm50_se_x", item->lm50.x}
			,{"lm50_se_y", item->lm50.y}
			,{"lm280_se_x", item->lm280.x}
			,{"lm280_se_y", item->lm280.y}
			,{"lm61_se_x", item->lm61.x}
			,{"lm61_se_y", item->lm61.y}
			,{"lm291_se_x", item->lm291.x}
			,{"lm291_se_y", item->lm291.y}

			,{"lm130_se_x", item->lm130.x}
			,{"lm130_se_y", item->lm130.y}
			,{"lm155_se_x", item->lm155.x}
			,{"lm155_se_y", item->lm155.y}
			,{"lm359_se_x", item->lm359.x}
			,{"lm359_se_y", item->lm359.y}
			,{"lm362_se_x", item->lm362.x}
			,{"lm362_se_y", item->lm362.y}
		});

		//v1
		/*arrayItem["score"] = item->score;
		arrayItem["facebox"] = json11::Json(json11::Json::object{
			{"left_top_x", item->bBox.x1},
			{"left_top_y", item->bBox.y1},
			{"right_bottom_x", item->bBox.x2},
			{"right_bottom_y", item->bBox.y2}
			});

		arrayItem["faceeye"] = json11::Json(json11::Json::object{
			{"left_eye_x", item->eyes.left.x},
			{"left_eye_y", item->eyes.left.y},
			{"right_eye_x", item->eyes.right.x},
			{"right_eye_y", item->eyes.right.y}
			});

		arrayItem["facenose"] = json11::Json(json11::Json::object{
			{"x", item->nose.x},
			{"y", item->nose.y}
			});

		arrayItem["facemouth"] = json11::Json(json11::Json::object{
			{"left_mouth_x", item->mouth.left.x},
			{"left_mouth_y", item->mouth.left.y},
			{"right_mouth_x", item->mouth.right.x},
			{"right_mouth_y", item->mouth.right.y}
			});*/

		dataArray.push_back(arrayItem);
	}
	jsonData["result"] = dataArray;
	rootObject["data"] = jsonData;
	root = rootObject;
}



void handleDetectResult(std::vector<std::shared_ptr<FaceDetectData2>>& vData, int width, int height) {
	if (enableLogPerFrame) {
		ResetFaceData(vData, width, height);
		
		json11::Json root;
		if(g_dshow_source_count > 0)
			FaceDataToJson(vData, root);
		else
		{
			// If camera is removed, send empty data once to clear face effects
			std::vector<std::shared_ptr<FaceDetectData2>> vEmpty;
			FaceDataToJson(vEmpty, root);
		}
		OnAIDetectData(root.dump());
	}
}

std::string SendhandleEmptyDetectResult()
{
	json11::Json root;
	std::vector<std::shared_ptr<FaceDetectData2>> vEmpty;
	FaceDataToJson(vEmpty, root);
	return root.dump();
}



static void I420ToRGB(unsigned char * src, unsigned char *dest,int width, int height) {
	
#define B 2
#define G 1
#define R 0
	int numOfPixel = width * height;
	int positionOfV = numOfPixel;
	int positionOfU = numOfPixel / 4 + numOfPixel;
	dest = new unsigned char[numOfPixel * 3];

	for (int i = 0; i < height; i++) {
		int startY = i * width;
		int step = (i / 2) * (width / 2);
		int startV = positionOfV + step;
		int startU = positionOfU + step;
		for (int j = 0; j < width; j++) {
			int Y = startY + j;
			int V = startV + j / 2;
			int U = startU + j / 2;
			int index = Y * 3;
			dest[index + B] = (int)((src[Y] & 0xff) + 1.4075 * ((src[V] & 0xff) - 128));
			dest[index + G] = (int)((src[Y] & 0xff) - 0.3455 * ((src[U] & 0xff) - 128) - 0.7169 * ((src[V] & 0xff) - 128));
			dest[index + R] = (int)((src[Y] & 0xff) + 1.779 * ((src[U] & 0xff) - 128));
		}
	}
}

void ncnn_resize_bilinear_c2(const unsigned char* src, int srcw, int srch, int srcstride, unsigned char* dst, int w, int h, int stride)
{
	const int INTER_RESIZE_COEF_BITS = 11;
	const int INTER_RESIZE_COEF_SCALE = 1 << INTER_RESIZE_COEF_BITS;
	//     const int ONE=INTER_RESIZE_COEF_SCALE;

	double scale_x = (double)srcw / w;
	double scale_y = (double)srch / h;

	int* buf = new int[w + h + w + h];

	int* xofs = buf;     //new int[w];
	int* yofs = buf + w; //new int[h];

	short* ialpha = (short*)(buf + w + h);    //new short[w * 2];
	short* ibeta = (short*)(buf + w + h + w); //new short[h * 2];

	float fx;
	float fy;
	int sx;
	int sy;

#define SATURATE_CAST_SHORT(X) (short)::std::min(::std::max((int)(X + (X >= 0.f ? 0.5f : -0.5f)), SHRT_MIN), SHRT_MAX);

	for (int dx = 0; dx < w; dx++)
	{
		fx = (float)((dx + 0.5) * scale_x - 0.5);
		sx = static_cast<int>(floor(fx));
		fx -= sx;

		if (sx < 0)
		{
			sx = 0;
			fx = 0.f;
		}
		if (sx >= srcw - 1)
		{
			sx = srcw - 2;
			fx = 1.f;
		}

		xofs[dx] = sx * 2;

		float a0 = (1.f - fx) * INTER_RESIZE_COEF_SCALE;
		float a1 = fx * INTER_RESIZE_COEF_SCALE;

		ialpha[dx * 2] = SATURATE_CAST_SHORT(a0);
		ialpha[dx * 2 + 1] = SATURATE_CAST_SHORT(a1);
	}

	for (int dy = 0; dy < h; dy++)
	{
		fy = (float)((dy + 0.5) * scale_y - 0.5);
		sy = static_cast<int>(floor(fy));
		fy -= sy;

		if (sy < 0)
		{
			sy = 0;
			fy = 0.f;
		}
		if (sy >= srch - 1)
		{
			sy = srch - 2;
			fy = 1.f;
		}

		yofs[dy] = sy;

		float b0 = (1.f - fy) * INTER_RESIZE_COEF_SCALE;
		float b1 = fy * INTER_RESIZE_COEF_SCALE;

		ibeta[dy * 2] = SATURATE_CAST_SHORT(b0);
		ibeta[dy * 2 + 1] = SATURATE_CAST_SHORT(b1);
	}

#undef SATURATE_CAST_SHORT

	// loop body
	Mat rowsbuf0(w * 2 + 2, (size_t)2u, CV_8UC1);
	Mat rowsbuf1(w * 2 + 2, (size_t)2u, CV_8UC1);
	short* rows0 = (short*)rowsbuf0.data;
	short* rows1 = (short*)rowsbuf1.data;

	int prev_sy1 = -2;

	for (int dy = 0; dy < h; dy++)
	{
		sy = yofs[dy];

		if (sy == prev_sy1)
		{
			// reuse all rows
		}
		else if (sy == prev_sy1 + 1)
		{
			// hresize one row
			short* rows0_old = rows0;
			rows0 = rows1;
			rows1 = rows0_old;
			const unsigned char* S1 = src + srcstride * (sy + 1);

			const short* ialphap = ialpha;
			short* rows1p = rows1;
			for (int dx = 0; dx < w; dx++)
			{
				sx = xofs[dx];

				const unsigned char* S1p = S1 + sx;
#if __ARM_NEON
				int16x4_t _a0a1XX = vld1_s16(ialphap);
				int16x4_t _a0a0a1a1 = vzip_s16(_a0a1XX, _a0a1XX).val[0];
				uint8x8_t _S1 = uint8x8_t();

				_S1 = vld1_lane_u8(S1p, _S1, 0);
				_S1 = vld1_lane_u8(S1p + 1, _S1, 1);
				_S1 = vld1_lane_u8(S1p + 2, _S1, 2);
				_S1 = vld1_lane_u8(S1p + 3, _S1, 3);

				int16x8_t _S116 = vreinterpretq_s16_u16(vmovl_u8(_S1));
				int16x4_t _S1lowhigh = vget_low_s16(_S116);
				int32x4_t _S1ma0a1 = vmull_s16(_S1lowhigh, _a0a0a1a1);
				int32x2_t _rows1low = vadd_s32(vget_low_s32(_S1ma0a1), vget_high_s32(_S1ma0a1));
				int32x4_t _rows1 = vcombine_s32(_rows1low, vget_high_s32(_S1ma0a1));
				int16x4_t _rows1_sr4 = vshrn_n_s32(_rows1, 4);
				vst1_s16(rows1p, _rows1_sr4);
#else
				short a0 = ialphap[0];
				short a1 = ialphap[1];

				rows1p[0] = (S1p[0] * a0 + S1p[2] * a1) >> 4;
				rows1p[1] = (S1p[1] * a0 + S1p[3] * a1) >> 4;
#endif // __ARM_NEON

				ialphap += 2;
				rows1p += 2;
			}
		}
		else
		{
			// hresize two rows
			const unsigned char* S0 = src + srcstride * (sy);
			const unsigned char* S1 = src + srcstride * (sy + 1);

			const short* ialphap = ialpha;
			short* rows0p = rows0;
			short* rows1p = rows1;
			for (int dx = 0; dx < w; dx++)
			{
				sx = xofs[dx];
				short a0 = ialphap[0];
				short a1 = ialphap[1];

				const unsigned char* S0p = S0 + sx;
				const unsigned char* S1p = S1 + sx;
#if __ARM_NEON
				int16x4_t _a0 = vdup_n_s16(a0);
				int16x4_t _a1 = vdup_n_s16(a1);
				uint8x8_t _S0 = uint8x8_t();
				uint8x8_t _S1 = uint8x8_t();

				_S0 = vld1_lane_u8(S0p, _S0, 0);
				_S0 = vld1_lane_u8(S0p + 1, _S0, 1);
				_S0 = vld1_lane_u8(S0p + 2, _S0, 2);
				_S0 = vld1_lane_u8(S0p + 3, _S0, 3);

				_S1 = vld1_lane_u8(S1p, _S1, 0);
				_S1 = vld1_lane_u8(S1p + 1, _S1, 1);
				_S1 = vld1_lane_u8(S1p + 2, _S1, 2);
				_S1 = vld1_lane_u8(S1p + 3, _S1, 3);

				int16x8_t _S016 = vreinterpretq_s16_u16(vmovl_u8(_S0));
				int16x8_t _S116 = vreinterpretq_s16_u16(vmovl_u8(_S1));
				int16x4_t _S0lowhigh = vget_low_s16(_S016);
				int16x4_t _S1lowhigh = vget_low_s16(_S116);
				int32x2x2_t _S0S1low_S0S1high = vtrn_s32(vreinterpret_s32_s16(_S0lowhigh), vreinterpret_s32_s16(_S1lowhigh));
				int32x4_t _rows01 = vmull_s16(vreinterpret_s16_s32(_S0S1low_S0S1high.val[0]), _a0);
				_rows01 = vmlal_s16(_rows01, vreinterpret_s16_s32(_S0S1low_S0S1high.val[1]), _a1);
				int16x4_t _rows01_sr4 = vshrn_n_s32(_rows01, 4);
				int16x4_t _rows1_sr4 = vext_s16(_rows01_sr4, _rows01_sr4, 2);
				vst1_s16(rows0p, _rows01_sr4);
				vst1_s16(rows1p, _rows1_sr4);
#else
				rows0p[0] = (S0p[0] * a0 + S0p[2] * a1) >> 4;
				rows0p[1] = (S0p[1] * a0 + S0p[3] * a1) >> 4;
				rows1p[0] = (S1p[0] * a0 + S1p[2] * a1) >> 4;
				rows1p[1] = (S1p[1] * a0 + S1p[3] * a1) >> 4;
#endif // __ARM_NEON

				ialphap += 2;
				rows0p += 2;
				rows1p += 2;
			}
		}

		prev_sy1 = sy;

		// vresize
		short b0 = ibeta[0];
		short b1 = ibeta[1];

		short* rows0p = rows0;
		short* rows1p = rows1;
		unsigned char* Dp = dst + stride * (dy);

#if __ARM_NEON
		int nn = (w * 2) >> 3;
#else
		int nn = 0;
#endif
		int remain = (w * 2) - (nn << 3);

#if __ARM_NEON
#if __aarch64__
		int16x4_t _b0 = vdup_n_s16(b0);
		int16x4_t _b1 = vdup_n_s16(b1);
		int32x4_t _v2 = vdupq_n_s32(2);
		for (; nn > 0; nn--)
		{
			int16x4_t _rows0p_sr4 = vld1_s16(rows0p);
			int16x4_t _rows1p_sr4 = vld1_s16(rows1p);
			int16x4_t _rows0p_1_sr4 = vld1_s16(rows0p + 4);
			int16x4_t _rows1p_1_sr4 = vld1_s16(rows1p + 4);

			int32x4_t _rows0p_sr4_mb0 = vmull_s16(_rows0p_sr4, _b0);
			int32x4_t _rows1p_sr4_mb1 = vmull_s16(_rows1p_sr4, _b1);
			int32x4_t _rows0p_1_sr4_mb0 = vmull_s16(_rows0p_1_sr4, _b0);
			int32x4_t _rows1p_1_sr4_mb1 = vmull_s16(_rows1p_1_sr4, _b1);

			int32x4_t _acc = _v2;
			_acc = vsraq_n_s32(_acc, _rows0p_sr4_mb0, 16);
			_acc = vsraq_n_s32(_acc, _rows1p_sr4_mb1, 16);

			int32x4_t _acc_1 = _v2;
			_acc_1 = vsraq_n_s32(_acc_1, _rows0p_1_sr4_mb0, 16);
			_acc_1 = vsraq_n_s32(_acc_1, _rows1p_1_sr4_mb1, 16);

			int16x4_t _acc16 = vshrn_n_s32(_acc, 2);
			int16x4_t _acc16_1 = vshrn_n_s32(_acc_1, 2);

			uint8x8_t _D = vqmovun_s16(vcombine_s16(_acc16, _acc16_1));

			vst1_u8(Dp, _D);

			Dp += 8;
			rows0p += 8;
			rows1p += 8;
		}
#else
		if (nn > 0)
		{
			asm volatile(
				"vdup.s16   d16, %8         \n"
				"mov        r4, #2          \n"
				"vdup.s16   d17, %9         \n"
				"vdup.s32   q12, r4         \n"
				"pld        [%0, #128]      \n"
				"vld1.s16   {d2-d3}, [%0 :128]!\n"
				"pld        [%1, #128]      \n"
				"vld1.s16   {d6-d7}, [%1 :128]!\n"
				"0:                         \n"
				"vmull.s16  q0, d2, d16     \n"
				"vmull.s16  q1, d3, d16     \n"
				"vorr.s32   q10, q12, q12   \n"
				"vorr.s32   q11, q12, q12   \n"
				"vmull.s16  q2, d6, d17     \n"
				"vmull.s16  q3, d7, d17     \n"
				"vsra.s32   q10, q0, #16    \n"
				"vsra.s32   q11, q1, #16    \n"
				"pld        [%0, #128]      \n"
				"vld1.s16   {d2-d3}, [%0 :128]!\n"
				"vsra.s32   q10, q2, #16    \n"
				"vsra.s32   q11, q3, #16    \n"
				"pld        [%1, #128]      \n"
				"vld1.s16   {d6-d7}, [%1 :128]!\n"
				"vshrn.s32  d20, q10, #2    \n"
				"vshrn.s32  d21, q11, #2    \n"
				"vqmovun.s16 d20, q10        \n"
				"vst1.8     {d20}, [%2]!    \n"
				"subs       %3, #1          \n"
				"bne        0b              \n"
				"sub        %0, #16         \n"
				"sub        %1, #16         \n"
				: "=r"(rows0p), // %0
				"=r"(rows1p), // %1
				"=r"(Dp),     // %2
				"=r"(nn)      // %3
				: "0"(rows0p),
				"1"(rows1p),
				"2"(Dp),
				"3"(nn),
				"r"(b0), // %8
				"r"(b1)  // %9
				: "cc", "memory", "r4", "q0", "q1", "q2", "q3", "q8", "q9", "q10", "q11", "q12");
		}
#endif // __aarch64__
#endif // __ARM_NEON
		for (; remain; --remain)
		{
			//             D[x] = (rows0[x]*b0 + rows1[x]*b1) >> INTER_RESIZE_COEF_BITS;
			*Dp++ = (unsigned char)(((short)((b0 * (short)(*rows0p++)) >> 16) + (short)((b1 * (short)(*rows1p++)) >> 16) + 2) >> 2);
		}

		ibeta += 2;
	}

	delete[] buf;
}

static int frame_resize_bilinear_c2(const VideoFormat &format, unsigned char *fData, const int fWidth, const int fHeight, unsigned char* &destData, int& dWidth, int& dHeight, float& ratio) {

	ratio = 1.0;
	dWidth = fWidth;
	dHeight = fHeight;
	int m = max(fWidth, fHeight);
	if (m > max_detect_spatial && max_detect_spatial > 0) {
		ratio = max_detect_spatial * 1.0 / m;
		dWidth = fWidth * ratio;
		dHeight = fHeight * ratio;
		if (!done_once_tasks) {
			blog(LOG_INFO,
			     "[OnVideoData] resized: ratio %.2f with expected max spatial %d ,original width %d and height %d",
			     ratio, max_detect_spatial, fWidth, fHeight);
		}
	}

	int allocSize = 0;
	switch(format) {
		case VideoFormat::YUY2:
		case VideoFormat::YVYU:
		case VideoFormat::HDYC:
		case VideoFormat::UYVY:
			allocSize = dWidth * dHeight * 2; break;
		case VideoFormat::ARGB:
		case VideoFormat::XRGB:
			allocSize = dWidth * dHeight * 4; break;
		default:
			break;
	}
	if (allocSize == 0)
		return allocSize;

	
	destData = new unsigned char[allocSize]();
	//1 use mat_pixel_resize to resize, or 2 use save_image to check image effectness ***
	if (ratio != 1.0) { // this is common case,so place srcYUY2Scaled above is reasonable
		long t1 = clock();
		int num = 1;// 1000;
		for (int i = 0; i < num; i++) {
			//v1 - copied from ncnn, a bit faster than detct on original directly, finally it's much faster than latter in entire procedure since
			// the next step image format conversion cost heavily!
			ncnn_resize_bilinear_c2(fData, fWidth, fHeight, fWidth * 2, destData, dWidth, dHeight, dWidth * 2);

			//v2 - failed, why? maybe it's vialabe by transform to bgr then resize
			//cv::Mat yuv(fHeight, fWidth, CV_8UC2, fData); ///yuy2 is 2 channels
			//cv::Mat goal ;
			////goal.data = destData;
			//cv::resize(yuv, goal, cv::Size(dWidth, dHeight), 0,0);
			//destData = goal.data;			
		}
		//if(frame_num % LogFrameInterval == 0)
		//	blog(LOG_INFO,"[%d] avg resize cost %f", frame_num, (clock() - t1) * 1.0 / num); //avg cost ~0.12ms for 1000 loop, 1ms in common in prod
	}
	else {
		memcpy(destData, fData, allocSize);
	}
	return allocSize;
}

static int frame_format_convert(const ValidFourccCodes &srcFormat,const unsigned char* fData,const int &size, const int &fWidth,const int &fHeight,
	const ValidFourccCodes &destFormat,  unsigned char* &destRGBData)
{
	zs::Frame srcFrame;
	srcFrame.size = size;
	srcFrame.data = new uint8_t[size];
	memcpy(srcFrame.data, fData, size);
	srcFrame.width = fWidth;
	srcFrame.height = fHeight;
	srcFrame.fourcc = (int)srcFormat;
	
	zs::Frame destRGB;
	destRGB.fourcc = (int)destFormat;
	static PixelFormatConverter converter;
	if (!converter.Convert(srcFrame, destRGB)) {
		return 0;
	}
	if(destRGBData == nullptr)
		destRGBData = new unsigned char[destRGB.size]();
	memcpy(destRGBData, destRGB.data, destRGB.size);

	return destRGB.size;
}

static ValidFourccCodes video_format_to_pixformat(const VideoFormat& format) {
	ValidFourccCodes codes = ValidFourccCodes::UNSUPORT;
	switch (format) {
		case VideoFormat::ARGB:codes = ValidFourccCodes::UNSUPORT; break;
		case VideoFormat::XRGB:codes = ValidFourccCodes::UNSUPORT; break;
		case VideoFormat::I420:codes = ValidFourccCodes::I420; break;
		case VideoFormat::NV12:codes = ValidFourccCodes::NV12; break;
		case VideoFormat::YV12:codes = ValidFourccCodes::UNSUPORT; break;
		case VideoFormat::Y800:codes = ValidFourccCodes::Y800; break;
		/* packed YUV formats */
		case VideoFormat::YVYU:codes = ValidFourccCodes::YUV1; break;
		case VideoFormat::YUY2:codes = ValidFourccCodes::YUY2; break;
		case VideoFormat::UYVY:codes = ValidFourccCodes::UYVY; break;
		case VideoFormat::HDYC:codes = ValidFourccCodes::UNSUPORT; break;
		/* encoded formats */
		case VideoFormat::MJPEG:ValidFourccCodes::JPEG; break;
		case VideoFormat::H264:ValidFourccCodes::UNSUPORT; break;
		default: codes = ValidFourccCodes::UNSUPORT; break;
	}
	return codes;
}

/*
 flow : use dot - product to solve cosQ = x, then use arccos(x) to derive angle Q
: param line1_start : tuple, (x, y)
: param line1_end :
: param line2_start :
: param line2_end :
: return : a common angle b / w 0~90, instead of radian
*/
static int angle(float p11x, float p11y, float p12x, float p12y, //line 1
	float p21x, int p21y, float p22x, float p22y, float trackId) 
{ //line 2

	float v1x = p12x - p11x;
	float v1y = p12y - p11y;
	float v2x = p22x - p21x;
	float v2y = p22y - p21y;

	float sr_v1 = sqrt(pow(v1x, 2) + pow(v1y, 2));
	float sr_v2 = sqrt(pow(v2x, 2) + pow(v2y, 2));

	float sum = v1x * v2x + v1y * v2y;

	float cosQ = 0;
	try {
		cosQ = sum * 1.0 / (sr_v1 * sr_v2);
	}
	catch (const char*& e) { // maybe divide by zero, eg.the pinky's last two nodes are same coordinates by prediction
		blog(LOG_ERROR,
			"failed to calculate cosine:%s, track id %d: ", e,
			trackId);
		return 1000; // return a unlegal angle
	}

	// normalize to tolerate a little change
	cosQ = cosQ > 1 && cosQ <= (1 + 0.1 * 1e-10) ? 1 : cosQ;
	if (cosQ > 1.0 or cosQ < -1.0) {
		blog(LOG_ERROR,
			"unreasonable cosine value met,ignore:%f , track id %d: ",
			cosQ, trackId);
		return 1000;
	}

	float ang = acos(cosQ) * 180 / PI;
	if (ang > 90)
		ang = 180 - ang;
	return ang;
}

static float euclidDistance(int p1x, int p1y, int p2x, int p2y)
{
	return sqrt(pow((p1x - p2x), 2) + pow((p1y - p2y), 2));
}

// called when each new frame come in; NOTE exaxtly same as 'static_image_mode=False and refine_landmarks=True' in python by default. refine_landmark is as 'with_attention=true'
bool detect(uchar *pImageData, int width, int height, FaceCalcResult &result)
{
	return Mediapipe_Face_Tracking_Detect_Frame_Direct(
		width, height, (void *)pImageData, result);
}

// Better to have no value than a bad one: inconsistent angles will cause jittering
const int ILLEGAL_ANGLE = 1000;
int fixAngle(float angle, string eulerName)
{	
	if (eulerName == "yaw") {		
		if (abs(angle) > 60) // use regression(speed) to compare current and prediction to exclude the illgegal angle
			return ILLEGAL_ANGLE;
		return angle;
	}
	if (eulerName == "pitch") {
		if (abs(angle) > 60)
			return ILLEGAL_ANGLE;
		return -angle;
	}
	//roll by default
	//if (abs(angle) > 60) { // use regression(speed) to compare current and prediction to exclude the illgegal angle
	//	return ILLEGAL_ANGLE;
	//}
	return -angle; 
}

float euclidDist(float p1x, float p1y, float p2x, float p2y) {
  return sqrt(pow(p1x - p2x, 2) + pow(p1y - p2y, 2));
}

json11::Json::object AnglesJsonData;
std::vector<std::shared_ptr<FaceDetectData2>> oldFaces; //fixed to same or less than the one 'num_faces' set in face_mesh_desktop_live.pbtxt
/*
*  smooth the face movement if both face are identified same person.
*  procedure:
*  1. check whether one of two special efficient movement distances is close enough. consider both are same one if it was yes
*  2. update the newFace position w/ new smoothed positions in case of same person, else use old ones to keep no changes
*  then use the updated positions for down-stream to do business logistic
*  
*  @return true in case of both faces are belong to same person, ie. do smoothen
*/
//todo: optimize by real change or mis-detection solution
bool smoothFaces(std::vector<std::shared_ptr<FaceDetectData2>>& oldFaces,
		 std::shared_ptr<FaceDetectData2>& newFace)
{
	bool smooth = false;
	//smoothen old face one by one
	for (int j = 0; j < oldFaces.size(); j++) {
		//FaceInfo* newFace = &newFaces[i];
		auto oldFace = oldFaces[j];
		//solution 1
		//1.
		float dist1 =
			euclidDist(oldFace->leftSpecialEffectPoint.x,
				   oldFace->leftSpecialEffectPoint.y,
				   newFace->leftSpecialEffectPoint.x,
				   newFace->leftSpecialEffectPoint.y); //`50 for me
		float dist2 = euclidDist(oldFace->rightSpecialEffectPoint.x,
					 oldFace->rightSpecialEffectPoint.y,
					 newFace->rightSpecialEffectPoint.x,
					 newFace->rightSpecialEffectPoint.y);
		float dist3 = euclidDist(newFace->leftSpecialEffectPoint.x,
					 newFace->leftSpecialEffectPoint.y,
					 newFace->rightSpecialEffectPoint.x,
					 newFace->rightSpecialEffectPoint.y);
		const float THRESHOLD_SAME_PERSON = dist3 / 2; //50 is very fast movement(in case of 640x480 and dist b/w face and camera is about 1M, dist b/w two points is about 45 pixels .
		bool test2 = false;
		if (min(dist1, dist2) < THRESHOLD_SAME_PERSON) { //use as bool to verify whether this is the first time to find face, use dynamic threshold since it dependents on distance

			//v5 velocity
			/*float leftx = firstEffectLeft.x - tmpLeft.x;
			float lefty = firstEffectLeft.y - tmpLeft.y;
			float ltang = lefty / leftx;
			float rightx = firstEffectRight.x - tmpRight.x;
			float righty = firstEffectRight.y - tmpRight.y;
			float rtang = righty / rightx;*/

			float avgDist = (dist1 + dist2) / 2;
			bool test = false;                // true;
			if (test || max(dist1, dist2) > 2 /*v1,v3,v4,v5:0; v2:4*/) { //or use std dev; maybe its more suitable to use dynamic threshold(eg. based on dist of two-points) than a definite value
				//v2
				/*firstEffectLeft = tmpLeft;
				  firstEffectRight = tmpRight;*/

				//v3 : use average distance,as a 'brief average movement distance'. static is good while movement is a bit bad
				/*firstEffectLeft.x = (firstEffectLeft.x + tmpLeft.x) / 2;
				  firstEffectLeft.y = (firstEffectLeft.y + tmpLeft.y) / 2;
				  firstEffectRight.x = (firstEffectRight.x + tmpRight.x) / 2;
				  firstEffectRight.y = (firstEffectRight.y + tmpRight.y) / 2;*/

				float gramma = 1. / pow(avgDist, 1. / 2); //todo: or use one of dist1 or dist2; this case is most smooth
				//float gramma = 1 / avgDist; //second smooth
				//float gramma = 1 / pow(2, avgDist); //most jitter
				newFace->leftSpecialEffectPoint.x =
					gramma * oldFace->leftSpecialEffectPoint.x +
					(1 - gramma) *
						newFace->leftSpecialEffectPoint.x;
				newFace->leftSpecialEffectPoint.y =
					gramma * oldFace->leftSpecialEffectPoint.y +
					(1 - gramma) *
						newFace->leftSpecialEffectPoint.y;
				newFace->rightSpecialEffectPoint.x =
					gramma *
						oldFace->rightSpecialEffectPoint.x +
					(1 - gramma) *
						newFace->rightSpecialEffectPoint.x;
				newFace->rightSpecialEffectPoint.y =
					gramma *
						oldFace->rightSpecialEffectPoint.y +
					(1 - gramma) *
						newFace->rightSpecialEffectPoint.y;
/*
#ifdef _DEBUG
				blog(LOG_INFO,"-- dist1 %.2f, dist2 %.2f. avg dist:%.2f, gram %.2f; dist w/ two points: %.2f",
				       dist1, dist2, avgDist, gramma, dist3);
#endif // _DEBUG
*/
				//v5
				/*float gramma = 0.5;
				  lvelocity = gramma * lvelocity + 0.1 * ltang;
				  rvelocity = gramma * rvelocity + 0.1 * rtang;
				  firstEffectLeft.x -= lvelocity;
				  firstEffectLeft.y -= lvelocity;
				  firstEffectRight.x -= rvelocity;
				  firstEffectRight.y -= rvelocity;*/

				//v6: accumulate two vectors(a bit worse delay than EWMA)
				/*newFace->specialEfficentLeft.x = (oldFace->specialEfficentLeft.x + newFace->specialEfficentLeft.x) / 2;
				  newFace->specialEfficentLeft.y = (oldFace->specialEfficentLeft.y + newFace->specialEfficentLeft.y) / 2;
				  newFace->specialEfficentRight.x = (oldFace->specialEfficentRight.x + newFace->specialEfficentRight.x) / 2;
				  newFace->specialEfficentRight.y = (oldFace->specialEfficentRight.y + newFace->specialEfficentRight.y) / 2;*/
			} else {
				//use old one to keep no changes. this means that both euler angles and positions are not changed
				newFace = oldFace; //ie. new face w/ old landmarks in fact to keep special effects no change
			}
			//fast quit
			smooth = true;
			break;
		} //else this is a new face, so it's needless to correlate w/ any old faces
	}
	
	//no face close enough is found(ie. are all new person),
	return smooth;
}
FacePoint poseInfo2FacePoint(PoseInfo lm,float oriRatio) {
	FacePoint fp;
	fp.x = lm.x / oriRatio;
	fp.y = lm.y / oriRatio;
	return fp;
}
float smoothAngleLandmark(float lm) {
	return lm; //most sense
	//return round(lm * 10) / 10; //mid-sense,accurate at tenile
	//return round(lm); //lowest sense
}

bool postProcess(FaceCalcResult result/*, Mat frame*/, int expectWidth, int expectHeight,
		 int frame_num, int LOG_BATCH,
		 std::vector<std::shared_ptr<FaceDetectData2>> &vfinalData,int oriW,int oriH,float oriRatio)
{
	 //construct the new faces data then compare to old ones for smooth op
	//vector<FaceDetectData2> newFaces;

	// == [1] first calculate the euler angles
	for (int m = 0; m < result.faceLandmarks.size(); ++m) { //iterate face by face
		vector<PoseInfo> single_face_NormalizedLandmarkList =
			result.faceLandmarks[m];

		long start = clock();
		std::vector<cv::Point2d> image_points;
		
		PoseInfo lm = single_face_NormalizedLandmarkList[1]; //tip
		//case: no flip anywhere
		if (SMOOTH_ANGLES_LANDMARKS) { // de-sense locations
			image_points.push_back(cv::Point2d(smoothAngleLandmark(lm.x), smoothAngleLandmark(lm.y))); // Nose tip
			lm = single_face_NormalizedLandmarkList[152];
			image_points.push_back(cv::Point2d(smoothAngleLandmark(lm.x), smoothAngleLandmark(lm.y))); // Chin
			lm = single_face_NormalizedLandmarkList[33];
			image_points.push_back(cv::Point2d(smoothAngleLandmark(lm.x), smoothAngleLandmark(lm.y))); // Left eye(based on IMAGE instead of person) left corner
			lm = single_face_NormalizedLandmarkList[263];
			image_points.push_back(cv::Point2d(smoothAngleLandmark(lm.x), smoothAngleLandmark(lm.y))); // Right eye right corner
			lm = single_face_NormalizedLandmarkList[62];
			image_points.push_back(cv::Point2d(smoothAngleLandmark(lm.x), smoothAngleLandmark(lm.y))); // Left Mouth corner
			lm = single_face_NormalizedLandmarkList[292];
			image_points.push_back(cv::Point2d(smoothAngleLandmark(lm.x), smoothAngleLandmark(lm.y))); // Right mouth corner
		}
		else {
			image_points.push_back(cv::Point2d(lm.x, lm.y)); // Nose tip
			lm = single_face_NormalizedLandmarkList[152];
			image_points.push_back(cv::Point2d(lm.x, lm.y)); // Chin
			lm = single_face_NormalizedLandmarkList[33];
			image_points.push_back(cv::Point2d(lm.x, lm.y)); // Left eye(based on IMAGE instead of person) left corner
			lm = single_face_NormalizedLandmarkList[263];
			image_points.push_back(cv::Point2d(lm.x, lm.y)); // Right eye right corner
			lm = single_face_NormalizedLandmarkList[62];
			image_points.push_back(cv::Point2d(lm.x, lm.y)); // Left Mouth corner
			lm = single_face_NormalizedLandmarkList[292];
			image_points.push_back(cv::Point2d(lm.x, lm.y)); // Right mouth corner
		}

		/* case: flipped in api
		int widthHalf = 0;// copyMat.cols / 2; //fix the position flipped in face_tracking_detect.cpp
		image_points.push_back(cv::Point2d(2* widthHalf-lm.x, lm.y ));    // Nose tip
		lm = single_face_NormalizedLandmarkList[152];
		image_points.push_back(cv::Point2d(2* widthHalf-lm.x, lm.y ));    // Chin
		lm = single_face_NormalizedLandmarkList[33];
		image_points.push_back(cv::Point2d(2* widthHalf-lm.x, lm.y ));     // Left eye(based on IMAGE instead of person) left corner
		lm = single_face_NormalizedLandmarkList[263];
		image_points.push_back(cv::Point2d(2* widthHalf-lm.x, lm.y));    // Right eye right corner
		lm = single_face_NormalizedLandmarkList[62];
		image_points.push_back(cv::Point2d(2* widthHalf-lm.x, lm.y ));    // Left Mouth corner
		lm = single_face_NormalizedLandmarkList[292];
		image_points.push_back(cv::Point2d(2* widthHalf-lm.x, lm.y ));    // Right mouth corner
		*/

		// 3D modelpoints.
		std::vector<cv::Point3d> model_points;

		model_points.push_back(
			cv::Point3d(0.0f, 0.0f, 0.0f)); // Nose tip
		model_points.push_back(
			cv::Point3d(0.0f, -330.0f, -65.0f)); //Chin
		model_points.push_back(cv::Point3d(
			-225.0f, 170.0f, -135.0f)); // Left eye left corner
		model_points.push_back(cv::Point3d(
			225.0f, 170.0f, -135.0f)); // Right eye rightcorner
		model_points.push_back(cv::Point3d(
			-150.0f, -150.0f, -125.0f)); // Left Mouth corner
		model_points.push_back(cv::Point3d(
			150.0f, -150.0f, -125.0f)); // Right mouth corner

		// Camera internals

		double focal_length = expectWidth; // Approximate focal length.

		Point2d center = cv::Point2d(expectWidth / 2, expectHeight / 2); //the w, h are always fixed so use int type is ok too
		cv::Mat camera_matrix = (cv::Mat_<double>(3, 3) << focal_length,
					 0, center.x, 0, focal_length, center.y,
					 0, 0, 1);
		cv::Mat dist_coeffs = cv::Mat::zeros(
			4, 1,
			cv::DataType<double>::type); // Assuming no lens distortion

		//cout << "Camera Matrix " << endl << camera_matrix << endl;

		// Outpu trotation and translation

		cv::Mat rotation_vector; // Rotation in axis-angle form
		cv::Mat translation_vector;
		//printf(" [%d] prepare pnp cost(ms): %d\r\n", image_index, clock() - start);

		// Solve fo rpose
		start = clock();
		bool success = cv::solvePnP(model_points, image_points, 
					    camera_matrix, dist_coeffs,
					    rotation_vector,
					    translation_vector);
		//printf(" [%d] solvePnP cost(ms): %d\r\n", image_index, clock() - start); //cost lightly here
		if (success) { //solve the euler angles
			// calculate rotation angles
			start = clock();
			float theta = cv::norm(rotation_vector, cv::NORM_L2);

			// transformed to quaterniond
			float w = cos(theta / 2);
			float x = sin(theta / 2) *
				  rotation_vector.at<double>(0, 0) / theta;
			float y = sin(theta / 2) *
				  rotation_vector.at<double>(1, 0) / theta;
			float z = sin(theta / 2) *
				  rotation_vector.at<double>(2, 0) / theta;

			float ysqr = y * y;
			// pitch(x - axis rotation);
			float t0 = 2.0 * (w * x + y * z);
			float t1 = 1.0 - 2.0 * (x * x + ysqr);
			float pitch = atan2(t0, t1);
			float pitch0 = atan(t0 / t1);

			// yaw(y - axis rotation)
			float t2 = 2.0 * (w * y - z * x);
			if (t2 > 1.0)
				t2 = 1.0;
			if (t2 < -1.0)
				t2 = -1.0;

			float yaw = asin(t2);

			// roll(z - axis rotation)
			float t3 = 2.0 * (w * z + x * y);
			float t4 = 1.0 - 2.0 * (ysqr + z * z);
			float roll = atan2(t3, t4);
			float roll0 = atan(t3 / t4);
			//int Y = fixAngle((pitch / PI) * 180, "pitch");
			int X = fixAngle((yaw / PI) * 180, "yaw");
			//int Z = fixAngle((roll / PI) * 180, "roll"); //last:-roll
			int Y0 = fixAngle((pitch0 / PI) * 180, "pitch");
			int Z0 = fixAngle((roll0 / PI) * 180, "roll"); //last:-roll
			//printf(" [%d] euler angles calc cost(ms): %d\r\n", image_index, clock() - start); //cost lightly

			//reduce to verify conveniently the result whether ok or not; should be moved to caller
			//if (frame_num % LOG_BATCH == 0) {
			//	stringstream fmt;				
			//	for (int i = 0; i < image_points.size(); i++) {
			//		fmt << to_string((image_points[i].x)) << "-" << to_string((image_points[i].y)) << ",";
			//	}
			//	/*string s = fmt.str();
			//	printf(s.c_str());*/
			//	blog(LOG_INFO, " [%d] frame face id %d : pitch %d, yaw %d, roll %d - landmarks %s", frame_num, m, Y0, X, Z0, fmt.str().c_str());
			//}

			//add two more landmarks to locate the special effect shown on head
			lm = single_face_NormalizedLandmarkList[67];
			FacePoint leftSEPoint;
			leftSEPoint.x = lm.x / oriRatio;
			leftSEPoint.y = lm.y / oriRatio;

			lm = single_face_NormalizedLandmarkList[297];
			FacePoint rightSEPoint;
			rightSEPoint.x = lm.x / oriRatio;
			rightSEPoint.y = lm.y / oriRatio;

			std::shared_ptr<FaceDetectData2> pData =
				std::make_shared<FaceDetectData2>();
			//set euler angles first
			pData->yaw = X;
			pData->pitch = Y0;
			pData->roll = Z0;

			//vv
			if (SMOOTH_ANGLES) {
				// todo: smooth against face id
				// todo: add switch web-socket param instead of hard code here
				// todo: this smoothness depends on the result of faces so raise the prob to avoid fixing css drawing
				// smooth the angles, especially in side faces cases. the solution below is similar to kalman filter
				// use acceleration(a simple version of kalman filter) and time-elapced checking mechanisms
				// 1. acceleration
				// todo: outside code is slow
				long ts = clock(); //elapsed ms since start up this app	

				float predRollAngle;
				static const int TIME_CHECKING_THRESHOLD = 150; //ms in unit, reset if no data accept after about 3~5 frames
				// step 1: verify if weird angle( this case will be ignored if the roll never be ILLEGAL_ANGLE!
				if (pData->roll == ILLEGAL_ANGLE) { //always use the last angle for definitely unreasonable angle
					if (frame_num == 1) {
						AnglesJsonData = json11::Json::object{
							{"lastRollAngle", pData->roll * 1.0},
							{"lastRollSpeed", 0.},
							{"firstRollSpeed",0.},
							{"lastRollTS",    ts * 1.0}, //give a placehodler 
						};
					}
					else { //use last info as current data
						pData->roll = AnglesJsonData["lastRollAngle"].number_value();
						//AnglesJsonData["lastRollAngle"] keep no change
						AnglesJsonData["firstRollSpeed"] = AnglesJsonData["lastRollSpeed"];
						AnglesJsonData["lastRollSpeed"] = 0; //current is same as last so it's 0 acceleration
						AnglesJsonData["lastRollTS"] = ts * 1.0;
					}
				}
				// step 2: do smooth now
				else {
					if (frame_num == 1 //initiate w/ first frame data. frame_num has been set to 1 when come in OnVideoData() at first time
						|| (ts - AnglesJsonData["lastRollTS"].number_value()) > TIME_CHECKING_THRESHOLD) { //ignore obsolete info if more than sometime
						if (frame_num == 1) {
							AnglesJsonData = json11::Json::object{
								{"lastRollAngle", pData->roll * 1.0},
								{"lastRollSpeed", 0.},
								{"firstRollSpeed",0.},
								{"lastRollTS",    ts * 1.0}, //give a placehodler 
							};
						}
						else { //todo: very rare case since ths TS is always updated
							AnglesJsonData["lastRollAngle"] = pData->roll * 1.0;
							AnglesJsonData["firstRollSpeed"] = 0.;
							AnglesJsonData["lastRollSpeed"] = 0;
							AnglesJsonData["lastRollTS"] = ts * 1.0;
						}
						//never reset the frame_num to 0 even if the threshold is met!! since it's used in other cases
					}
					else if (frame_num == 2 || frame_num == 3) { //todo: this condition may be optional since the next condition is self-autonomous(self-adaptive)
						float dt = ts - AnglesJsonData["lastRollTS"].number_value(); //ms in unit
						float curRollSpeed = (pData->roll - AnglesJsonData["lastRollAngle"].number_value()) / dt;
						AnglesJsonData["firstRollSpeed"] = AnglesJsonData["lastRollSpeed"]; //always transit last speed to first, must do it before below block
						AnglesJsonData["lastRollSpeed"] = curRollSpeed;
						AnglesJsonData["lastRollAngle"] = pData->roll;
						AnglesJsonData["lastRollTS"] = ts * 1.0;
					}
					else {
						static const int CONFIDENT_ANGLE = 10;
						static const int UNCONFIDENT_ANGLE = 20;// 25;
						float dt = ts - AnglesJsonData["lastRollTS"].number_value(); //ms in unit, used before update to current ts
						//a. detection is exactly in reasonable range(assume that the history is smooth)
						if (abs(pData->roll - AnglesJsonData["lastRollAngle"].number_value()) <= CONFIDENT_ANGLE) { //during reasonable angle , use observation to update
							AnglesJsonData["firstRollSpeed"] = AnglesJsonData["lastRollSpeed"]; //always transit last speed to its former first, must do it before update lastRollSpeed
							float curRollSpeed = (pData->roll - AnglesJsonData["lastRollAngle"].number_value()) / dt; //todo: 1/2 more
							AnglesJsonData["lastRollSpeed"] = curRollSpeed;
							AnglesJsonData["lastRollAngle"] = pData->roll;
							AnglesJsonData["lastRollTS"] = ts * 1.0; //do this in public place
						}
						//b. use last angle if it's obviously out of reasonable range. it's unnecessary to smooth for this case since it's most likely mis-detection
						//	todo: or use prediction to further do colaborative filtering
						else if (abs(pData->roll - AnglesJsonData["lastRollAngle"].number_value()) >= UNCONFIDENT_ANGLE) {
							//it's possible that if no face detected since last detection
							if (ts - AnglesJsonData["lastRollTS"].number_value() <= 500) { // within a normal time window, 2s here
								pData->roll = AnglesJsonData["lastRollAngle"].number_value();
								//AnglesJsonData["lastRollAngle"] keep no change
								AnglesJsonData["firstRollSpeed"] = 0;
								AnglesJsonData["lastRollSpeed"] = 0; //current is same as last so it's 0 acceleration
								//never change the lastRollTS in this case to make it expired
							}
							else { // out of range , use current one
								AnglesJsonData["lastRollAngle"] = pData->roll;
								AnglesJsonData["firstRollSpeed"] = 0;
								AnglesJsonData["lastRollSpeed"] = 0; //current is same as last so it's 0 acceleration
								AnglesJsonData["lastRollTS"] = ts * 1.0; //do this in public place
							}
						}
						else {//c. fix only if in a relatively reasonable range,ie. gray range. 				

						       // now all stuff are prepared so do it's real work
							// (-0.010204081423580647-(-0.11842105537652969)) / (59730-59633) = 0.00111563890673143343298969072165
							//(-0.094754397869110107)/(56960-56862) = -0.00096688
							float avgAcce = 0;
							if (AnglesJsonData["lastRollSpeed"].number_value() != AnglesJsonData["firstRollSpeed"].number_value())
								avgAcce = (AnglesJsonData["lastRollSpeed"].number_value() - AnglesJsonData["firstRollSpeed"].number_value()) / dt; //squared ms in unit
							// -11 + -0.010204081423580647 * (59730-59633) + 1/2 * 0.00111563890673143343298969072165 * (59730-59633) * (59730-59633) = -6.7
							//2.0541238784790039-0.094754397869110107*(56960-56862) + 1/2 * (-0.00096688) * (56960-56862)^2 = 
							float predRollAngle = AnglesJsonData["lastRollAngle"].number_value() + AnglesJsonData["lastRollSpeed"].number_value() * dt + 1. / 3 * avgAcce * pow(dt, 2); // chnage from original formula from 1/2 to 1/3 to narrow down acceleration
							predRollAngle = isnan(predRollAngle) ? pData->roll : predRollAngle; //use detection if a nan raise

							float tmpA;
							static const int VALID_PRED_RANGE = 20; //angle in unit
							//prefer near angle(ie. self-autonomous even if the angle is out of reasonable range,eg. 1000)
							// reset to initialization at first if this pData->roll is weird
							/*if (abs(predRollAngle - AnglesJsonData["lastRollAngle"].number_value()) <= CONFIDENT_ANGLE) { // prefer pred if close enough
								tmpA = predRollAngle; //this case may be from smooth movement which result in always same angle
							}
							else*/ if (abs(predRollAngle - AnglesJsonData["lastRollAngle"].number_value()) >= UNCONFIDENT_ANGLE) { //exclude obviously wrong angle first
								tmpA = (pData->roll + AnglesJsonData["lastRollAngle"].number_value()) / 2; //back to detection
							}
							else {
								tmpA = (pData->roll + predRollAngle) / 2;
							}
							//now both pred and detection are within gray range
							//else if (abs(predRollAngle - pData->roll) > VALID_PRED_RANGE) {// prevent the prediction from being larger and larger w/o controll
							//	
							//}
							//else choose a closer one
							/*else if (abs(pData->roll - AnglesJsonData["lastRollAngle"].number_value()) < abs((predRollAngle - AnglesJsonData["lastRollAngle"].number_value())))
								tmpA = pData->roll;
							else
								tmpA = predRollAngle;*/
							if (frame_num % LogFrameInterval == 0)
								blog(LOG_INFO, " [%d] corrected roll! from %d to %.1f(pred:%.1f) ", frame_num, pData->roll, tmpA, predRollAngle);
							AnglesJsonData["firstRollSpeed"] = AnglesJsonData["lastRollSpeed"]; //always transit last speed to its former first, must do it before update lastRollSpeed
							float curRollSpeed = (tmpA - AnglesJsonData["lastRollAngle"].number_value()) / dt; //todo: 1/2 more
							AnglesJsonData["lastRollSpeed"] = curRollSpeed;
							AnglesJsonData["lastRollAngle"] = tmpA;
							pData->roll = tmpA;
							AnglesJsonData["lastRollTS"] = ts * 1.0; //do this in public place
						}

					}
				}
				/*if (frame_num % LogFrameInterval == 0) {
					stringstream fmt;
					for (std::map<string, json11::Json>::iterator iter = AnglesJsonData.begin(); iter != AnglesJsonData.end(); ++iter) {
						fmt << iter->first << ": " << iter->second.dump() << "	";
					}
					blog(LOG_INFO, " [%d] angles data: %s", frame_num, fmt.str().c_str());
					fmt.clear();
				}*/
			}
			//^^ smooth angles

			pData->leftSpecialEffectPoint = leftSEPoint;
			pData->rightSpecialEffectPoint = rightSEPoint;
			pData->lm4 = poseInfo2FacePoint(single_face_NormalizedLandmarkList[4], oriRatio);
			pData->lm50 = poseInfo2FacePoint(single_face_NormalizedLandmarkList[50], oriRatio);
			pData->lm280 = poseInfo2FacePoint(single_face_NormalizedLandmarkList[280], oriRatio);
			pData->lm61 = poseInfo2FacePoint(single_face_NormalizedLandmarkList[61], oriRatio);
			pData->lm291 = poseInfo2FacePoint(single_face_NormalizedLandmarkList[291], oriRatio);

			pData->lm130 = poseInfo2FacePoint(single_face_NormalizedLandmarkList[130], oriRatio);
			pData->lm155 = poseInfo2FacePoint(single_face_NormalizedLandmarkList[155], oriRatio);
			pData->lm359 = poseInfo2FacePoint(single_face_NormalizedLandmarkList[359], oriRatio);
			pData->lm362 = poseInfo2FacePoint(single_face_NormalizedLandmarkList[362], oriRatio);

			//FaceDetectData2 face;
			//face.leftSpecialEffectPoint = leftSEPoint;
			//face.rightSpecialEffectPoint = rightSEPoint;
			smoothFaces(oldFaces, pData); //here may change the pData's value
			vfinalData.push_back(pData); //must be placed after smoothFaces()!
			
			/*for (int i = 0; i < image_points.size(); i++) {
				cv::circle(frame, image_points[i], 1, cv::Scalar(0, 0, 200, 155), -1, 8, 0);
			}*/
		} else {
			cout << "failed to calculate solvePnP() on opencv!" << endl;
			return false;
		}
	}

	// == [2] update the smoothed special efficient points
	//switch new and old faces AFTER renderring!!
	//old method - fail for resolution less than 1280
	//oldFaces = vfinalData;

	//new method
	oldFaces.clear();
	for (int i = 0; i < vfinalData.size(); i++) { //todo: why the oldFaces's elemnt's pointer is been changed in 800 but not change in 1280? so use this mode instead
		std::shared_ptr<FaceDetectData2> pData = std::make_shared<FaceDetectData2>();
		FaceDetectData2 data = *vfinalData[i];
		pData->yaw = data.yaw;
		pData->pitch = data.pitch;
		pData->roll = data.roll;
		pData->leftSpecialEffectPoint = data.leftSpecialEffectPoint;
		pData->rightSpecialEffectPoint = data.rightSpecialEffectPoint;
		pData->lm4 = data.lm4;
		pData->lm50 = data.lm50;
		pData->lm280 = data.lm280;
		pData->lm61 = data.lm61;
		pData->lm291 = data.lm291;

		pData->lm130 = data.lm130;
		pData->lm155 = data.lm155;
		pData->lm359 = data.lm359;
		pData->lm362 = data.lm362;

		oldFaces.push_back(pData);
	}

	return true;
}


//release some resources
bool release()
{
	if (Mediapipe_Face_Tracking_Release()) {
		std::cout << "Mediapipe release success" << std::endl;
	} else {
		std::cout << "Mediapipe release failed!" << std::endl;
	}
	dllLoader.UnloadDynamicModule();
#ifdef _DEBUG //note this name is not same as "DEBUG"!
	      //getchar(); //debug used only to quit via any input
#endif
	return true;
}

void DetectXRGBNoConvert(const VideoConfig& videoConfig, unsigned char* data, const size_t& size, const int& width, const int& height, std::vector <std::shared_ptr<FaceDetectData2>>& vfinalData)
{
	//ncnn::Mat ncnn_img = ncnn::Mat::from_pixels(data, ncnn::Mat::PIXEL_BGRA2RGB, width, height); //NOTE: this fromat is different to image process . see image usage in main(). ie. detect by RGB while draw results by BGR

	long t4 = clock();
	int channels = 3;
	unsigned char* srcYUY2 = data;
	int rawW = width;
	int rawH = height;

	//v1
	//std::vector<Bbox> finalBbox;
	//s_mtcnn.SetMinFace(miniFace);
	//s_mtcnn.detect(ncnn_img, finalBbox); // RGB style to detection

	FaceCalcResult result;
	detect(data, width, height, result); //v2
	postProcess(result, /*nullptr,*/ width, height, frame_num,
			LogFrameInterval, vfinalData, rawW, rawH, 1);

	/*if (finalBbox.size() > 0) {
		CnnBoxDataToJsonFaceData(1.0, width, height, finalBbox, vfinalData);
		CalculateEulerAngleWithData(vfinalData);
	}*/
}

bool DShowInput::OnDetectFaceWithFrame(const VideoConfig& videoConfig, unsigned char* data, const size_t& size, const int& fWidth, const int& fHeight,
	std::vector <std::shared_ptr<FaceDetectData2>> & vfinalData)
{
	bool had_detect = false;
	if (videoConfig.format == VideoFormat::XRGB || videoConfig.format == VideoFormat::XRGB)
	{
		DetectXRGBNoConvert(videoConfig, data, size, frame.width, frame.height,vfinalData);
		return true;
	}
	do {
		// vvv detection function block vvvvvvvvvvvvvvvvv
		ValidFourccCodes videoPixCode = video_format_to_pixformat(videoConfig.format);
		if (videoPixCode != ValidFourccCodes::UNSUPORT && detect_face && frame_num % DetectInterval == 0) {
			// vv face detection entrance
			// entire flow: YUY2 > RGB > Mat(cv2) > model
			//  https://stackoverflow.com/questions/42873173/resize-yuv-byte-array-image-to-a-specific-width-and-height
			long t1 = clock();
			// == step 1: YUY2 > RGB
			//// solution 1: manual method
			int expect_width = 0, expect_height = 0;
			unsigned char* frameScaled = nullptr;
			float ratio = 0.f;
			int sizeScale = frame_resize_bilinear_c2(videoConfig.format, data, fWidth, fHeight, frameScaled, expect_width, expect_height, ratio);
			if (!frameScaled)
			{
				expect_width = fWidth;
				expect_height = fHeight;
				frameScaled = new unsigned char[size]();
				sizeScale = size;
				ratio = 1.0;
				memcpy(frameScaled, data, size);
			}

			/*cv::Mat yuy2(expect_height, expect_width, CV_8UC2, frameScaled);
			cv::Mat dst;
			cv::cvtColor(yuy2, dst, CV_YUV2BGR_YUY2);
			cv::imwrite("D:/Projects/CPP/obs-studio/trunk/yuy2.jpg", dst);*/

			// Convert frame format to RGB24 format, i.e. yuy2 to bgr
			unsigned char* rgbData = nullptr;
			int sizeRGB = frame_format_convert(videoPixCode, frameScaled, sizeScale, expect_width, expect_height, ValidFourccCodes::BGR24 //raw:RGB24, same effect as BGR24
				, rgbData);
			delete[] frameScaled;
			if (!rgbData)
				return false;

		
			long t2 = clock();
			preprocess_cost_ms += t2 - t1;

			int channels = 3;
			unsigned char* srcYUY2 = data;
			int rawW = frame.width;
			int rawH = frame.height;

			long s1 = clock();
			

			FaceCalcResult result;
			bool rst = detect(rgbData, expect_width, expect_height, result); //todo: check the prob, set it be 0.7+ then verify by 2022-03-15_19-11-20-opencv.avi
			pure_detect_cost_ms += clock() - s1;
			if (rst) { //v2
				//it's a good idea to use multi-threads to do below processes respectively
				postProcess(result, /*nullptr,*/ expect_width,
					expect_height, frame_num, LogFrameInterval, vfinalData, rawW, rawH, ratio); //v2: it costs cheap so put here is ok too

			}
			else {
				blog(LOG_ERROR,
					"Failed to detect! must check it now!");
			}
			if (rgbData)
			{
				delete[]rgbData;
				rgbData = nullptr;
			}

			long e1 = clock();
			detect_cost_ms += e1 - s1;

			unsigned char* bgrFrame = nullptr; 
			int sizeBGR = 0;
			if (result.faceLandmarks.size() > 0) {
				faces_frame_detected++;
				if (drawBboxes) {
					sizeBGR = frame_format_convert(videoPixCode, data, size, fWidth, fHeight, ValidFourccCodes::BGR24, bgrFrame);
				}
			}
			face_handled += result.faceLandmarks.size();		

			if (frame_num % LogFrameInterval == 0) {
				blog(LOG_INFO,
					" [%d] avg detection+postProc cost(ms) in past %d frames:%.0f(pure avg detect:%.0f), preprocess:%.0f." \
					"frames w/ faces detected % d; w % d, h % d. faces postProcess:%d",
					frame_num, LogFrameInterval,
					detect_cost_ms * 1.0 / LogFrameInterval,
					pure_detect_cost_ms * 1.0 / LogFrameInterval,
					preprocess_cost_ms * 1.0 / LogFrameInterval,
					faces_frame_detected, rawW, rawH,
					face_handled
				);
				faces_frame_detected = 0;
				detect_cost_ms = 0;
				preprocess_cost_ms = 0;
				pure_detect_cost_ms = 0;
				face_handled = 0;
			}


			if (vfinalData.size() > 0)
			{
				if (drawBboxes)
				{
					Mat fr(fHeight, fWidth, CV_8UC3);
					fr.data = bgrFrame;
					//fr.dims = 3;
					for (auto& face : vfinalData)
					{
						cv::Point2d tmpLeft = cv::Point2d(
							face->leftSpecialEffectPoint.x,
							face->leftSpecialEffectPoint.y);
						cv::Point2d tmpRight = cv::Point2d(
							face->rightSpecialEffectPoint.x,
							face->rightSpecialEffectPoint.y);

						cv::circle(fr, tmpLeft, 22,
							cv::Scalar(200, 0, 0,155),-1, 8, 0);
						cv::circle(fr, tmpRight, 22,
							cv::Scalar(200, 0, 0,155),-1, 8, 0);
					}
				}
			}

			// == step 3: transform BGR format back to yuy2 if in need
			if (drawBboxes && result.faceLandmarks.size() > 0) { //avoid to decline perf; [optional:used to check face detection effect ONLY. disable in Product]
				if (flipImage) {
					long t1 = clock();
					Mat src(fHeight, fWidth, CV_8UC3);
					src.data = bgrFrame;
					Mat dst;
					cv::flip(src, dst, 1);
					delete[]bgrFrame;
					bgrFrame = dst.data;
					frame_format_convert(ValidFourccCodes::BGR24, bgrFrame, sizeBGR, fWidth, fHeight, videoPixCode, srcYUY2);
					if (frame_num % LogFrameInterval == 0)
						blog(LOG_INFO, "[%d] flip1 cost %d ms",frame_num, clock() - t1);
				}
				else {
					frame_format_convert(ValidFourccCodes::BGR24, bgrFrame, sizeBGR, fWidth, fHeight, videoPixCode, srcYUY2);
					delete[]bgrFrame;
				}
			}
			else if (flipImage) {
				long t1 = clock();
				unsigned char* bgrFrame = nullptr;
				sizeBGR = frame_format_convert(videoPixCode, data, size, fWidth, fHeight, ValidFourccCodes::BGR24, bgrFrame);
				Mat src(fHeight, fWidth, CV_8UC3);
				src.data = bgrFrame;
				Mat dst;
				cv::flip(src, dst, 1);
				delete[]bgrFrame;
				bgrFrame = dst.data;
				frame_format_convert(ValidFourccCodes::BGR24, bgrFrame, sizeBGR, fWidth, fHeight, videoPixCode, srcYUY2);
				if (frame_num % LogFrameInterval == 0)
					blog(LOG_INFO, "[%d] flip2 cost %d ms", frame_num, clock() - t1);
			}
		}
		else if (flipImage) {
			
		}
	}
	while (0);
	return had_detect;
}

VideoCapture cap ; //this file should be same shape as camera; failed to start obs if no camera
const int MAX_FRAMES = 100000;
void DShowInput::OnVideoData(const VideoConfig& config, unsigned char* data,
	size_t size, long long startTime,
	long long endTime, long rotation)
{
	long ts = clock();
	frame_num++;	

	if (rotation != lastRotation) {
		lastRotation = rotation;
		obs_source_set_async_rotation(source, rotation);
	}

	if (videoConfig.format == VideoFormat::H264) {
		OnEncodedVideoData(AV_CODEC_ID_H264, data, size, startTime); //forward to obs_source_output_video2()
		return;
	}

	if (videoConfig.format == VideoFormat::MJPEG) {
		OnEncodedVideoData(AV_CODEC_ID_MJPEG, data, size, startTime); //forward to obs_source_output_video2()
		return;
	}

	const int cx = config.cx;
	const int cy_abs = config.cy_abs;
	frame.timestamp = (uint64_t)startTime * 100;
	frame.width = config.cx;
	frame.height = cy_abs;
	frame.format = ConvertVideoFormat(config.format);
	frame.flip = flip;
	
	if (!done_once_tasks) { //this var is updated at last
		blog(LOG_INFO, "[OnVideoData]-H:%d,W:%d", frame.height, frame.width);

		if (READ_FROM_FILE) {
			string file = "D:/Temp/video_out/2022-03-15_19-11-20-all-opencv.avi";// todo: only avi format can be read in obs! and both nvDecMFTMjpegx have been deleted in Lam pc, may be caused by obs video output settings
			//file = "D:/Temp/video_out/2022-03-15_19-11-20-xunjie.avi";
			cap = VideoCapture(file);
			if (!cap.isOpened()) {
				blog(LOG_ERROR, "*cant open file %s", file);
				return;
			}
		}
	}

	// vvvv test block for read video file intead of camera
	if (READ_FROM_FILE) {
		if (MAX_FRAMES > 0 && frame_num > MAX_FRAMES) {
			blog(LOG_ERROR, " !!max frames(%d) from file met, stop now!", MAX_FRAMES);
			return;
		}

		//cv::VideoCapture cap("D:/Temp/video_out/gestures.mp4-1634903305-320-mtcnn-25.avi");
		cv::Mat frame_file;
		bool rst = cap.read(frame_file);
		if (!rst) {
			blog(LOG_ERROR, " No more frames(ie. max:%d) to read in video file, stop now !", frame_num - 1);
			return;
		}
		//imwrite("D:/Projects/CPP/obs-studio/trunk/frame_file.jpg", frame_file);
		//transform from bgr to yuy2 so the code followed keep same
		unsigned char* yuy2Frame = nullptr;
		int sizeBGR = frame_format_convert(ValidFourccCodes::BGR24, frame_file.data, frame.width * frame.height * 3, frame.width, frame.height, ValidFourccCodes::YUY2, yuy2Frame);
		if (sizeBGR == 0) {
			blog(LOG_ERROR, " failed to convert video format to yuy2,stop now.");
			return;
		}
		data = yuy2Frame; //replace the input data
	}
	// ^^^^ test:read from file
	
	/* YUV DIBS are always top-down */
	if (config.format == VideoFormat::XRGB ||
		config.format == VideoFormat::ARGB) {
		/* RGB DIBs are bottom-up by default */
		if (!config.cy_flip)
			frame.flip = !frame.flip;
	}

	if (videoConfig.format == VideoFormat::XRGB ||
		videoConfig.format == VideoFormat::ARGB) {
		frame.data[0] = data;
		frame.linesize[0] = cx * 4;

	}
	else if (videoConfig.format == VideoFormat::YVYU ||
		videoConfig.format ==
		VideoFormat::YUY2 || 
		videoConfig.format == VideoFormat::HDYC ||
		videoConfig.format == VideoFormat::UYVY) {
		frame.data[0] = data;
		frame.linesize[0] = cx * 2;

	}
	else if (videoConfig.format == VideoFormat::I420) {
		frame.data[0] = data;
		frame.data[1] = frame.data[0] + (cx * cy_abs);
		frame.data[2] = frame.data[1] + (cx * cy_abs / 4);
		frame.linesize[0] = cx;
		frame.linesize[1] = cx / 2;
		frame.linesize[2] = cx / 2;

	}
	else if (videoConfig.format == VideoFormat::YV12) {
		frame.data[0] = data;
		frame.data[2] = frame.data[0] + (cx * cy_abs);
		frame.data[1] = frame.data[2] + (cx * cy_abs / 4);
		frame.linesize[0] = cx;
		frame.linesize[1] = cx / 2;
		frame.linesize[2] = cx / 2;
	}
	else if (videoConfig.format == VideoFormat::NV12) {
		frame.data[0] = data;
		frame.data[1] = frame.data[0] + (cx * cy_abs); 
		frame.linesize[0] = cx;
		frame.linesize[1] = cx;
	}
	else if (videoConfig.format == VideoFormat::Y800) {
		frame.data[0] = data;
		frame.linesize[0] = cx;

	}
	else {
		/* other formats not support now */
		return;
	}

	if (ShouldDetectFace())
	{
		int start = clock();
		std::vector <std::shared_ptr<FaceDetectData2>> vDetectResult;

		OnDetectFaceWithFrame(config, data, size, frame.width, frame.height, vDetectResult);
		int end = clock();
#ifdef _DEBUG
		blog(LOG_DEBUG, "[Camera] Detect Face time:%d", end - start);
#endif
		static long long hasCheckFace = 0;
		if (vDetectResult.size() > 0) { //avoid to decline perf; [optional:used to check face detection effect ONLY. disable in Product]

			hasCheckFace = 0;
			handleDetectResult(vDetectResult, frame.width, frame.height);
		}
		else
			hasCheckFace++;
		if (hasCheckFace == 1)
			handleDetectResult(vDetectResult, frame.width, frame.height);
	}

	obs_source_output_video2(source, &frame); // flush to graphic card, cost about 1ms
	done_once_tasks = true;
	UNUSED_PARAMETER(endTime); /* it's the enndd tiimmes! */
	UNUSED_PARAMETER(size);

	function_cost_ms += clock() - ts;
	if (frame_num % LogFrameInterval == 0) { //always output this info
		blog(LOG_INFO, "[%d]= end of OnVideoData(), avg cost(ms) % .0f",frame_num,function_cost_ms /LogFrameInterval); //100ms for opencv (yuy2 to BGR)
		function_cost_ms = 0;
	}
}

void DShowInput::OnEncodedAudioData(enum AVCodecID id, unsigned char* data,
	size_t size, long long ts)
{
	if (!ffmpeg_decode_valid(audio_decoder)) {
		if (ffmpeg_decode_init(audio_decoder, id, false) < 0) {
			blog(LOG_WARNING, "Could not initialize audio decoder");
			return;
		}
	}

	bool got_output = false;
	do {
		bool success = ffmpeg_decode_audio(audio_decoder, data, size,
			&audio, &got_output);
		if (!success) {
			blog(LOG_WARNING, "Error decoding audio");
			return;
		}

		if (got_output) {
			audio.timestamp = (uint64_t)ts * 100;
#if LOG_ENCODED_AUDIO_TS
			blog(LOG_DEBUG, "audio ts: %llu", audio.timestamp);
#endif
			obs_source_output_audio(source, &audio);
		}
		else {
			break;
		}

		ts += int64_t(audio_decoder->frame->nb_samples) * 10000000LL /
			int64_t(audio_decoder->frame->sample_rate);
		size = 0;
		data = nullptr;
	} while (got_output);
}

void DShowInput::OnAudioData(const AudioConfig& config, unsigned char* data,
	size_t size, long long startTime,
	long long endTime)
{
	size_t block_size;

	if (config.format == AudioFormat::AAC) {
		OnEncodedAudioData(AV_CODEC_ID_AAC, data, size, startTime);
		return;
	}
	else if (config.format == AudioFormat::AC3) {
		OnEncodedAudioData(AV_CODEC_ID_AC3, data, size, startTime);
		return;
	}
	else if (config.format == AudioFormat::MPGA) {
		OnEncodedAudioData(AV_CODEC_ID_MP2, data, size, startTime);
		return;
	}

	audio.speakers = convert_speaker_layout((uint8_t)config.channels);
	audio.format = ConvertAudioFormat(config.format);
	audio.samples_per_sec = (uint32_t)config.sampleRate;
	audio.data[0] = data;

	block_size = get_audio_bytes_per_channel(audio.format) *
		get_audio_channels(audio.speakers);

	audio.frames = (uint32_t)(size / block_size);
	audio.timestamp = (uint64_t)startTime * 100;

	if (audio.format != AUDIO_FORMAT_UNKNOWN)
		obs_source_output_audio(source, &audio);

	UNUSED_PARAMETER(endTime);
}

struct PropertiesData {
	DShowInput* input;
	vector<VideoDevice> devices;
	vector<AudioDevice> audioDevices;

	bool GetDevice(VideoDevice& device, const char* encoded_id) const
	{
		DeviceId deviceId;
		DecodeDeviceId(deviceId, encoded_id);

		for (const VideoDevice& curDevice : devices) {
			if (deviceId.name == curDevice.name &&
				deviceId.path == curDevice.path) {
				device = curDevice;
				return true;
			}
		}

		return false;
	}
};

static inline bool ConvertRes(int& cx, int& cy, const char* res)
{
	return sscanf(res, "%dx%d", &cx, &cy) == 2;
}

static inline bool FormatMatches(VideoFormat left, VideoFormat right)
{
	return left == VideoFormat::Any || right == VideoFormat::Any ||
		left == right;
}

static inline bool ResolutionValid(string res, int& cx, int& cy)
{
	if (!res.size())
		return false;

	return ConvertRes(cx, cy, res.c_str());
}

static inline bool CapsMatch(const VideoInfo&)
{
	return true;
}

template<typename... F> static bool CapsMatch(const VideoDevice& dev, F... fs);

template<typename F, typename... Fs>
static inline bool CapsMatch(const VideoInfo& info, F&& f, Fs... fs)
{
	return f(info) && CapsMatch(info, fs...);
}

template<typename... F> static bool CapsMatch(const VideoDevice& dev, F... fs)
{
	// no early exit, trigger all side effects.
	bool match = false;
	for (const VideoInfo& info : dev.caps)
		if (CapsMatch(info, fs...))
			match = true;
	return match;
}

static inline bool MatcherMatchVideoFormat(VideoFormat format, bool& did_match,
	const VideoInfo& info)
{
	bool match = FormatMatches(format, info.format);
	did_match = did_match || match;
	return match;
}

static inline bool MatcherClosestFrameRateSelector(long long interval,
	long long& best_match,
	const VideoInfo& info)
{
	long long current = FrameRateInterval(info, interval);
	if (llabs(interval - best_match) > llabs(interval - current))
		best_match = current;
	return true;
}

#if 0
auto ResolutionMatcher = [](int cx, int cy)
{
	return [cx, cy](const VideoInfo& info)
	{
		return ResolutionAvailable(info, cx, cy);
	};
};

auto FrameRateMatcher = [](long long interval)
{
	return [interval](const VideoInfo& info)
	{
		return FrameRateAvailable(info, interval);
	};
};

auto VideoFormatMatcher = [](VideoFormat format, bool& did_match)
{
	return [format, &did_match](const VideoInfo& info)
	{
		return MatcherMatchVideoFormat(format, did_match, info);
	};
};

auto ClosestFrameRateSelector = [](long long interval, long long& best_match)
{
	return [interval, &best_match](const VideoInfo& info) mutable -> bool
	{
		MatcherClosestFrameRateSelector(interval, best_match, info);
	};
}
#else
#define ResolutionMatcher(cx, cy)                         \
	[cx, cy](const VideoInfo &info) -> bool {         \
		return ResolutionAvailable(info, cx, cy); \
	}
#define FrameRateMatcher(interval)                         \
	[interval](const VideoInfo &info) -> bool {        \
		return FrameRateAvailable(info, interval); \
	}
#define VideoFormatMatcher(format, did_match)                            \
	[format, &did_match](const VideoInfo &info) mutable -> bool {    \
		return MatcherMatchVideoFormat(format, did_match, info); \
	}
#define ClosestFrameRateSelector(interval, best_match)                       \
	[interval, &best_match](const VideoInfo &info) mutable -> bool {     \
		return MatcherClosestFrameRateSelector(interval, best_match, \
						       info);                \
	}
#endif

static bool ResolutionAvailable(const VideoDevice & dev, int cx, int cy)
{
	return CapsMatch(dev, ResolutionMatcher(cx, cy));
}

static bool DetermineResolution(int& cx, int& cy, obs_data_t* settings,
	VideoDevice dev)
{
	const char* res = obs_data_get_autoselect_string(settings, RESOLUTION);
	if (obs_data_has_autoselect_value(settings, RESOLUTION) &&
		ConvertRes(cx, cy, res) && ResolutionAvailable(dev, cx, cy))
		return true;

	res = obs_data_get_string(settings, RESOLUTION);
	if (ConvertRes(cx, cy, res) && ResolutionAvailable(dev, cx, cy))
		return true;

	res = obs_data_get_string(settings, LAST_RESOLUTION);
	if (ConvertRes(cx, cy, res) && ResolutionAvailable(dev, cx, cy))
		return true;

	return false;
}

static long long GetOBSFPS();

static inline bool IsDelayedDevice(const VideoConfig& config)
{
	return config.format > VideoFormat::MJPEG ||
		wstrstri(config.name.c_str(), L"elgato") != NULL ||
		wstrstri(config.name.c_str(), L"stream engine") != NULL;
}

static inline bool IsDecoupled(const VideoConfig& config)
{
	return wstrstri(config.name.c_str(), L"GV-USB2") != NULL;
}

inline void DShowInput::SetupBuffering(obs_data_t* settings)
{
	BufferingType bufType;
	bool useBuffering;

	bufType = (BufferingType)obs_data_get_int(settings, BUFFERING_VAL);

	if (bufType == BufferingType::Auto)
		useBuffering = IsDelayedDevice(videoConfig);
	else
		useBuffering = bufType == BufferingType::On;

	obs_source_set_async_unbuffered(source, !useBuffering);
	obs_source_set_async_decoupled(source, IsDecoupled(videoConfig));
}

static DStr GetVideoFormatName(VideoFormat format);

bool DShowInput::UpdateVideoConfig(obs_data_t* settings)
{
	string video_device_id = obs_data_get_string(settings, VIDEO_DEVICE_ID);
	deactivateWhenNotShowing = obs_data_get_bool(settings, DEACTIVATE_WNS);
	flip = obs_data_get_bool(settings, FLIP_IMAGE);

	DeviceId id;
	if (!DecodeDeviceId(id, video_device_id.c_str())) {
		blog(LOG_WARNING, "%s: DecodeDeviceId failed",
			obs_source_get_name(source));
		return false;
	}

	PropertiesData data;
	OBSDevice::EnumVideoDevices(data.devices);
	VideoDevice dev;
	if (!data.GetDevice(dev, video_device_id.c_str())) {
		blog(LOG_WARNING, "%s: data.GetDevice failed",
			obs_source_get_name(source));
		return false;
	}

	int resType = (int)obs_data_get_int(settings, RES_TYPE);
	int cx = 0, cy = 0;
	long long interval = 0;
	VideoFormat format = VideoFormat::Any;

	if (resType == ResType_Custom) {
		bool has_autosel_val;
		string resolution = obs_data_get_string(settings, RESOLUTION);
		if (!ResolutionValid(resolution, cx, cy)) {
			blog(LOG_WARNING, "%s: ResolutionValid failed",
				obs_source_get_name(source));
			return false;
		}

		has_autosel_val =
			obs_data_has_autoselect_value(settings, FRAME_INTERVAL);
		interval = has_autosel_val
			? obs_data_get_autoselect_int(settings,
				FRAME_INTERVAL)
			: obs_data_get_int(settings, FRAME_INTERVAL);

		if (interval == FPS_MATCHING)
			interval = GetOBSFPS();

		format = (VideoFormat)obs_data_get_int(settings, VIDEO_FORMAT);

		long long best_interval = numeric_limits<long long>::max();
		bool video_format_match = false;
		bool caps_match = CapsMatch(
			dev, ResolutionMatcher(cx, cy),
			VideoFormatMatcher(format, video_format_match),
			ClosestFrameRateSelector(interval, best_interval),
			FrameRateMatcher(interval));

		if (!caps_match && !video_format_match) {
			blog(LOG_WARNING, "%s: Video format match failed",
				obs_source_get_name(source));
			return false;
		}

		interval = best_interval;
	}

	videoConfig.name = id.name.c_str();
	videoConfig.path = id.path.c_str();
	videoConfig.useDefaultConfig = resType == ResType_Preferred;
	videoConfig.cx = cx;
	videoConfig.cy_abs = abs(cy);
	videoConfig.cy_flip = cy < 0;
	videoConfig.frameInterval = interval;
	videoConfig.internalFormat = format;

	deviceHasAudio = dev.audioAttached;
	deviceHasSeparateAudioFilter = dev.separateAudioFilter;

	videoConfig.callback = std::bind(&DShowInput::OnVideoData, this,
		placeholders::_1, placeholders::_2,
		placeholders::_3, placeholders::_4,
		placeholders::_5, placeholders::_6);

	videoConfig.format = videoConfig.internalFormat;

	if (!device.SetVideoConfig(&videoConfig)) {
		blog(LOG_WARNING, "%s: device.SetVideoConfig failed",
			obs_source_get_name(source));
		return false;
	}

	DStr formatName = GetVideoFormatName(videoConfig.internalFormat);

	double fps = 0.0;

	if (videoConfig.frameInterval)
		fps = 10000000.0 / double(videoConfig.frameInterval);

	BPtr<char> name_utf8;
	BPtr<char> path_utf8;
	os_wcs_to_utf8_ptr(videoConfig.name.c_str(), videoConfig.name.size(),
		&name_utf8);
	os_wcs_to_utf8_ptr(videoConfig.path.c_str(), videoConfig.path.size(),
		&path_utf8);

	blog(LOG_INFO, "---------------------------------");
	blog(LOG_INFO,
		"[DShow Device: '%s'] settings updated: \n"
		"\tvideo device: %s\n"
		"\tvideo path: %s\n"
		"\tresolution: %dx%d\n"
		"\tflip: %d\n"
		"\tfps: %0.2f (interval: %lld)\n"
		"\tformat: %s",
		obs_source_get_name(source), (const char*)name_utf8,
		(const char*)path_utf8, videoConfig.cx, videoConfig.cy_abs,
		(int)videoConfig.cy_flip, fps, videoConfig.frameInterval,
		formatName->array);

	SetupBuffering(settings);

	return true;
}

bool DShowInput::UpdateAudioConfig(obs_data_t* settings)
{
	string audio_device_id = obs_data_get_string(settings, AUDIO_DEVICE_ID);
	bool useCustomAudio = obs_data_get_bool(settings, USE_CUSTOM_AUDIO);

	if (useCustomAudio) {
		DeviceId id;
		if (!DecodeDeviceId(id, audio_device_id.c_str()))
			return false;

		audioConfig.name = id.name.c_str();
		audioConfig.path = id.path.c_str();

	}
	else if (!deviceHasAudio) {
		return true;
	}

	audioConfig.useVideoDevice = !useCustomAudio &&
		!deviceHasSeparateAudioFilter;
	audioConfig.useSeparateAudioFilter = deviceHasSeparateAudioFilter;

	audioConfig.callback = std::bind(&DShowInput::OnAudioData, this,
		placeholders::_1, placeholders::_2,
		placeholders::_3, placeholders::_4,
		placeholders::_5);

	audioConfig.mode =
		(AudioMode)obs_data_get_int(settings, AUDIO_OUTPUT_MODE);

	bool success = device.SetAudioConfig(&audioConfig);
	if (!success)
		return false;

	BPtr<char> name_utf8;
	os_wcs_to_utf8_ptr(audioConfig.name.c_str(), audioConfig.name.size(),
		&name_utf8);

	blog(LOG_INFO, "\tusing video device audio: %s",
		audioConfig.useVideoDevice ? "yes" : "no");

	if (!audioConfig.useVideoDevice) {
		if (audioConfig.useSeparateAudioFilter)
			blog(LOG_INFO, "\tseparate audio filter");
		else
			blog(LOG_INFO, "\taudio device: %s",
				(const char*)name_utf8);
	}

	const char* mode = "";

	switch (audioConfig.mode) {
	case AudioMode::Capture:
		mode = "Capture";
		break;
	case AudioMode::DirectSound:
		mode = "DirectSound";
		break;
	case AudioMode::WaveOut:
		mode = "WaveOut";
		break;
	}

	blog(LOG_INFO,
		"\tsample rate: %d\n"
		"\tchannels: %d\n"
		"\taudio type: %s",
		audioConfig.sampleRate, audioConfig.channels, mode);
	return true;
}

void DShowInput::SetActive(bool active_)
{
	obs_data_t* settings = obs_source_get_settings(source);
	QueueAction(active_ ? Action::Activate : Action::Deactivate);
	obs_data_set_bool(settings, "active", active_);
	obs_data_set_bool(settings, "ActivateSuccess", active_);
	active = active_;
	AiSourceDidAddToScene(false);
	obs_data_release(settings);
}

inline enum video_colorspace
DShowInput::GetColorSpace(obs_data_t* settings) const
{
	const char* space = obs_data_get_string(settings, COLOR_SPACE);

	if (astrcmpi(space, "709") == 0)
		return VIDEO_CS_709;

	if (astrcmpi(space, "601") == 0)
		return VIDEO_CS_601;

	return VIDEO_CS_DEFAULT;
}

inline enum video_range_type
DShowInput::GetColorRange(obs_data_t* settings) const
{
	const char* range = obs_data_get_string(settings, COLOR_RANGE);

	if (astrcmpi(range, "full") == 0)
		return VIDEO_RANGE_FULL;
	if (astrcmpi(range, "partial") == 0)
		return VIDEO_RANGE_PARTIAL;
	return VIDEO_RANGE_DEFAULT;
}

inline bool DShowInput::Activate(obs_data_t* settings)
{
	if (!device.ResetGraph())
		return false;

	if (!UpdateVideoConfig(settings)) {
		blog(LOG_WARNING, "%s: Video configuration failed",
			obs_source_get_name(source));
		return false;
	}

	if (!UpdateAudioConfig(settings))
		blog(LOG_WARNING,
			"%s: Audio configuration failed, ignoring "
			"audio",
			obs_source_get_name(source));

	if (!device.ConnectFilters())
		return false;

	if (device.Start() != Result::Success)
		return false;

	enum video_colorspace cs = GetColorSpace(settings);
	range = GetColorRange(settings);
	frame.range = range;

	bool success = video_format_get_parameters(cs, range,
		frame.color_matrix,
		frame.color_range_min,
		frame.color_range_max);
	if (!success) {
		blog(LOG_ERROR,
			"Failed to get video format parameters for "
			"video format %u",
			cs);
	}
	obs_data_set_bool(settings, "ActivateSuccess", true);
	return true;
}

inline void DShowInput::Deactivate()
{
	device.ResetGraph();
	obs_source_output_video2(source, nullptr);
}

/* ------------------------------------------------------------------------- */

static const char* GetDShowInputName(void*)
{
	return "Lovense AI Camera";
	return TEXT_INPUT_NAME;
}

static void proc_activate(void* data, calldata_t* cd)
{
	bool activate = calldata_bool(cd, "active");
	DShowInput* input = reinterpret_cast<DShowInput*>(data);
	input->SetActive(activate);
}




static void* CreateDShowInput(obs_data_t* settings, obs_source_t* source)
{
	bool force_create = obs_data_get_bool(settings, "force_create");
	if (!force_create && g_dshow_source_count > 0)
		return nullptr;
	g_dshow_source_count = 1;
	DShowInput* dshow = nullptr;
	AiSourceDidAddToScene(true);
	try {
		dshow = new DShowInput(source, settings);
		proc_handler_t* ph = obs_source_get_proc_handler(source);
		proc_handler_add(ph, "void activate(bool active)",
			proc_activate, dshow);
	}
	catch (const char* error) {
		blog(LOG_ERROR, "Could not create device '%s': %s",
			obs_source_get_name(source), error);
	}
	return dshow; 
}

static void DestroyDShowInput(void* data)
{
	AiSourceDidRemoveToScene();
	g_dshow_source_count = 0;
	delete reinterpret_cast<DShowInput*>(data);
}

static void UpdateDShowInput(void* data, obs_data_t* settings)
{
	DShowInput* input = reinterpret_cast<DShowInput*>(data);
	if (input->active)
		input->QueueActivate(settings);
}

static void GetDShowDefaults(obs_data_t* settings)
{
	obs_data_set_default_int(settings, FRAME_INTERVAL, FPS_MATCHING);
	obs_data_set_default_int(settings, RES_TYPE, ResType_Custom);

	obs_data_set_default_int(settings, VIDEO_FORMAT, (int)VideoFormat::Any);
	obs_data_set_default_bool(settings, "active", true);
	obs_data_set_default_string(settings, COLOR_SPACE, "default");
	obs_data_set_default_string(settings, COLOR_RANGE, "default");
	obs_data_set_default_int(settings, AUDIO_OUTPUT_MODE,
		(int)AudioMode::Capture);
	obs_data_set_bool(settings, "ActivateSuccess", false);
}

struct Resolution {
	int cx, cy;

	inline Resolution(int cx, int cy) : cx(cx), cy(cy) {}
};

static void InsertResolution(vector<Resolution>& resolutions, int cx, int cy)
{
	int bestCY = 0;
	size_t idx = 0;

	for (; idx < resolutions.size(); idx++) {
		const Resolution& res = resolutions[idx];
		if (res.cx > cx)
			break;

		if (res.cx == cx) {
			if (res.cy == cy)
				return;

			if (!bestCY)
				bestCY = res.cy;
			else if (res.cy > bestCY)
				break;
		}
	}

	resolutions.insert(resolutions.begin() + idx, Resolution(cx, cy));
}

static inline void AddCap(vector<Resolution>& resolutions, const VideoInfo& cap)
{
	InsertResolution(resolutions, cap.minCX, cap.minCY);
	InsertResolution(resolutions, cap.maxCX, cap.maxCY);
}

#define MAKE_DSHOW_FPS(fps) (10000000LL / (fps))
#define MAKE_DSHOW_FRACTIONAL_FPS(den, num) ((num)*10000000LL / (den))

static long long GetOBSFPS()
{
	obs_video_info ovi;
	if (!obs_get_video_info(&ovi))
		return 0;

	return MAKE_DSHOW_FRACTIONAL_FPS(ovi.fps_num, ovi.fps_den);
}

struct FPSFormat {
	const char* text;
	long long interval;
};

static const FPSFormat validFPSFormats[] = {
	{"60", MAKE_DSHOW_FPS(60)},
	{"59.94 NTSC", MAKE_DSHOW_FRACTIONAL_FPS(60000, 1001)},
	{"50", MAKE_DSHOW_FPS(50)},
	{"48 film", MAKE_DSHOW_FRACTIONAL_FPS(48000, 1001)},
	{"40", MAKE_DSHOW_FPS(40)},
	{"30", MAKE_DSHOW_FPS(30)},
	{"29.97 NTSC", MAKE_DSHOW_FRACTIONAL_FPS(30000, 1001)},
	{"25", MAKE_DSHOW_FPS(25)},
	{"24 film", MAKE_DSHOW_FRACTIONAL_FPS(24000, 1001)},
	{"20", MAKE_DSHOW_FPS(20)},
	{"15", MAKE_DSHOW_FPS(15)},
	{"10", MAKE_DSHOW_FPS(10)},
	{"5", MAKE_DSHOW_FPS(5)},
	{"4", MAKE_DSHOW_FPS(4)},
	{"3", MAKE_DSHOW_FPS(3)},
	{"2", MAKE_DSHOW_FPS(2)},
	{"1", MAKE_DSHOW_FPS(1)},
};

static bool DeviceIntervalChanged(obs_properties_t* props, obs_property_t* p,
	obs_data_t* settings);

static bool TryResolution(VideoDevice& dev, string res)
{
	int cx, cy;
	if (!ConvertRes(cx, cy, res.c_str()))
		return false;

	return ResolutionAvailable(dev, cx, cy);
}

static bool SetResolution(obs_properties_t* props, obs_data_t* settings,
	string res, bool autoselect = false)
{
	if (autoselect)
		obs_data_set_autoselect_string(settings, RESOLUTION,
			res.c_str());
	else
		obs_data_unset_autoselect_value(settings, RESOLUTION);

	DeviceIntervalChanged(props, obs_properties_get(props, FRAME_INTERVAL),
		settings);

	if (!autoselect)
		obs_data_set_string(settings, LAST_RESOLUTION, res.c_str());
	return true;
}

static bool DeviceResolutionChanged(obs_properties_t* props, obs_property_t* p,
	obs_data_t* settings)
{
	UNUSED_PARAMETER(p);

	PropertiesData* data =
		(PropertiesData*)obs_properties_get_param(props);
	const char* id;
	VideoDevice device;

	id = obs_data_get_string(settings, VIDEO_DEVICE_ID);
	string res = obs_data_get_string(settings, RESOLUTION);
	string last_res = obs_data_get_string(settings, LAST_RESOLUTION);

	if (!data->GetDevice(device, id))
		return false;
	if (TryResolution(device, res))
		return SetResolution(props, settings, res);

	if (TryResolution(device, last_res))
		return SetResolution(props, settings, last_res, true);

	return false;
}

struct VideoFormatName {
	VideoFormat format;
	const char* name;
};

static const VideoFormatName videoFormatNames[] = {
	/* autoselect format*/
	{VideoFormat::Any, "VideoFormat.Any"},

	/* raw formats */
	{VideoFormat::ARGB, "ARGB"},
	{VideoFormat::XRGB, "XRGB"},

	/* planar YUV formats */
	{VideoFormat::I420, "I420"},
	{VideoFormat::NV12, "NV12"},
	{VideoFormat::YV12, "YV12"},
	{VideoFormat::Y800, "Y800"},

	/* packed YUV formats */
	{VideoFormat::YVYU, "YVYU"},
	{VideoFormat::YUY2, "YUY2"},
	{VideoFormat::UYVY, "UYVY"},
	{VideoFormat::HDYC, "HDYC"},

	/* encoded formats */
	{VideoFormat::MJPEG, "MJPEG"},
	{VideoFormat::H264, "H264"} };

static bool ResTypeChanged(obs_properties_t* props, obs_property_t* p,
	obs_data_t* settings);

static size_t AddDevice(obs_property_t* device_list, const string& id)
{
	DStr name, path;
	if (!DecodeDeviceDStr(name, path, id.c_str()))
		return numeric_limits<size_t>::max();

	return obs_property_list_add_string(device_list, name, id.c_str());
}

static bool UpdateDeviceList(obs_property_t* list, const string& id)
{
	size_t size = obs_property_list_item_count(list);
	bool found = false;
	bool disabled_unknown_found = false;

	for (size_t i = 0; i < size; i++) {
		if (obs_property_list_item_string(list, i) == id) {
			found = true;
			continue;
		}
		if (obs_property_list_item_disabled(list, i))
			disabled_unknown_found = true;
	}

	if (!found && !disabled_unknown_found) {
		size_t idx = AddDevice(list, id);
		obs_property_list_item_disable(list, idx, true);
		return true;
	}

	if (found && !disabled_unknown_found)
		return false;

	for (size_t i = 0; i < size;) {
		if (obs_property_list_item_disabled(list, i)) {
			obs_property_list_item_remove(list, i);
			continue;
		}
		i += 1;
	}

	return true;
}

static bool DeviceSelectionChanged(obs_properties_t* props, obs_property_t* p,
	obs_data_t* settings)
{
	PropertiesData* data =
		(PropertiesData*)obs_properties_get_param(props);
	VideoDevice device;

	string id = obs_data_get_string(settings, VIDEO_DEVICE_ID);
	string old_id = obs_data_get_string(settings, LAST_VIDEO_DEV_ID);

	bool device_list_updated = UpdateDeviceList(p, id);

	if (!data->GetDevice(device, id.c_str()))
		return !device_list_updated;

	vector<Resolution> resolutions;
	for (const VideoInfo& cap : device.caps)
	{
		AddCap(resolutions, cap);
	}

	p = obs_properties_get(props, RESOLUTION);
	obs_property_list_clear(p);

	config_t* config = obs_frontend_get_profile_config();
	uint32_t nx = config_get_uint(config, "Video", "BaseCX");
	uint32_t ny = config_get_uint(config, "Video", "BaseCY");
	bool machVideoSize = false;
	string machRes;
	for (size_t idx = resolutions.size(); idx > 0; idx--) {
		const Resolution& res = resolutions[idx - 1];

		string strRes;
		strRes += to_string(res.cx);
		strRes += "x";
		strRes += to_string(res.cy);
		if (res.cx == nx && ny == res.cy)
		{
			machVideoSize = true;
			machRes = strRes;
		}
		else
			obs_property_list_add_string(p, strRes.c_str(), strRes.c_str());
	}
	if(machVideoSize)
		obs_property_list_insert_string(p,0, machRes.c_str(), machRes.c_str());


	/* only refresh properties if device legitimately changed */
	if (!id.size() || !old_id.size() || id != old_id) {
		p = obs_properties_get(props, RES_TYPE);
		ResTypeChanged(props, p, settings);
		obs_data_set_string(settings, LAST_VIDEO_DEV_ID, id.c_str());
	}

	return true;
}

static bool VideoConfigClicked(obs_properties_t* props, obs_property_t* p,
	void* data)
{
	DShowInput* input = reinterpret_cast<DShowInput*>(data);
	input->QueueAction(Action::ConfigVideo);

	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(p);
	return false;
}

/*static bool AudioConfigClicked(obs_properties_t *props, obs_property_t *p,
		void *data)
{
	DShowInput *input = reinterpret_cast<DShowInput*>(data);
	input->QueueAction(Action::ConfigAudio);

	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(p);
	return false;
}*/

static bool CrossbarConfigClicked(obs_properties_t* props, obs_property_t* p,
	void* data)
{
	DShowInput* input = reinterpret_cast<DShowInput*>(data);
	input->QueueAction(Action::ConfigCrossbar1);

	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(p);
	return false;
}

/*static bool Crossbar2ConfigClicked(obs_properties_t *props, obs_property_t *p,
		void *data)
{
	DShowInput *input = reinterpret_cast<DShowInput*>(data);
	input->QueueAction(Action::ConfigCrossbar2);

	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(p);
	return false;
}*/

static bool AddDevice(obs_property_t* device_list, const VideoDevice& device, bool insertFront = false)
{
	DStr name, path, device_id;

	dstr_from_wcs(name, device.name.c_str());
	dstr_from_wcs(path, device.path.c_str());

	encode_dstr(path);

	dstr_copy_dstr(device_id, name);
	encode_dstr(device_id);
	dstr_cat(device_id, ":");
	dstr_cat_dstr(device_id, path);

	if (insertFront)
	{
		obs_property_list_insert_string(device_list,0, name, device_id);
	}
	else
		obs_property_list_add_string(device_list, name, device_id);

	return true;
}

static bool AddAudioDevice(obs_property_t* device_list,
	const AudioDevice& device)
{
	DStr name, path, device_id;

	dstr_from_wcs(name, device.name.c_str());
	dstr_from_wcs(path, device.path.c_str());

	encode_dstr(path);

	dstr_copy_dstr(device_id, name);
	encode_dstr(device_id);
	dstr_cat(device_id, ":");
	dstr_cat_dstr(device_id, path);

	obs_property_list_add_string(device_list, name, device_id);

	return true;
}

static void PropertiesDataDestroy(void* data)
{
	delete reinterpret_cast<PropertiesData*>(data);
}

static bool ResTypeChanged(obs_properties_t* props, obs_property_t* p,
	obs_data_t* settings)
{
	int val = (int)obs_data_get_int(settings, RES_TYPE);
	bool enabled = (val != ResType_Preferred);

	p = obs_properties_get(props, RESOLUTION);
	obs_property_set_enabled(p, enabled);

	p = obs_properties_get(props, FRAME_INTERVAL);
	obs_property_set_enabled(p, enabled);

	p = obs_properties_get(props, VIDEO_FORMAT);
	obs_property_set_enabled(p, enabled);

	if (val == ResType_Custom) {
		p = obs_properties_get(props, RESOLUTION);
		DeviceResolutionChanged(props, p, settings);
	}
	else {
		obs_data_unset_autoselect_value(settings, FRAME_INTERVAL);
	}

	return true;
}

static DStr GetFPSName(long long interval)
{
	DStr name;

	if (interval == FPS_MATCHING) {
		dstr_cat(name, TEXT_FPS_MATCHING);
		return name;
	}

	if (interval == FPS_HIGHEST) {
		dstr_cat(name, TEXT_FPS_HIGHEST);
		return name;
	}

	for (const FPSFormat& format : validFPSFormats) {
		if (format.interval != interval)
			continue;

		dstr_cat(name, format.text);
		return name;
	}

	dstr_cat(name, to_string(10000000. / interval).c_str());
	return name;
}

static void UpdateFPS(VideoDevice& device, VideoFormat format,
	long long interval, int cx, int cy,
	obs_properties_t* props)
{
	obs_property_t* list = obs_properties_get(props, FRAME_INTERVAL);

	obs_property_list_clear(list);

	obs_property_list_add_int(list, TEXT_FPS_MATCHING, FPS_MATCHING);
	obs_property_list_add_int(list, TEXT_FPS_HIGHEST, FPS_HIGHEST);

	bool interval_added = interval == FPS_HIGHEST ||
		interval == FPS_MATCHING;
	for (const FPSFormat& fps_format : validFPSFormats) {
		bool video_format_match = false;
		long long format_interval = fps_format.interval;

		bool available = CapsMatch(
			device, ResolutionMatcher(cx, cy),
			VideoFormatMatcher(format, video_format_match),
			FrameRateMatcher(format_interval));

		if (!available && interval != fps_format.interval)
			continue;

		if (interval == fps_format.interval)
			interval_added = true;

		size_t idx = obs_property_list_add_int(list, fps_format.text,
			fps_format.interval);
		obs_property_list_item_disable(list, idx, !available);
	}

	if (interval_added)
		return;

	size_t idx =
		obs_property_list_add_int(list, GetFPSName(interval), interval);
	obs_property_list_item_disable(list, idx, true);
}

static DStr GetVideoFormatName(VideoFormat format)
{
	DStr name;
	for (const VideoFormatName& format_ : videoFormatNames) {
		if (format_.format == format) {
			dstr_cat(name, obs_module_text(format_.name));
			return name;
		}
	}

	dstr_cat(name, TEXT_FORMAT_UNKNOWN);
	dstr_replace(name, "%1", std::to_string((long long)format).c_str());
	return name;
}

static void UpdateVideoFormats(VideoDevice& device, VideoFormat format_, int cx,
	int cy, long long interval,
	obs_properties_t* props)
{
	set<VideoFormat> formats = { VideoFormat::Any };
	auto format_gatherer =
		[&formats](const VideoInfo& info) mutable -> bool {
		formats.insert(info.format);
		return false;
	};

	CapsMatch(device, ResolutionMatcher(cx, cy), FrameRateMatcher(interval),
		format_gatherer);

	obs_property_t* list = obs_properties_get(props, VIDEO_FORMAT);
	obs_property_list_clear(list);

	bool format_added = false;
	for (const VideoFormatName& format : videoFormatNames) {
		bool available = formats.find(format.format) != end(formats);

		if (!available && format.format != format_)
			continue;

		if (format.format == format_)
			format_added = true;

		size_t idx = obs_property_list_add_int(
			list, obs_module_text(format.name),
			(long long)format.format);
		obs_property_list_item_disable(list, idx, !available);
	}

	if (format_added)
		return;

	size_t idx = obs_property_list_add_int(
		list, GetVideoFormatName(format_), (long long)format_);
	obs_property_list_item_disable(list, idx, true);
}

static bool UpdateFPS(long long interval, obs_property_t* list)
{
	size_t size = obs_property_list_item_count(list);
	DStr name;

	for (size_t i = 0; i < size; i++) {
		if (obs_property_list_item_int(list, i) != interval)
			continue;

		obs_property_list_item_disable(list, i, true);
		if (size == 1)
			return false;

		dstr_cat(name, obs_property_list_item_name(list, i));
		break;
	}

	obs_property_list_clear(list);

	if (!name->len)
		name = GetFPSName(interval);

	obs_property_list_add_int(list, name, interval);
	obs_property_list_item_disable(list, 0, true);

	return true;
}

static bool DeviceIntervalChanged(obs_properties_t* props, obs_property_t* p,
	obs_data_t* settings)
{
	long long val = obs_data_get_int(settings, FRAME_INTERVAL);

	PropertiesData* data =
		(PropertiesData*)obs_properties_get_param(props);
	const char* id = obs_data_get_string(settings, VIDEO_DEVICE_ID);
	VideoDevice device;

	if (!data->GetDevice(device, id))
		return UpdateFPS(val, p);

	int cx = 0, cy = 0;
	if (!DetermineResolution(cx, cy, settings, device)) {
		UpdateVideoFormats(device, VideoFormat::Any, 0, 0, 0, props);
		UpdateFPS(device, VideoFormat::Any, 0, 0, 0, props);
		return true;
	}

	int resType = (int)obs_data_get_int(settings, RES_TYPE);
	if (resType != ResType_Custom)
		return true;

	if (val == FPS_MATCHING)
		val = GetOBSFPS();

	VideoFormat format =
		(VideoFormat)obs_data_get_int(settings, VIDEO_FORMAT);

	bool video_format_matches = false;
	long long best_interval = numeric_limits<long long>::max();
	bool frameRateSupported =
		CapsMatch(device, ResolutionMatcher(cx, cy),
			VideoFormatMatcher(format, video_format_matches),
			ClosestFrameRateSelector(val, best_interval),
			FrameRateMatcher(val));

	if (video_format_matches && !frameRateSupported &&
		best_interval != val) {
		long long listed_val = 0;
		for (const FPSFormat& format : validFPSFormats) {
			long long diff = llabs(format.interval - best_interval);
			if (diff < DEVICE_INTERVAL_DIFF_LIMIT) {
				listed_val = format.interval;
				break;
			}
		}

		if (listed_val != val) {
			obs_data_set_autoselect_int(settings, FRAME_INTERVAL,
				listed_val);
			val = listed_val;
		}

	}
	else {
		obs_data_unset_autoselect_value(settings, FRAME_INTERVAL);
	}

	UpdateVideoFormats(device, format, cx, cy, val, props);
	UpdateFPS(device, format, val, cx, cy, props);

	UNUSED_PARAMETER(p);
	return true;
}

static bool UpdateVideoFormats(VideoFormat format, obs_property_t* list)
{
	size_t size = obs_property_list_item_count(list);
	DStr name;

	for (size_t i = 0; i < size; i++) {
		if ((VideoFormat)obs_property_list_item_int(list, i) != format)
			continue;

		if (size == 1)
			return false;

		dstr_cat(name, obs_property_list_item_name(list, i));
		break;
	}

	obs_property_list_clear(list);

	if (!name->len)
		name = GetVideoFormatName(format);

	obs_property_list_add_int(list, name, (long long)format);
	obs_property_list_item_disable(list, 0, true);

	return true;
}

static bool VideoFormatChanged(obs_properties_t* props, obs_property_t* p,
	obs_data_t* settings)
{
	PropertiesData* data =
		(PropertiesData*)obs_properties_get_param(props);
	const char* id = obs_data_get_string(settings, VIDEO_DEVICE_ID);
	VideoDevice device;

	VideoFormat curFormat =
		(VideoFormat)obs_data_get_int(settings, VIDEO_FORMAT);

	if (!data->GetDevice(device, id))
		return UpdateVideoFormats(curFormat, p);

	int cx, cy;
	if (!DetermineResolution(cx, cy, settings, device)) {
		UpdateVideoFormats(device, VideoFormat::Any, cx, cy, 0, props);
		UpdateFPS(device, VideoFormat::Any, 0, 0, 0, props);
		return true;
	}

	long long interval = obs_data_get_int(settings, FRAME_INTERVAL);

	UpdateVideoFormats(device, curFormat, cx, cy, interval, props);
	UpdateFPS(device, curFormat, interval, cx, cy, props);
	return true;
}

static bool CustomAudioClicked(obs_properties_t* props, obs_property_t* p,
	obs_data_t* settings)
{
	bool useCustomAudio = obs_data_get_bool(settings, USE_CUSTOM_AUDIO);
	p = obs_properties_get(props, AUDIO_DEVICE_ID);
	obs_property_set_visible(p, useCustomAudio);
	return true;
}

static bool ActivateClicked(obs_properties_t*, obs_property_t* p, void* data)
{
	DShowInput* input = reinterpret_cast<DShowInput*>(data);

	if (input->active) {
		input->SetActive(false);
		obs_property_set_description(p, TEXT_ACTIVATE);
	}
	else {
		input->SetActive(true);
		obs_property_set_description(p, TEXT_DEACTIVATE);
	}

	return true;
}

static bool CheckIsPhysicalCameraID(const wchar_t* szID) {
	if (!szID || wcslen(szID) == 0)
		return false;
	std::wstring strID = szID;
	if (strID.find(L"usb") != std::wstring::npos)
		return true;
	return false;
}

static obs_properties_t* GetDShowProperties(void* obj)
{
	DShowInput* input = reinterpret_cast<DShowInput*>(obj);
	obs_properties_t* ppts = obs_properties_create();
	PropertiesData* data = new PropertiesData;

	data->input = input;

	obs_properties_set_param(ppts, data, PropertiesDataDestroy);

	obs_property_t* p = obs_properties_add_list(ppts, VIDEO_DEVICE_ID,
		TEXT_DEVICE,
		OBS_COMBO_TYPE_LIST,
		OBS_COMBO_FORMAT_STRING);

	obs_property_set_modified_callback(p, DeviceSelectionChanged);

	OBSDevice::EnumVideoDevices(data->devices);
	for (const VideoDevice& device : data->devices)
	{
		if(CheckIsPhysicalCameraID(device.path.c_str()))
			AddDevice(p, device,true);
		else
			AddDevice(p, device);
	}

	const char* activateText = TEXT_ACTIVATE;
	if (input) {
		if (input->active)
			activateText = TEXT_DEACTIVATE;
	}

	obs_properties_add_button(ppts, "activate", activateText,
		ActivateClicked);
	obs_properties_add_button(ppts, "video_config", TEXT_CONFIG_VIDEO,
		VideoConfigClicked);
	obs_properties_add_button(ppts, "xbar_config", TEXT_CONFIG_XBAR,
		CrossbarConfigClicked);

	obs_properties_add_bool(ppts, DEACTIVATE_WNS, TEXT_DWNS);

	/* ------------------------------------- */
	/* video settings */

	p = obs_properties_add_list(ppts, RES_TYPE, TEXT_RES_FPS_TYPE,
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);

	obs_property_set_modified_callback(p, ResTypeChanged);

	obs_property_list_add_int(p, TEXT_PREFERRED_RES, ResType_Preferred);
	obs_property_list_add_int(p, TEXT_CUSTOM_RES, ResType_Custom);

	p = obs_properties_add_list(ppts, RESOLUTION, TEXT_RESOLUTION,
		OBS_COMBO_TYPE_LIST,
		OBS_COMBO_FORMAT_STRING);
	
	obs_property_set_modified_callback(p, DeviceResolutionChanged);

	p = obs_properties_add_list(ppts, FRAME_INTERVAL, "FPS",
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);

	obs_property_set_modified_callback(p, DeviceIntervalChanged);

	p = obs_properties_add_list(ppts, VIDEO_FORMAT, TEXT_VIDEO_FORMAT,
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);

	obs_property_set_modified_callback(p, VideoFormatChanged);

	p = obs_properties_add_list(ppts, COLOR_SPACE, TEXT_COLOR_SPACE,
		OBS_COMBO_TYPE_LIST,
		OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(p, TEXT_COLOR_DEFAULT, "default");
	obs_property_list_add_string(p, "709", "709");
	obs_property_list_add_string(p, "601", "601");

	p = obs_properties_add_list(ppts, COLOR_RANGE, TEXT_COLOR_RANGE,
		OBS_COMBO_TYPE_LIST,
		OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(p, TEXT_RANGE_DEFAULT, "default");
	obs_property_list_add_string(p, TEXT_RANGE_PARTIAL, "partial");
	obs_property_list_add_string(p, TEXT_RANGE_FULL, "full");

	p = obs_properties_add_list(ppts, BUFFERING_VAL, TEXT_BUFFERING,
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, TEXT_BUFFERING_AUTO,
		(int64_t)BufferingType::Auto);
	obs_property_list_add_int(p, TEXT_BUFFERING_ON,
		(int64_t)BufferingType::On);
	obs_property_list_add_int(p, TEXT_BUFFERING_OFF,
		(int64_t)BufferingType::Off);

	obs_property_set_long_description(p,
		obs_module_text("Buffering.ToolTip"));

	obs_properties_add_bool(ppts, FLIP_IMAGE, TEXT_FLIP_IMAGE);

	/* ------------------------------------- */
	/* audio settings */

	OBSDevice::EnumAudioDevices(data->audioDevices);

	p = obs_properties_add_list(ppts, AUDIO_OUTPUT_MODE, TEXT_AUDIO_MODE,
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, TEXT_MODE_CAPTURE,
		(int64_t)AudioMode::Capture);
	obs_property_list_add_int(p, TEXT_MODE_DSOUND,
		(int64_t)AudioMode::DirectSound);
	obs_property_list_add_int(p, TEXT_MODE_WAVEOUT,
		(int64_t)AudioMode::WaveOut);

	if (!data->audioDevices.size())
		return ppts;

	p = obs_properties_add_bool(ppts, USE_CUSTOM_AUDIO, TEXT_CUSTOM_AUDIO);

	obs_property_set_modified_callback(p, CustomAudioClicked);

	p = obs_properties_add_list(ppts, AUDIO_DEVICE_ID, TEXT_AUDIO_DEVICE,
		OBS_COMBO_TYPE_LIST,
		OBS_COMBO_FORMAT_STRING);

	for (const AudioDevice& device : data->audioDevices)
		AddAudioDevice(p, device);

	OBSVideoAdapter::TransfromStretchToScreen();
	OBSVideoAdapter::MoveFeedbackSourceToTop();
	return ppts;
}

void DShowModuleLogCallback(LogType type, const wchar_t* msg, void* param)
{
	int obs_type = LOG_DEBUG;

	switch (type) {
	case LogType::Error:
		obs_type = LOG_ERROR;
		break;
	case LogType::Warning:
		obs_type = LOG_WARNING;
		break;
	case LogType::Info:
		obs_type = LOG_INFO;
		break;
	case LogType::Debug:
		obs_type = LOG_DEBUG;
		break;
	}

	DStr dmsg;

	dstr_from_wcs(dmsg, msg);
	blog(obs_type, "DShow: %s", dmsg->array);

	UNUSED_PARAMETER(param);
}

static void HideDShowInput(void* data)
{
	DShowInput* input = reinterpret_cast<DShowInput*>(data);

	if (input->deactivateWhenNotShowing && input->active)
		input->QueueAction(Action::Deactivate);
}

static void ShowDShowInput(void* data)
{
	DShowInput* input = reinterpret_cast<DShowInput*>(data);

	if (input->deactivateWhenNotShowing && input->active)
		input->QueueAction(Action::Activate);
}

static uint32_t GetWidth(void* data)
{
	config_t* config = obs_frontend_get_profile_config();
	uint32_t nx = config_get_uint(config, "Video", "BaseCX");
	return nx;
}

static uint32_t GetHeight(void* data)
{
	config_t* config = obs_frontend_get_profile_config();
	uint32_t ny = config_get_uint(config, "Video", "BaseCY");
	return ny;
}

void RegisterDShowSource()
{
	SetLogCallback(DShowModuleLogCallback, nullptr);
	obs_source_info info = {};
	info.id = "lovense_dshow_input";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_AUDIO |
		OBS_SOURCE_ASYNC | OBS_SOURCE_DO_NOT_DUPLICATE;
	info.show = ShowDShowInput;
	info.hide = HideDShowInput;
	info.get_name = GetDShowInputName;
	info.create = CreateDShowInput;
	info.destroy = DestroyDShowInput;
	info.update = UpdateDShowInput;
	info.get_defaults = GetDShowDefaults;
	info.get_properties = GetDShowProperties;
	info.icon_type = OBS_ICON_TYPE_CAMERA;
	//info.get_width = GetWidth;
	//info.get_height = GetHeight;
	obs_register_source(&info);
}
