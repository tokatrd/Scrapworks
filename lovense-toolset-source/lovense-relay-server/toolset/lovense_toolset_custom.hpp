#pragma once


namespace toolset
{
	bool initialize();
	void on_load_end();

#ifdef _WIN32
	void check_low_version_plugin();
#endif // _WIN32

	
}
	

