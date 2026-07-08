#include "util/dstr.hpp"
#include "util/platform.h"



#include "json11/json11.hpp"

#include "lovense_toolset_custom.hpp"
#include "lovense_toolset_updater.hpp"

#include <shellapi.h>

#ifdef _WIN32
using namespace std;
namespace lovense::toolset
{
#if 0
	enum MyVersionType
	{
		DISTHTML = 0,
		LOVESE_PLUGIN,
		LOVESE_ACTION,
	};


	 wstring ReadInitFile(wstring strGroun, wstring strKey)
	{
		char path[512] = { 0 };
		if (os_get_config_path(path, sizeof(path), "obs-studio\\PluginVersion.ini") <= 0)
		{
			blog(LOG_ERROR, "[Lovese] ReadVersionConfig -> obs-studio\\PluginVersion.ini fail !");
			return wstring();
		}
		wstring strAppPath;
		strAppPath = string2wstring(path);
		TCHAR Result[MAX_PATH] = { 0 };
		DWORD num = GetPrivateProfileString(strGroun.c_str(), strKey.c_str(), NULL, Result, MAX_PATH, strAppPath.c_str());
		if (num == 0)
			return wstring();
		blog(LOG_ERROR, "[Lovese] GetPrivateProfileString num = %d ", num);

		wstring value = Result;
		return value;
	}

	static void ReadVersionConfig(wstring& strversion, MyVersionType VersionType)
	{

		char path[512] = { 0 };
		if (os_get_config_path(path, sizeof(path), "obs-studio\\PluginVersion.ini") <= 0)
		{
			blog(LOG_ERROR, "[Lovese] ReadVersionConfig -> obs-studio\\PluginVersion.ini fail !");
		}

		wstring strAppPath;
		strAppPath = string2wstring(path);
		TCHAR Result[MAX_PATH] = { 0 };
		if (VersionType == DISTHTML)
		{
			GetPrivateProfileString(L"DIST", L"version", L"1", Result, MAX_PATH, strAppPath.c_str());
		}
		else if (LOVESE_PLUGIN == VersionType)
		{
			GetPrivateProfileString(L"OBS_PLUGIN", L"version", L"1.0.0", Result, MAX_PATH, strAppPath.c_str());
		}
		strversion = Result;
	}

	static void WriteVersionConfig(wstring strversion, MyVersionType VersionType)
	{
		wstring strAppPath;
		char path[512] = { 0 };
		if (os_get_config_path(path, sizeof(path), "obs-studio\\PluginVersion.ini") <= 0)
		{
			blog(LOG_ERROR, "[Lovese] WriteVersionConfig -> obs-studio\\PluginVersion.ini fail !");
		}
		strAppPath = string2wstring(path);
		if (VersionType == DISTHTML)
		{
			WritePrivateProfileString(L"DIST", L"version", strversion.c_str(), strAppPath.c_str());
		}
		else if (LOVESE_PLUGIN == VersionType)
		{
			WritePrivateProfileString(L"OBS_PLUGIN", L"version", strversion.c_str(), strAppPath.c_str());
		}
	}
#endif

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

#if 0
	//{"hasUpdate":true, "version" : "1.0.2", "url" : "http://www.xxx.com/xx.zip"}
	static bool doRequest(std::string url, std::string& resp) {

		bool ret = false;
		do {
			char szobs_config_path[MAX_PATH] = { 0 };
			if (os_get_config_path(szobs_config_path, MAX_PATH, "obs-studio\\curl") <= 0)
			{
				blog(LOG_INFO, "[Lovense] curl process not exits!");
				break;
			}
			std::string curl_exe = "\"";
			curl_exe += szobs_config_path;
			curl_exe += "\\curl.exe\" ";
			curl_exe += url;
			curl_exe += " -o \"";
			curl_exe += szobs_config_path;
			curl_exe += "\\request.json\"";
			DWORD exit_code = 0;
			blog(LOG_INFO, "[Lovese] curl cmd= %s", curl_exe.c_str());

			int result = executeCommandLine(string2wstring(curl_exe), exit_code);// WinExec(cmd.c_str(), 0);
			if (result && exit_code == 0)
			{
				std::string resul_file = szobs_config_path;
				resul_file += "\\request.json";
				char* buffer = os_quick_read_utf8_file(resul_file.c_str());
				if (buffer)
				{
					resp = buffer;
					ret = true;
					blog(LOG_INFO, "[Lovese] curl respond= %s", resp.c_str());
				}
			}
			blog(LOG_INFO, "[Lovese] curl cmd result = %d exitcode = %d", result, exit_code);

		} while (0);

		return ret;
	}

