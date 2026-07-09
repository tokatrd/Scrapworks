#pragma once

namespace lovense
{

#define rs_info(msg, ...) blog(LOG_INFO, "[lovense:relay-server] " msg, ##__VA_ARGS__)
#define rs_warn(msg, ...) blog(LOG_WARNING, "[lovense:relay-server] " msg, ##__VA_ARGS__)
#define rs_error(msg, ...) blog(LOG_ERROR, "[lovense:relay-server] " msg, ##__VA_ARGS__)
#define rs_debug(msg, ...) blog(LOG_DEBUG, "[lovense:relay-server] " msg, ##__VA_ARGS__)

	namespace rs
	{
		static const char* VERSION = "1.0.0";


	}
}
