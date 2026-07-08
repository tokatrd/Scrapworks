#ifndef __UNTIL_HELP__
#include <string>
#include <vector>
#include <obs.h>
#include <map>
namespace UntilRes {
	enum Ratio {
		RATIO_16_9 = 0,
		RATIO_4_3  = 1,
		RATIO_OTHERS = 2,
	};

	enum LiveMode {
		LiveMode_Horizontal, // Set to landscape mode
		LiveMode_Vertical, // Set to portrait mode
	};

	class CameraBaseInfo {
	public:
		std::string name;
		std::string path;
		std::string resolution;
	};

	std::string GetResourceImage(std::string name);
	size_t GetCameraResolution(obs_source_t* source, std::map<Ratio, std::vector<std::string>>& resolution);
	bool ResetCameraResolution(obs_source_t* source, const std::string& resoulution);
	CameraBaseInfo GetCameraSourceBaseInfo(obs_source_t* source);
	obs_source_t* FindCurrentDshowSource();
	void TransfromFixToScreen(obs_scene_t* scene, obs_source_t* source);
	void TransfromSizeToScreen(obs_scene_t* scene, obs_source_t* source);
	void ActiveCameraDeviceSource(obs_source_t* source);
	void SetPluginPath(std::string path);
	std::string GetPluginPath();


	std::string GetResolutionTypeString(size_t x, size_t y);

	// Switch to portrait resolution
#if 0
	bool OBSSourceScreenResolution(bool vertical);
#endif
	LiveMode GetLiveMode();
	bool SetLiveMode(LiveMode mode);
}


#endif
