#pragma once

#include "obs.hpp"

namespace lovense::so
{
	static const char* FILTER_ID   = "lovense-smart-object-filter";	
	static const char* MODULE_NAME = "lovense_smart_object";


#define so_log(level, msg, ...) blog(level, "[lovense: smart-object] " msg, ##__VA_ARGS__)
#define so_info(msg, ...)       blog(LOG_INFO, "[lovense: smart-object] " msg, ##__VA_ARGS__)
#define so_warn(msg, ...)       blog(LOG_WARNING, "[lovense: smart-object] " msg, ##__VA_ARGS__)
#define so_error(msg, ...)      blog(LOG_ERROR, "[lovense: smart-object] " msg, ##__VA_ARGS__)
#define so_debug(msg, ...)      blog(LOG_DEBUG, "[lovense: smart-object] " msg, ##__VA_ARGS__)
}
