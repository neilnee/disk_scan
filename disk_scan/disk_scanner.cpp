#include "stdafx.h"
#include "disk_scanner.h"

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

VOID CScanner::Init()
{
    UnInit();
	INT ignores[] = IGNORE_DIRS;
	INT prioritys[] = PRIORITY_DIRS;
	TCHAR szPath[MAX_PATH];

	INT size  = sizeof(prioritys)/sizeof(prioritys[0]);
	for (INT i=0; i<size; i++) {
		SHGetFolderPath(NULL, prioritys[i], NULL, 0, szPath);
		std::wstring directory = szPath;
        if (PushBackDir(m_PriorityDirs, directory)) {
            m_FirstDirs ++;
        }
	}
	size  = sizeof(ignores)/sizeof(ignores[0]);
	for (INT i=0; i<size; i++) {
		SHGetFolderPath(NULL, ignores[i], NULL, 0, szPath);
		std::wstring directory = szPath;
		PushBackDir(m_IgnoreDirs, directory);
	}
    m_TotalDirs = m_FirstDirs;
}

VOID CScanner::UnInit()
{
    m_Exit = FALSE;
    m_Done = FALSE;
	m_ScanDirs = 0;
    m_TotalDirs = 0;
    m_FirstDirs = 0;
	m_PriorityDirs.clear();
	m_IgnoreDirs.clear();
	m_BaseDirs.clear();
	m_ScanTargetCallback = NULL;
}

BOOL CScanner::PushBackDir(std::vector<std::wstring> &dirList, std::wstring &directory) 
{
    BOOL result = FALSE;
	if (directory.rfind(L"\\") != directory.length() -1) {
		directory.append(L"\\");
	}
	if (std::find(dirList.begin(), dirList.end(), directory) == dirList.end()) {
		dirList.push_back(directory);
        result = TRUE;
	}
    return result;
}

VOID CScanner::InitBaseDir()
{
    TCHAR szTemp[PATH_BUF_SIZE];
    szTemp[0] = '\0';
    DWORD ret = GetLogicalDriveStrings(PATH_BUF_SIZE -1, szTemp);
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
                        if (PushBackDir(m_BaseDirs, directory)) {
                            m_FirstDirs++;
                        }
                    }
                    finish = !FindNextFile(handle, &fileData);
                } while (!finish);
            }
			while(*p++);
        } while (*p);
        FindClose(handle);
    }
    m_TotalDirs = m_FirstDirs;
}

VOID CScanner::SetScanTargetCallback(ScanTargetCallback callback)
{
	m_ScanTargetCallback = callback;
}

VOID CScanner::ClearResult()
{
    m_ScanDirs = 0;
    m_TotalDirs = m_FirstDirs;
    m_Done = FALSE;
    m_ImgDirectorys.clear();
}

VOID CScanner::ScanTargetDir(std::vector<std::wstring>* baseDir, std::vector<std::wstring> &targetDir, BOOL priority)
{
	if (m_Exit || baseDir == NULL || baseDir->size() <= 0) {
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
            m_ScanDirs++;
			continue;
		}
		// ����Ƿ�����ɨ�裬����������ɨ���ļ���
		if (!priority && std::find(m_PriorityDirs.begin(), m_PriorityDirs.end(), directory) != m_PriorityDirs.end()) {
            m_ScanDirs++;
			continue;
		}
		//��ʼɨ��directoryĿ¼�����ص�����
		if (m_ScanTargetCallback != NULL) {
			m_ScanTargetCallback(SCAN_START, m_ScanDirs, m_TotalDirs, directory);
		}
		SetCurrentDirectory(directory.c_str());
		handle = FindFirstFile(L"*", &fileData);
		if (handle != INVALID_HANDLE_VALUE) {
			BOOL finish = FALSE;
            INT targetCount = 0;
            INT otherCount = 0;
			do {
				std::wstring strFileName = fileData.cFileName;
				//��Ҫ���˵�.��..
                if (strFileName != L"." && strFileName != L"..") {
                    if (fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                        // ��Ŀ¼�����´εݹ�ɨ����б�
                        searchDir.push_back(directory+strFileName);
                        m_TotalDirs++;
                    } else {
                        // ���ļ����з������������ж���Ŀ¼�Ƿ���Ҫ�ҵ�Ŀ��Ŀ¼
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
                    }
                }
				finish = !FindNextFile(handle, &fileData);
			} while (!finish);
            // ��������ж��Ƿ�����ͼƬ�ļ���
            if(targetCount > 5 && ((targetCount * 10) / (targetCount + otherCount)) > 9) {
                PushBackDir(targetDir, directory);
				if (m_ScanTargetCallback != NULL) {
					m_ScanTargetCallback(SCAN_FOUND, m_ScanDirs, m_TotalDirs, directory);
				}
            }
            FindClose(handle);
		}
        m_ScanDirs++;
	}
    if (searchDir.size() > 0) {
        ScanTargetDir(&searchDir, targetDir, priority);
    }
}