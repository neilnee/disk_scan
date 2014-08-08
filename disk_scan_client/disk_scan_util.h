#ifndef _DISK_SCAN_UTIL_
#define _DISK_SCAN_UTIL_

#include <windows.h>
#include <vector>
#include <string>
#include "disk_scan.h"

std::wstring GetProcessPath();

std::string UTF16ToUTF8(const wchar_t* src);

std::wstring UTF8ToUTF16(const char*src);

BOOL CreateDownloadTask(std::vector<xl_ds_api::CScanFileInfo> files);

BOOL SetDownloadInfo(xl_ds_api::CScanFileInfo fileInfo);

std::string CIDCalculate(std::wstring path);

#endif