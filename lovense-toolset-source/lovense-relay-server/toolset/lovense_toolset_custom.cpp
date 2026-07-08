#include <filesystem>

#include "obs.hpp"
#include "util/dstr.hpp"
#include "util/platform.h"
#include "obs-module.h"

#ifdef _WIN32
#include <process.h>
#include <Shlobj.h>
#endif

#include "lovense_string_helper.hpp"
#include "lovense_util_helper.hpp"
#include "lovense_toolset_custom.hpp"
#include "lovense_obs_common.hpp"

namespace toolset {

	using namespace std;
	using namespace lovense;

	bool initialize()
	{
#if _WIN32
#if (LIBOBS_API_VER < MAKE_SEMANTIC_VERSION(28, 0, 0)) 
			check_low_version_plugin();
#endif	
#endif
		return true;
	}

	void on_load_end()
	{
		
	}

#ifdef _WIN32

	static uint32_t get_obs_version_from_regedit()
	{
		//Check if OBS is installed
		const HKEY hkey = HKEY_LOCAL_MACHINE;
		HKEY  key_handle;
		DWORD Type = REG_SZ;

#if ARCH_BITS == 64
		std::string path = "SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\OBS Studio";
#else
		std::string path = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\OBS Studio";
#endif
		const char* key = "DisplayVersion";



		auto  version = std::string(1024, 0);
		DWORD size = version.size();
		if (RegOpenKeyExA(hkey, path.c_str(), 0
			, KEY_QUERY_VALUE, &key_handle) != ERROR_SUCCESS)
		{
			return 0;
		}

		if (RegQueryValueExA(key_handle, key, NULL, &Type, (LPBYTE)&version[0], &size) != ERROR_SUCCESS)
		{
			RegCloseKey(key_handle);
			return 0;
		}
		RegCloseKey(key_handle);
	
		if (size == 0) {
			return 0;
		}

		version.resize(size);

		std::vector<std::string> v = string_util::split(version, ".");
		if (v.size() < 3)
		{
			return 0;
		}

		int major = atoi(v[0].c_str());
		int minor = atoi(v[1].c_str());
		int patch = atoi(v[2].c_str());
		if (major < 24) return 0;

		return MAKE_SEMANTIC_VERSION(major, minor, patch);
	}

	static bool  executeCommandLine(std::wstring cmdLine, DWORD& exitCode)
	{
		PROCESS_INFORMATION processInformation = { 0 };
		STARTUPINFO startupInfo = { 0 };
		startupInfo.cb = sizeof(startupInfo);
		int nStrBuffer = cmdLine.size() + 50;

		// Create the process

		BOOL result = CreateProcess(NULL, (LPWSTR)cmdLine.data(),
			NULL, NULL, FALSE,
			NORMAL_PRIORITY_CLASS | CREATE_NO_WINDOW,
			NULL, NULL, &startupInfo, &processInformation);


		if (!result)
		{
			// CreateProcess() failed
			// Get the error from the system
			LPVOID lpMsgBuf;
			DWORD dw = GetLastError();
			FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);

			// Display the error

			// Free resources created by the system
			LocalFree(lpMsgBuf);

			// We failed.
			return FALSE;
		}
		else
		{
			HANDLE hToken;
			TOKEN_PRIVILEGES tkp;

			STARTUPINFO si;
			PROCESS_INFORMATION pi;

			ZeroMemory(&si, sizeof(si));
			si.cb = sizeof(si);
			ZeroMemory(&pi, sizeof(pi));

			if (OpenProcessToken(pi.hProcess, TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
			{
				LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &tkp.Privileges[0].Luid);
				tkp.PrivilegeCount = 1;
				tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
				
				AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, (PTOKEN_PRIVILEGES)NULL, 0);

				CloseHandle(hToken);
			}
			// Successfully created the process.  Wait for it to finish.
			WaitForSingleObject(processInformation.hProcess, INFINITE);

			// Get the exit code.
			result = GetExitCodeProcess(processInformation.hProcess, &exitCode);

			// Close the handles.
			CloseHandle(processInformation.hProcess);
			CloseHandle(processInformation.hThread);

			if (!result)
			{
				// Could not get exit code.
				//TRACE(_T("Executed command but couldn't get exit code.\nCommand=%s\n"), cmdLine);
				return FALSE;
			}
			// We succeeded.
			return TRUE;
		}
	}

	void check_low_version_plugin()
	{
		uint32_t version = obs_get_version();
		if (version < MAKE_SEMANTIC_VERSION(28, 0, 0))
		{
			return;
		}

		//obs_get_version

		uint32_t regedit_version = get_obs_version_from_regedit();
		if (regedit_version > 0 && regedit_version < MAKE_SEMANTIC_VERSION(28, 0, 0))
		{
			return;
		}
		
		wchar_t path[MAX_PATH] = { 0 };
		SHGetSpecialFolderPath(
			NULL,
			path,
			CSIDL_COMMON_APPDATA,
			FALSE
		);

		std::filesystem::path plugin_path(path);
		if (plugin_path.empty()) {
			return;
		}

		plugin_path.append("obs-studio");
		plugin_path.append("plugins");

		std::vector<std::string> plugins = { "lovense-webcam-qt5", "obs-multi-rtmp" };

#if 0
		std::vector<std::wstring> cmds;
#endif
			
		std::error_code  err_code;
		for (auto plugin : plugins)
		{
			auto plugin_file = plugin_path;
			plugin_file.append(plugin);
			plugin_file.append("bin");
			plugin_file.append("64bit");
			plugin_file.append(plugin + ".dll");

			if (std::filesystem::exists(plugin_file, err_code))
			{
				std::filesystem::remove(plugin_file, err_code);

#if 0
				std::wstring cmd = L"\"";
				cmd += L"cmd.exe /c ";
				cmd += L"del ";
				cmd += plugin_file.wstring();
				cmd += L" /q /f";
				cmd += L"\"";
				

				cmds.emplace_back(cmd);
#endif
			}			
		}

#if 0
		for (auto cmd : cmds)
		{
			DWORD exit_code = 0;
			int result = executeCommandLine(cmd, exit_code);// WinExec(cmd.c_str(), 0);
			if (result && exit_code == 0)
			{
				//todo;
			}
		}
#endif
	}
#endif
}


