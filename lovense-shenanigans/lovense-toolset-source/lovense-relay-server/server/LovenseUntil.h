#pragma once
#ifndef __LOVENSE_UNTIL_H__
#define __LOVENSE_UNTIL_H__
#include <string>
#include <memory>


#include "lovense_websocket_common.hpp"
#include "lovense_camera_source.hpp"


using namespace lovense;

class LovenseUntil
{
public:	
	static std::shared_ptr<CameraInfo> GetCurrentSceneCameraInfo();
};

namespace lovense
{
	namespace ws {
		void DebugLog(const std::string& log);

#ifdef WIN32
		bool  executeCommandLine(std::wstring cmdLine, DWORD& exitCode);
#endif

#ifdef DEF_TOOLSETS
		unsigned short GetWsAvailablePort(bool reCheck = false);
		unsigned short GetWSSAvailablePort(bool reCheck = false);
#endif

		void OnProcReceivedData(ws::WsocketType socket_t, calldata_t* data, int clientType, const char* name);
	}

	bool UpdateOBSRecordStatus(bool bStart);
	std::string GetOBSRecordDirectory();
	bool GetOBSRecordStatus();
}
#endif //__LOVENSE_UNTIL_H__

