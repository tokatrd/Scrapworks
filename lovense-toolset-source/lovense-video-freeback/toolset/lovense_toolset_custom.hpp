#pragma once

#include <shlwapi.h>

#ifdef _WIN32
#pragma comment(lib,"shlwapi.lib")
#pragma comment(lib,"DXGI.lib")
#endif

#include <obs-frontend-api.h>

#include "zip/unzip.h"
#include "zip/zip.h"


namespace lovense::toolset
{
	bool initialize();
	void on_load_end();

	void on_obs_frontend_event(enum obs_frontend_event event, void*);

	bool is_valid_obs();

	std::wstring string2wstring(std::string str);
	std::string  wstring2string(std::wstring wstr);

	void ReplaceChar(char* pStr, char cDest, char cSrc);
	std::string FilePathToHtmlPath(const char* file);

	bool lovense_plugins_is_setup();
	void setup_lovense_plugin_config();
}
	

