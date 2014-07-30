#include "stdafx.h"
#include "disk_scan.h"
#include <vector>
#include <ShlObj.h>

#define BUFF_SIZE 1024
#define TIMEOUT 5000
#define SCAN_REQUEST_IMG L"scan_img"
#define SCAN_REQUEST_IMG_AFREAH L"scan_img_afreah"

using namespace xl_ds_api;

LPTSTR m_PipeName = TEXT("\\\\.\\pipe\\xlspace_disk_scan_pipe");

std::vector<DWORD> m_NotifyThreadIDs;
BOOL m_ImgProcessScanning = FALSE;
HANDLE m_IPSMutex = CreateMutex(NULL,FALSE,NULL);

DWORD WINAPI ScanImgProcessExecute(LPVOID lpParam);

CDiskScan::CDiskScan()
{
}

CDiskScan::~CDiskScan()
{
    m_NotifyThreadIDs.clear();
}

VOID CDiskScan::ScanImgInProcess(DWORD notifyThreadID)
{
    DWORD* pNotifyThreadID = new DWORD(notifyThreadID);
    DWORD threadID;
    CreateThread(
        NULL,
        0,
        ScanImgProcessExecute,
        (LPVOID) pNotifyThreadID,
        0,
        &threadID);
}

DWORD WINAPI ScanImgProcessExecute(LPVOID lpParam)
{
    BOOL success = FALSE;

    WaitForSingleObject(m_IPSMutex,INFINITE);
    DWORD* threadIDPtr = (DWORD*) lpParam;
    if (threadIDPtr == NULL) {
        return -1;
    }
    DWORD threadID = *threadIDPtr;
    delete threadIDPtr;
    m_NotifyThreadIDs.push_back(threadID);
    if(!m_ImgProcessScanning) {
        m_ImgProcessScanning = TRUE;
    } else {
        success = TRUE;
    }
    ReleaseMutex(m_IPSMutex);

    if (success) {
        return 0;
    }

    HANDLE pipe = INVALID_HANDLE_VALUE;
    for(;;) {
        pipe = CreateFile(
            m_PipeName,
            GENERIC_READ |
            GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED,
            NULL);
        if (pipe != INVALID_HANDLE_VALUE) {
            // 连接成功
            break;
        }
        if (GetLastError() == ERROR_FILE_NOT_FOUND) {
            // 没有扫描进程启动，启动扫描进程
            continue;
        }
        if (GetLastError() != ERROR_PIPE_BUSY) {
            return -1;
        }
        if (!WaitNamedPipe(m_PipeName, TIMEOUT)) {
            // 连接超时
            continue;
        }
    }

    DWORD dMode = PIPE_READMODE_MESSAGE;
    success = SetNamedPipeHandleState(
        pipe,
        &dMode,
        NULL,
        NULL);
    if (!success) {
        return -1;
    }

    LPTSTR message = SCAN_REQUEST_IMG_AFREAH;
    DWORD dWrite;
    DWORD dWritten;
    OVERLAPPED overl;
    HANDLE even = CreateEvent(NULL,TRUE,TRUE,NULL);
    overl.hEvent = even;
    overl.Offset = 0;
    overl.OffsetHigh = 0;
    dWrite = (lstrlen(message)+1) * sizeof(TCHAR);
    do {
        success = WriteFile(
            pipe,
            message,
            dWrite,
            &dWritten,
            &overl);
        WaitForSingleObject(even, TIMEOUT);
        DWORD transBytes;
        success = GetOverlappedResult(pipe, &overl, &transBytes, FALSE);
    } while (!success);

    TCHAR buf[BUFF_SIZE];
    DWORD dRead;
    do {
        success = ReadFile(
            pipe,
            buf,
            BUFF_SIZE*sizeof(TCHAR),
            &dRead,
            NULL);
        std::wstring data = buf;
		std::wstring path;
		INT eventCode = 0;
		INT scanCount = 0;
		INT totalCount = 0;
		std::wstring::size_type offset = 0;
        std::wstring::size_type length = 0;
		if ((length = data.find(L"|")) != std::wstring::npos) {
			eventCode = _ttoi((data.substr(offset, length)).c_str());
			offset = offset + length + 1;
		}
		if ((length = data.find(L"|", offset)) != std::wstring::npos) {
			scanCount = _ttoi((data.substr(offset, length - offset)).c_str());
			offset = length + 1;
		}
		if ((length = data.find(L"|", offset)) != std::wstring::npos) {
			totalCount = _ttoi((data.substr(offset, length - offset)).c_str());
			offset = length + 1;
		}
        path = &data[offset];

        WaitForSingleObject(m_IPSMutex,INFINITE);
        std::vector<DWORD>::iterator iter;
        for (iter = m_NotifyThreadIDs.begin(); iter != m_NotifyThreadIDs.end(); iter++) {
            xl_ds_api::CScanInfo* scanInfo = new xl_ds_api::CScanInfo();
            scanInfo->m_EventCode = eventCode;
			scanInfo->m_ScanCount = scanCount;
			scanInfo->m_TotalCount = totalCount;
			scanInfo->m_Path = path;
            BOOL ret = PostThreadMessage(*iter, SCAN_MSG_IMG_PROCESS, reinterpret_cast<WPARAM>(scanInfo), 0);
        }
        ReleaseMutex(m_IPSMutex);

        if (eventCode == SCAN_FINISH) {
            WaitForSingleObject(m_IPSMutex,INFINITE);
            m_NotifyThreadIDs.clear();
            ReleaseMutex(m_IPSMutex);
            break;
        }
    } while (success);

    CloseHandle(pipe);
    return 0;
}