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
			// ���˵�������ֻ���Ǳ��ش��̺Ϳ��ƶ�����
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
					//ֻ���Ƿ�ϵͳ��Ŀ¼��ͬʱ���˵�һЩϵͳ�������ļ���
					//��Ҫ���˵�.��..
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
        
        // ��Ҫ��������б��
        if (directory.rfind(L"\\") != directory.length() - 1) {
            directory.append(L"\\");
        }
		// ���������ļ���
		if (std::find(m_IgnoreDirs.begin(), m_IgnoreDirs.end(), directory) != m_IgnoreDirs.end()) {
			continue;
		}
		// ����Ƿ�����ɨ�裬����������ɨ���ļ���
		if (!priority && std::find(m_PriorityDirs.begin(), m_PriorityDirs.end(), directory) != m_PriorityDirs.end()) {
			continue;
		}
		//��ʼɨ��directoryĿ¼�����ص�����
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
				//��Ҫ���˵�.��..
                if (strFileName != L"." && strFileName != L"..") {
                    if (fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                        // ��Ŀ¼�����´εݹ�ɨ����б�
                        searchDir.push_back(directory+strFileName);
                    } else {
                        // ���ļ����з������������ж���Ŀ¼�Ƿ���Ҫ�ҵ�Ŀ��Ŀ¼
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
                            // Ŀ���ļ��е��ڲ��ж��ж����򣬱������ж�
                            if (targetCount > 50 && otherCount == 0) {
                                found = TRUE;
                            }
                        }
                    }
                }
				finish = !FindNextFile(handle, &fileData);
			} while (!finish);
            // Ŀ���ļ��е�����ж��ж����򣬱�������ж�
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