	static BOOL  unzipDownLoadfile(std::string strfilename)
	{
		HZIP hz;
		std::wstring  Wstrname = string2wstring(strfilename);

		hz = OpenZip(Wstrname.c_str(), 0);

		if (hz)
		{
			size_t a = Wstrname.find_last_of('\\');
			std::wstring  txtleft = Wstrname.substr(0, a + 1);
			SetUnzipBaseDir(hz, txtleft.c_str());
			string  unzippath = wstring2string(txtleft);

			blog(LOG_INFO, "[Lovese] unzipDownLoadfile path %s", unzippath.c_str());

			ZIPENTRY ze;
			GetZipItem(hz, -1, &ze);
			int numitems = ze.index;
			for (int zi = 0; zi < numitems; zi++)
			{
				GetZipItem(hz, zi, &ze);
				UnzipItem(hz, zi, ze.name);
			}
			CloseZip(hz);

			DeleteFile(Wstrname.c_str());
			return TRUE;
		}
		else
		{
			blog(LOG_INFO, "[Lovese] unzip failed!");

			return FALSE;
		}
		return FALSE;
	}

	void KDeleteDirectorySH(std::string folderPath)
	{
		char fileFound[256];
		WIN32_FIND_DATAA info;
		HANDLE hp;
		sprintf(fileFound, "%s\\*.*", folderPath.c_str());
		hp = FindFirstFileA(fileFound, &info);
		do
		{
			if (!((strcmp(info.cFileName, ".") == 0) ||
				(strcmp(info.cFileName, "..") == 0)))
			{
				if ((info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ==
					FILE_ATTRIBUTE_DIRECTORY)
				{
					string subFolder = folderPath;
					subFolder.append("\\");
					subFolder.append(info.cFileName);
					KDeleteDirectorySH(subFolder);
					RemoveDirectoryA(subFolder.c_str());
				}
				else
				{
					sprintf(fileFound, "%s\\%s", folderPath.c_str(), info.cFileName);
					BOOL retVal = DeleteFileA(fileFound);
				}
			}

		} while (FindNextFileA(hp, &info));
		FindClose(hp);
	}

	void TestUnzip()
	{
		char szobs_config_path[MAX_PATH] = { 0 };
		if (os_get_config_path(szobs_config_path, MAX_PATH, "obs-studio\\") <= 0)
		{
			blog(LOG_INFO, "[Lovense] curl process not exits!");
			return;
		}
		std::string zipFile = szobs_config_path;
		zipFile += "dist.zip";
		bool ret = unzipDownLoadfile(zipFile);
		blog(LOG_INFO, "[Lovese] curl cmd result = %d ", ret);
	}

	static int  downloadAndUnzip(string url, string strversion)
	{
		bool ret = false;
		do {
			char szobs_config_path[MAX_PATH] = { 0 };
			if (os_get_config_path(szobs_config_path, MAX_PATH, "obs-studio\\") <= 0)
			{
				blog(LOG_INFO, "[Lovense] curl process not exits!");
				break;
			}
			std::string zipFile = szobs_config_path;
			zipFile += "dist.zip";
			/*
			* std::string curl_exe = "\"";
			curl_exe += szobs_config_path;
			curl_exe += "curl\\curl.exe\" ";
			curl_exe += url;
			curl_exe += " -o \"";
			curl_exe += zipFile;
			curl_exe += "\"";
			DWORD exit_code = 0;
			blog(LOG_INFO, "[Lovese] curl cmd= %s", curl_exe.c_str());

			int result = executeCommandLine(string2wstring(curl_exe), exit_code);// WinExec(cmd.c_str(), 0);
			*/
			toolset::CHttpClient client;
			int result = client.downLoadFile(url, zipFile);
			if (result == 0)
			{
				if (PathFileExists(string2wstring(zipFile).c_str()))
				{
					std::string rm_path = szobs_config_path;
					rm_path += "dist";
					std::wstring wstr_path = string2wstring(rm_path);
					std::wstring error;
					KDeleteDirectorySH(wstring2string(wstr_path));
					if (unzipDownLoadfile(zipFile))
					{
						WriteVersionConfig(string2wstring(strversion), DISTHTML);
					}
					DeleteFile(string2wstring(zipFile).c_str());
				}
			}
			blog(LOG_INFO, "[Lovese] curl cmd result = %d ", result);
		} while (0);
		return ret;
	}


	static void ParseJsonAndDownload(std::string resp, MyVersionType versiontype)
	{
		blog(LOG_INFO, "[Lovese] ParseJsonAndDownload start ! ");
		std::string err;
		auto json = json11::Json::parse(resp, err);
		if (!err.empty())
		{
			blog(LOG_ERROR, "[Lovese] ParseJson fail! ");
			return;
		}

		bool  hasUpdate = json["hasUpdate"].bool_value();
		std::string version = json["version"].string_value();
		std::string url = json["url"].string_value();
		blog(LOG_INFO, "[Lovese] ParseJsonAndDownload version= %s", version.c_str());
		blog(LOG_INFO, "[Lovese] ParseJsonAndDownload hasUpdate= %d", int(hasUpdate));
		blog(LOG_INFO, "[Lovese] ParseJsonAndDownload url= %s", url.c_str());

		if ((url.length() > 5) && hasUpdate)
		{
			if (versiontype == DISTHTML)
			{
				downloadAndUnzip(url, version);
			}
		}
		else
		{
			blog(LOG_INFO, "[Lovese] No update! ");
		}
	}


	static void GetFileVersion(MyVersionType versiontype)
	{
		std::string resp;
		wstring _Wstrversion;
		ReadVersionConfig(_Wstrversion, versiontype);
		string _strversion = wstring2string(_Wstrversion);
		std::string url = "";
		if (versiontype == DISTHTML)
		{
			wstring wurl = ReadInitFile(L"DIST", L"UpdateUrl");
			if (wurl.empty())
				url += DISTHTMLBaseUrl;
			else
			{
				url = wstring2string(wurl);
			}
		}
		else if (versiontype == LOVESE_PLUGIN)
		{
			wstring key = L"UpdateUrl32";

#ifdef _WIN64
			key = L"UpdateUrl64";
#else
			key = L"UpdateUrl32";
#endif

			wstring wurl = ReadInitFile(L"OBS_PLUGIN", key.c_str());
			if (wurl.empty())
				url += Levese_PluginBaseUrl;
			else
			{
				url = wstring2string(wurl);
			}
		}

		url += "?";
		url += "v=";
		url += _strversion;
		blog(LOG_INFO, "[Lovese] doRequest url= %s", url.c_str());
		toolset::CHttpClient client;
		std::list<std::string> httpHeads;
		if (client.Get(url, resp, httpHeads) == 0)
		{
			if (resp.length() > 10)
				ParseJsonAndDownload(resp.c_str(), versiontype);
			else
				blog(LOG_ERROR, "[Lovese] doRequest  un response ");
		}
	}

	void UpdateFile(void* lpParam)
	{
		blog(LOG_INFO, "[Lovese] start  get dist html version! ");
		GetFileVersion(DISTHTML);
	}
#endif


	bool CopyDirTo(const wstring& source_folder, const wstring& target_folder)
	{
		wstring new_sf = source_folder + L"\\*";
		WCHAR sf[MAX_PATH + 1];
		WCHAR tf[MAX_PATH + 1];

		wcscpy_s(sf, MAX_PATH, new_sf.c_str());
		wcscpy_s(tf, MAX_PATH, target_folder.c_str());

		sf[lstrlenW(sf) + 1] = 0;
		tf[lstrlenW(tf) + 1] = 0;

		SHFILEOPSTRUCTW s = { 0 };
		s.wFunc = FO_COPY;
		s.pTo = tf;
		s.pFrom = sf;
		s.fFlags = FOF_SILENT | FOF_NOCONFIRMMKDIR | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_NO_UI;
		int res = SHFileOperationW(&s);
		DWORD error = GetLastError();
		blog(LOG_INFO, "[Lovense] copy ret = %d error=%d!", res, error);
		return res == 0;
	}

	void startCopyFile()
	{
		char path[512] = { 0 };
		if (os_get_program_data_path(path, sizeof(path), "obs-studio\\lovense-video-freedback") <= 0)
		{
			blog(LOG_INFO, "[Lovense] Get program data path failed!");
			return;
		}

		char appdataPath[512] = { 0 };
		if (os_get_config_path(appdataPath, sizeof(appdataPath), "obs-studio") <= 0)
		{
			blog(LOG_INFO, "[Lovense] Get appdata path failed!");
			return;
		}

		char pathConfig[512] = { 0 };
		if (os_get_config_path(pathConfig, sizeof(pathConfig), "obs-studio\\PluginVersion.ini") <= 0)
		{
			blog(LOG_ERROR, "[Lovese] ReadVersionConfig -> obs-studio\\PluginVersion.ini fail !");
			//Configuration file does not exist, no processing needed
			return;
		}
		wstring strAppPath = string2wstring(pathConfig);
		TCHAR Result[MAX_PATH] = { 0 };
		GetPrivateProfileString(L"DIST", L"version", nullptr, Result, MAX_PATH, strAppPath.c_str());

		TCHAR Result2[MAX_PATH] = { 0 };
		string strComPath = path;
		strComPath += "\\PluginVersion.ini";
		wstring strCommonAppPath = string2wstring(strComPath);
		GetPrivateProfileString(L"DIST", L"version", nullptr, Result2, MAX_PATH, strCommonAppPath.c_str());
		std::wstring strVersion = Result;
		std::wstring strVersion2 = Result2;
		if (strVersion.empty() || _wcsicmp(strVersion2.c_str(), strVersion.c_str()) > 0)
		{

			blog(LOG_INFO, "[Lovese] ReadVersionConfig ->copy!");
			std::string cmd = "xcopy \"";
			cmd += path;
			cmd += "\" \"";
			cmd += appdataPath;
			cmd += "\" /e /i /h /y";

			wchar_t wszCmd[MAX_PATH * 2] = { 0 };
			os_utf8_to_wcs(cmd.c_str(), cmd.size(), wszCmd, MAX_PATH * 2);
			DWORD iresult = 0;
			executeCommandLine(wszCmd, iresult);
			blog(LOG_INFO, "[Lovese] copy %d cmd = %s ", iresult, cmd.c_str());
		}

		char szLocalFile[512] = { 0 };
		os_get_config_path(szLocalFile, sizeof(szLocalFile), "obs-studio\\dist\\index.html");

		if (!os_file_exists(szLocalFile))
		{
			std::wstring wstrFrom = string2wstring(path);
			std::wstring wstrTo = string2wstring(appdataPath);
			wchar_t szFrom[MAX_PATH] = { 0 };
			wchar_t szTo[MAX_PATH] = { 0 };
			os_utf8_to_wcs(path, strlen(path), szFrom, MAX_PATH);
			os_utf8_to_wcs(appdataPath, strlen(appdataPath), szTo, MAX_PATH);
			CopyDirTo(szFrom, szTo);
		}
	}
}

#endif
