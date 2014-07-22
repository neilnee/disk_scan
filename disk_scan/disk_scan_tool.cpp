#include "stdafx.h"
#include "disk_scan_tool.h"

#include <algorithm>

using namespace xl_ds_api;

CScanner::CScanner()
{
	Init();
	InitBaseDir();
}

CScanner::~CScanner()
{
	UnInit();
}

void CScanner::Init()
{
	m_ScanDirs = 0;
	m_BaseDirs.clear();
	m_PriorityDirs.clear();
	m_IgnoreDirs.clear();
	m_ScanTargetCallback = NULL;

	INT ignores[] = IGNORE_DIRS;
	INT prioritys[] = PRIORITY_DIRS;
	TCHAR szPath[MAX_PATH];

	INT size  = sizeof(prioritys)/sizeof(prioritys[0]);
	for (INT i=0; i<size; i++) {
		SHGetFolderPath(NULL, prioritys[i], NULL, 0, szPath);
		std::wstring directory = szPath;
		PushBackDir(m_PriorityDirs, directory);
	}
	size  = sizeof(ignores)/sizeof(ignores[0]);
	for (INT i=0; i<size; i++) {
		SHGetFolderPath(NULL, ignores[i], NULL, 0, szPath);
		std::wstring directory = szPath;
		PushBackDir(m_IgnoreDirs, directory);
	}
}

void CScanner::UnInit()
{
	m_ScanDirs = 0;
	m_PriorityDirs.clear();
	m_IgnoreDirs.clear();
	m_BaseDirs.clear();
	m_ScanTargetCallback = NULL;
}

void CScanner::PushBackDir(std::vector<std::wstring> &dirList, std::wstring &directory) 
{
	if (directory.rfind(L"\\") != directory.length() -1) {
		directory.append(L"\\");
	}
	if (std::find(dirList.begin(), dirList.end(), directory) == dirList.end()) {
		dirList.push_back(directory);
	}
}

void CScanner::InitBaseDir()
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
                        std::wstring fileName = fileData.cFileName;
						std::wstring directory = szDrive+fileName;
						if (fileName == L"Windows"
							|| fileName == L"Program Files"
							|| fileName == L"ProgramData"
							|| fileName == L"Program Files (x86)") {
								PushBackDir(m_IgnoreDirs, directory);
						}
						m_BaseDirs.push_back(directory);
                    }
                    finish = !FindNextFile(handle, &fileData);
                } while (!finish);
            }
			while(*p++);
        } while (*p);
        FindClose(handle);
    }
}

void CScanner::SetScanTargetCallback(ScanTargetCallback callback) {
	m_ScanTargetCallback = callback;
}

void CScanner::ScanTargetDir(std::vector<std::wstring>* baseDir, std::vector<std::wstring> &targetDir, BOOL priority)
{
	if (baseDir == NULL || baseDir->size() <= 0) {
		return;
	}

	std::vector<std::wstring>::iterator iter;
	std::vector<std::wstring> searchDir;
	for (iter = baseDir->begin(); iter != baseDir->end(); iter++) {
		WIN32_FIND_DATA fileData;
		HANDLE handle = NULL;
		std::wstring directory = *iter;
        
        // 需要补上最后的斜杠
        if (directory.rfind(L"\\") != directory.length() - 1) {
            directory.append(L"\\");
        }
		// 跳过忽略文件夹
		if (std::find(m_IgnoreDirs.begin(), m_IgnoreDirs.end(), directory) != m_IgnoreDirs.end()) {
			continue;
		}
		// 如果是非优先扫描，则跳过优先扫描文件夹
		if (!priority && std::find(m_PriorityDirs.begin(), m_PriorityDirs.end(), directory) != m_PriorityDirs.end()) {
			continue;
		}
		//开始扫描directory目录，并回调进度
		if (m_ScanTargetCallback != NULL) {
			m_ScanTargetCallback(SCAN_START, m_ScanDirs, directory);
		}
		SetCurrentDirectory(directory.c_str());
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
								std::transform(suffix.begin(), suffix.end(), suffix.begin(), tolower);
                                if (IMG_SUFFIX->find(suffix) == std::wstring::npos) {
                                    otherCount++;
                                } else {
                                    targetCount++;
                                }
                            }
                            // 目标文件夹的内层判定判定规则，遍历中判定
                            if (targetCount > 50 && otherCount == 0) {
                                found = TRUE;
                            }
                        }
                    }
                }
				finish = !FindNextFile(handle, &fileData);
			} while (!finish);
            // 目标文件夹的外层判定判定规则，遍历完后判定
            if(found || (targetCount > 20 && otherCount == 0)) {
                PushBackDir(targetDir, directory);
				if (m_ScanTargetCallback != NULL) {
					m_ScanTargetCallback(SCAN_FOUND, m_ScanDirs, directory);
				}
            }
		}
        m_ScanDirs++;
	}
    if (searchDir.size() > 0) {
        ScanTargetDir(&searchDir, targetDir, priority);
    }
}