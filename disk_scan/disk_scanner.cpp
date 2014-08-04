#include "stdafx.h"
#include "disk_scanner.h"

#include <algorithm>
#include <time.h>
#include <Shlwapi.h>

// 持久化的扫描结果有效期 2天
#define RESULT_VALIDITY_PERIOD 172880

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
    TCHAR curDirectory[MAX_PATH];
    DWORD curDirRet = GetCurrentDirectory(MAX_PATH, curDirectory);

    SHGetFolderPath(NULL, CSIDL_RECENT, NULL, 0, szPath);
    if (SetCurrentDirectory(szPath)) {
        IShellLink* slPtr= NULL;
        IPersistFile* pfPtr = NULL;
        CoInitialize(NULL);
        HRESULT hr = CoCreateInstance(
            CLSID_ShellLink, 
            NULL,CLSCTX_INPROC_SERVER,  
            IID_IShellLink,
            reinterpret_cast<LPVOID*>(&slPtr));
        if (!FAILED(hr)) {
            hr = slPtr->QueryInterface(
                IID_IPersistFile,
                reinterpret_cast<LPVOID*>(&pfPtr));
            if (!FAILED(hr)) {
                WIN32_FIND_DATA findData;
                HANDLE handle = FindFirstFile(L"*.lnk", &findData);
                BOOL finish = FALSE;
                while (!finish && handle != INVALID_HANDLE_VALUE) {
                    std::wstring filePath = szPath;
                    if (filePath.rfind(L"\\") != filePath.length() - 1) {
                        filePath.append(L"\\");
                    }
                    filePath.append(findData.cFileName);
                    hr = pfPtr->Load(filePath.c_str(), STGM_READ);
                    if (!FAILED(hr)) {
                        hr = slPtr->Resolve(NULL, SLR_NO_UI | SLR_NOSEARCH);
                        if (!FAILED(hr)) {
                            TCHAR path[MAX_PATH];
                            slPtr->GetPath(path, MAX_PATH, &findData, NULL);
                            if(PathIsDirectory(path)) {
                                std::wstring directory = path;
                                PushBackDir(m_PriorityDirs, directory);
                            }
                        }
                    }
                    finish = !FindNextFile(handle, &findData);
                }
                FindClose(handle);
            }
        }
        if (slPtr != NULL) {
            slPtr->Release();
        }
        if (pfPtr != NULL) {
            pfPtr->Release();
        }
        CoUninitialize();
    }

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

    if (curDirRet < MAX_PATH) {
        SetCurrentDirectory(curDirectory);
    }
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
	m_ImgDirectorys.clear();
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

std::wstring CScanner::GetScanResultFilePath() {
    TCHAR fileName[MAX_PATH];
    GetModuleFileName(NULL, fileName, MAX_PATH);
    std::wstring filePath = fileName;
    filePath = filePath.substr(0, filePath.rfind(L"\\"));
    filePath.append(L"\\.scan_result");
    return filePath;
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
	DeleteFile(GetScanResultFilePath().c_str());
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
		HANDLE handle = INVALID_HANDLE_VALUE;
		std::wstring directory = *iter;
        
        // 需要补上最后的斜杠
        if (directory.rfind(L"\\") != directory.length() - 1) {
            directory.append(L"\\");
        }
		// 跳过忽略文件夹
		if (std::find(m_IgnoreDirs.begin(), m_IgnoreDirs.end(), directory) != m_IgnoreDirs.end()) {
            m_ScanDirs++;
			continue;
		}
		// 如果是非优先扫描，则跳过优先扫描文件夹
		if (!priority && std::find(m_PriorityDirs.begin(), m_PriorityDirs.end(), directory) != m_PriorityDirs.end()) {
            m_ScanDirs++;
			continue;
		}
		//开始扫描directory目录，并回调进度
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
				//需要过滤掉.和..
                if (strFileName != L"." && strFileName != L"..") {
                    if (fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                        // 将目录列入下次递归扫描的列表
                        searchDir.push_back(directory+strFileName);
                        m_TotalDirs++;
                    } else {
                        // 对文件进行分析操作，以判定该目录是否是要找的目标目录
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
            // 遍历完后判定是否属于图片文件夹
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

VOID CScanner::SaveImgScanResult(std::vector<std::wstring>* imgDirectorys)
{
	if (imgDirectorys == NULL) {
		return;
	}
	time_t curTime = time(NULL);

	std::wstring filePath = GetScanResultFilePath();

	HANDLE file = CreateFile(
		filePath.c_str(),
		GENERIC_WRITE,
		0,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_HIDDEN,
		NULL);
	if (file == INVALID_HANDLE_VALUE) {
		return;
	}

	DWORD written;
	TCHAR time[20] = {0};
	wsprintf(time, L"%d\n", curTime);
	WriteFile(file, time, sizeof(TCHAR) * lstrlen(time), &written, NULL);

	std::vector<std::wstring>::iterator iter;
	for (iter = imgDirectorys->begin(); iter != imgDirectorys->end(); iter++) {
		TCHAR path[MAX_PATH] = {0};
		wsprintf(path, L"%s\n", (*iter).c_str());
		WriteFile(file, path, sizeof(TCHAR) * lstrlen(path), &written, NULL);
	}
	CloseHandle(file);
}

BOOL CScanner::LoadImgScanResult(std::vector<std::wstring> &imgDirectorys)
{
	BOOL result = FALSE;
	time_t curTime = time(NULL);
	std::wstring filePath = GetScanResultFilePath();

	HANDLE file = CreateFile(
		filePath.c_str(),
		GENERIC_READ,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_HIDDEN,
		NULL);
	if (file == INVALID_HANDLE_VALUE) {
		return result;
	}

	TCHAR buf = '\0';
	DWORD bufSize = sizeof(TCHAR);
	DWORD readSize;
	std::wstring line;
	while(ReadFile(file, &buf, bufSize, &readSize, NULL) == TRUE && buf != '\n') {
		line = line + buf;
	}
	time_t saveTime = _ttoi64(line.c_str());
	if ((curTime - saveTime) < RESULT_VALIDITY_PERIOD) {
		TCHAR curDirectory[MAX_PATH];
		DWORD curDirRet = GetCurrentDirectory(MAX_PATH, curDirectory);
		buf = '\0';
		line.clear();
		BOOL finish = FALSE;
		do {
			finish = ReadFile(file, &buf, bufSize, &readSize, NULL);
			if (readSize == 0) {
				finish = FALSE;
			}
			if (finish) {
				if (buf == '\n') {
					if(SetCurrentDirectory(line.c_str())) {
						imgDirectorys.push_back(line);
					}
					line.clear();
				} else {
					line = line + buf;
				}
			}
		} while(finish);
		if (curDirRet <= MAX_PATH) {
			SetCurrentDirectory(curDirectory);
		}
	}
	if (imgDirectorys.size() > 0) {
		result = TRUE;
	}
	CloseHandle(file);
    return result;
}