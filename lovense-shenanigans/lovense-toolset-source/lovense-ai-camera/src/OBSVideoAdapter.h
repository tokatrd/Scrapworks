#pragma once
#include<map>
#include<string>
#include<vector>
#include "obs.h"

namespace OBSVideoAdapter
{
	// Get the header 16:9 4:3 resolution string
	enum Ratio{
		RATIO_16_9 = 0,
		RATIO_4_3,
	};

	enum CameraSourceType {
		CAMERA_WITH_OBS_SOURCE = 2 << 0,
		CAMERA_WITH_LOVENSE_SOURCE =  2 << 1,
	};

	bool IsLovenseAICameraSource( obs_source_t* source);
	bool GetCameraResolution( obs_source_t *source, std::map<Ratio, std::vector<std::string>>& resolution);
	bool ResetCameraResolution( obs_source_t* source, const std::string& resoulution);
	void SetCameraResolutionMatchCanvas( obs_source_t* source);
	void ConnectSourceSignals();
	void DisconnectSourceSignals();

	//map<name,resolution>
	void EnumCameraDevice(const std::wstring &device_name, std::map<Ratio, std::vector<std::string>>& resolution);
	void TransfromStretchToScreen();
	void TransfromFixToScreen();
	void MoveFeedbackSourceToTop();

	// Camera device list, device id
	obs_scene_t *FindLovenseScene(); 
	obs_scene_t *CreateLovenseScene();
	bool RemoveScene(obs_scene_t *scene);


	// Get the physical camera list
	std::vector<std::string> GetPhysicalCameraList(); 

	std::vector<std::string> GetNotActivedCameraList();

	bool HasDshowSource(obs_scene_t *scene);

	// Active the current scene camera
	bool ActiveCurrentSceneCamera(obs_scene_t *current_scene);

	obs_source_t* AddVideoFeedBackSource(obs_scene_t* scene);

	bool CheckSceneAiConfigValid(obs_source_t * scenesource, bool &hasDshow, bool &hasFeedback);
};

