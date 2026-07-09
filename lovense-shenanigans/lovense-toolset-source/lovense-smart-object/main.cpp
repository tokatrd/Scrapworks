/*
Plugin Name
Copyright (C) 2021 Roy Shilkrot roy.shil@gmail.com

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include <obs-module.h>
#include "obs-frontend-api.h"

#include "so-common.hpp"
#include "so-proc-handler.hpp"
#include "so-server-handler.hpp"
#include "so-detection-center.hpp"

using namespace lovense;

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")
OBS_MODULE_AUTHOR("lovense")

MODULE_EXPORT const char *obs_module_description(void)
{
  return  "lovense smart object recognition";
}

namespace lovense::so {
	extern void RegisterFilter();
}



bool obs_module_load(void)
{
  lovense::so::RegisterFilter();
  so::setup_proc_hander();
  so_info("plugin loaded successfully");

  obs_frontend_add_event_callback([](enum obs_frontend_event event, void* private_data) {
	  
		switch (event)
		{
		case OBS_FRONTEND_EVENT_EXIT:
		{
			so::DetectionCenter::instance()->shutdown();
			break;
		}
		case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		{
			so::DetectionCenter::instance()->initialize();
			/*so::ServerHandler::instance()->initialize();

			if (LvsGreenScreen::getInstance()->getStatus()) {
				so::start_render_media();
			}			*/
			break;
		}
	  
		default:
			break;
		}
  }, nullptr);
  return true;
}

void obs_module_unload()
{
  blog(LOG_INFO, "plugin unloaded");
}

