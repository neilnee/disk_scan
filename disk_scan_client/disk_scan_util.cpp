#include "stdafx.h"
#include "disk_scan_util.h"

#define SQL_BUF 1024

std::wstring GetProcessPath()
{
    TCHAR path[MAX_PATH];
    GetModuleFileName(NULL, path, MAX_PATH);
    std::wstring processPath = path;
    processPath = processPath.substr(0, processPath.rfind(L"\\"));
    return processPath;
}

std::string UTF16ToUTF8(const wchar_t* src)
{
    int cch2 = ::WideCharToMultiByte(CP_UTF8, 0, src, ::wcslen(src), 0, 0, 0, 0);
    char* str2  = new char[cch2 + 1];
    ::WideCharToMultiByte(CP_UTF8, 0, src, ::wcslen(src), str2, cch2 + 1, 0, 0);
    str2[cch2] = '\0';
    std::string destStr = str2;
    delete []str2;
    return destStr;
}

std::wstring UTF8ToUTF16(const char* src)
{
    int cch2 = ::MultiByteToWideChar(CP_UTF8, 0, src, ::strlen(src), NULL, 0);
    wchar_t* str2 = new wchar_t[cch2 + 1];
    ::MultiByteToWideChar(CP_UTF8, 0, src, ::strlen(src), str2, cch2);
    str2[cch2] = L'\0';
    std::wstring destStr = str2;
    delete []str2;
    return destStr;
}

BOOL CreateDownloadTask(std::vector<xl_ds_api::CScanFileInfo> files)
{
	return TRUE;
}

BOOL SetDownloadInfo(xl_ds_api::CScanFileInfo fileInfo)
{
	return TRUE;
}

std::string CIDCalculate(std::wstring path)
{
	return UTF16ToUTF8(path.c_str());
}