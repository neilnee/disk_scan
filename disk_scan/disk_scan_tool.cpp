#include "stdafx.h"
#include "disk_scan_tool.h"

using namespace xl_ds_api;

CScanner::CScanner()
{
    m_ScanDirs = 0;
}

CScanner::~CScanner()
{
}

void CScanner::ScanBaseDir()
{
    TCHAR szTemp[BUF_SIZE];
    szTemp[0] = '\0';
    DWORD ret = GetLogicalDriveStrings(BUF_SIZE -1, szTemp);
    if (ret) {
        TCHAR szDrive[4] = TEXT(" :\\");
        TCHAR* p = szTemp;
        WIN32_FIND_DATA fileData;
        HANDLE handle = NULL;
        m_BaseDirs.clear();
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
                BOOL finish = FALSE;
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
                                std::wstring targetDir = szDrive+strFileName;
                                m_BaseDirs.push_back(targetDir);
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

void CScanner::ScanPriorityDir()
{
    m_PriorityDirs.clear();
    INT prioritys[] = PRIORITY_DIRS;
    TCHAR szPath[MAX_PATH];
    size_t size  = sizeof(prioritys)/sizeof(prioritys[0]);
    for (INT i=0; i<size; i++) {
        SHGetFolderPath(NULL, prioritys[i], NULL, 0, szPath);
        std::wstring directory = szPath;
        m_PriorityDirs.push_back(directory);
    }
}

void CScanner::ScanTargetDir(std::vector<std::wstring>* baseDir, std::vector<std::wstring> &targetDir, ScanTargetCallback callback)
{
	if (baseDir == NULL || baseDir->size() <= 0) {
		return;
	}
	std::vector<std::wstring>::iterator iter;
	std::vector<std::wstring> searchDir;
	for (iter = baseDir->begin(); iter != baseDir->end(); iter++) {
		WIN32_FIND_DATA fileData;
		HANDLE handle = NULL;
		std::wstring dirStr = *iter;
        
        // 需要补上最后的斜杠
        if (dirStr.rfind(L"\\") != dirStr.length() - 1) {
            dirStr.append(L"\\");
        }

        //开始扫描directory目录
        if (callback != NULL) {
            callback(m_ScanDirs, dirStr);
        }
        LPCTSTR directory = dirStr.c_str();
		SetCurrentDirectory(directory);

		handle = FindFirstFile(L"*", &fileData);
		if (handle != INVALID_HANDLE_VALUE) {
			BOOL finish = FALSE;
            BOOL found = FALSE;
            INT targetCount = 0;
            INT otherCount = 0;
			do {
				std::wstring strFileName = fileData.cFileName;
				//需要过滤掉.和..
                if (strFileName != L"." && strFileName != L"..") {
                    if (fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                        // 将目录列入下次递归扫描的列表
                        searchDir.push_back(directory+strFileName);
                    } else {
                        // 对文件进行分析操作，以判定该目录是否是要找的目标目录
                        if (!found) {
                            std::wstring::size_type suffixPos = strFileName.rfind(L".");
                            if (suffixPos != std::wstring::npos) {
                                std::wstring suffix = strFileName.substr(suffixPos);
                                if (IMG_SUFFIX->find(suffix) == std::wstring::npos) {
                                    otherCount++;
                                } else {
                                    targetCount++;
                                }
                            }
                            // 目标文件夹的内层判定判定规则，遍历中判定
                            if (targetCount > 10 && otherCount == 0) {
                                found = TRUE;
                            }
                        }
                    }
                }
				finish = !FindNextFile(handle, &fileData);
			} while (!finish);
            // 目标文件夹的外层判定判定规则，遍历完后判定
            if(found) {
                targetDir.push_back(directory);
            }
		}
        m_ScanDirs++;
	}
    if (searchDir.size() > 0) {
        ScanTargetDir(&searchDir, targetDir, callback);
    }
}