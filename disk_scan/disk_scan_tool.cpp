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

void CScanner::ScanDirectory()
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
        m_RootPaths.clear();
        do {
            *szDrive = *p;
            SetCurrentDirectory(szDrive);
            handle = FindFirstFile(L"*", &fileData);
            if (handle != INVALID_HANDLE_VALUE) {
                finish = FALSE;
                do {
                    if ((fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                        && !(fileData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM)) {
                        std::wstring strFileName = fileData.cFileName;
                        if (strFileName != L"Windows"
                            && strFileName != L"Program Files"
                            && strFileName != L"Program Files (x86)"
                            && strFileName != L"ProgramData") {
                                m_RootPaths.push_back(szDrive+strFileName);
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