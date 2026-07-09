#pragma once

#include <string>


namespace lovense
{
	namespace rs
	{
		std::string get_app_token();

#if 0
		std::string GetDistHtmlVersion();
#endif

		bool get_version(std::string& dist_version, std::string& toolset_version, bool ignore_cache=false);

		void report_obs_info();
		void report_extra_modules();
		void report_profilter_inited();

		int select_camera(std::string camera_name);
	}
}
