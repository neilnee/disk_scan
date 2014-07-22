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
        
        // ��Ҫ��������б��
        if (dirStr.rfind(L"\\") != dirStr.length() - 1) {
            dirStr.append(L"\\");
        }

        //��ʼɨ��directoryĿ¼
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
                                if (IMG_SUFFIX->find(suffix) == std::wstring::npos) {
                                    otherCount++;
                                } else {
                                    targetCount++;
                                }
                            }
                            // Ŀ���ļ��е��ڲ��ж��ж����򣬱������ж�
                            if (targetCount > 10 && otherCount == 0) {
                                found = TRUE;
                            }
                        }
                    }
                }
				finish = !FindNextFile(handle, &fileData);
			} while (!finish);
            // Ŀ���ļ��е�����ж��ж����򣬱�������ж�
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