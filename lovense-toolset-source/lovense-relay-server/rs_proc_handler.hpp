#pragma once

#include "json11/json11.hpp"

namespace lovense
{
	namespace rs
	{
		void setup_proc_hander();

		void full_module_switch_values(json11::Json::object& values);
	}
}
