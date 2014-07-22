#include "stdafx.h"
#include "disk_scan_tool.h"
#include <windows.h>

using namespace xl_ds_api;

const DWORD BUF_SIZE  = 512;

CScanner::CScanner()
{
}

CScanner::~CScanner()
{
}

void CScanner::ScanBaseDir(std::vector<std::wstring> &baseDirs)
{
    TCHAR szTemp[BUF_SIZE];
    szTemp[0] = '\0';
    DWORD ret = GetLogicalDriveStrings(BUF_SIZE -1, szTemp);
    if (ret) {
        TCHAR szDrive[4] = TEXT(" :\\");
        TCHAR* p = szTemp;
        WIN32_FIND_DATA fileData;
        HANDLE handle = NULL;
        BOOL finish = FALSE;
        do {
            *szDrive = *p;
			UINT driveType = GetDriveType(szDrive);
			// 过滤掉软驱，只考虑本地磁盘和可移动磁盘
			if (szDrive[0] == L'a' || szDrive[0] == L'A' || szDrive[0] == L'b' || szDrive[0] == L'B'
				|| (driveType != DRIVE_FIXED && driveType != DRIVE_REMOVABLE)) {
				while(*p++);
				continue;
			}
            SetCurrentDirectory(szDrive);
            handle = FindFirstFile(L"*", &fileData);
            if (handle != INVALID_HANDLE_VALUE) {
                finish = FALSE;
                do {
					//只考虑非系统的目录，同时过滤调一些系统创建的文件夹
					//需要过滤掉.和..
                    if ((fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                        && !(fileData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM)) {
                        std::wstring strFileName = fileData.cFileName;
                        if (strFileName != L"Windows"
                            && strFileName != L"Program Files"
                            && strFileName != L"Program Files (x86)"
                            && strFileName != L"ProgramData") {
                                baseDirs.push_back(szDrive+strFileName);
                        }
                    }
                    finish = !FindNextFile(handle, &fileData);
                } while (!finish);
            }
			while(*p++);
        } while (*p);
        FindClose(handle);
    }
}

void CScanner::ScanPicDir(std::vector<std::wstring>* baseDir, std::vector<std::wstring> &picDir)
{
	if (baseDir == NULL || baseDir->size() <= 0) {
		return;
	}
	std::vector<std::wstring>::iterator iter;
	std::vector<std::wstring> searchDir;
	for (iter = baseDir->begin(); iter != baseDir->end(); iter++) {
		WIN32_FIND_DATA fileData;
		HANDLE handle = NULL;
		BOOL finish = FALSE;
		std::vector<std::wstring> fileList;
		std::wstring dirStr = *iter;
        
        // 需要补上最后的斜杠
        std::wstring::size_type pos = dirStr.rfind(L"\\");
        if (pos != dirStr.length() - 1) {
            dirStr.append(L"\\");
        }

		LPCTSTR directory = dirStr.c_str();
		SetCurrentDirectory(directory);
		handle = FindFirstFile(L"*", &fileData);
		if (handle != INVALID_HANDLE_VALUE) {
			finish = FALSE;
			do {
				std::wstring strFileName = fileData.cFileName;
				//需要过滤掉.和..
                if (strFileName != L"." && strFileName != L"..") {
                    if (fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                        searchDir.push_back(directory+strFileName);
                    } else {
                        fileList.push_back(directory+strFileName);
                    }
                }
				finish = !FindNextFile(handle, &fileData);
			} while (!finish);
		}
	}
    if (searchDir.size() > 0) {
        ScanPicDir(&searchDir, picDir);
    }
}