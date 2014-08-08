#ifndef _DISK_SCAN_UTIL_
#define _DISK_SCAN_UTIL_

#include <windows.h>

std::wstring GetProcessPath();

std::string UTF16ToUTF8(const wchar_t* src);

std::wstring UTF8ToUTF16(const char*src);

#endif