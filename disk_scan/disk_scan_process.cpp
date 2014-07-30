#include "stdafx.h"
#include "disk_scanner.h"

#define PIPE_BUF_SIZE 1024
#define TIMEOUT 12000
#define SCAN_REQUEST_IMG L"scan_img"
#define SCAN_REQUEST_IMG_AFREAH L"scan_img_afreah"
#define SCAN_REQUEST_EXIT L"scan_exit"

BOOL m_ImgScanning = FALSE;
HANDLE m_Thread = INVALID_HANDLE_VALUE;
HANDLE m_Mutex = CreateMutex(NULL,FALSE,NULL);
xl_ds_api::CScanner* m_Scanner = NULL;
std::vector<HANDLE> m_Pipes;

VOID ScanTargetCallback(INT eventCode, INT scanCount, INT totalCount, std::wstring directory);
DWORD WINAPI ThreadExecute(LPVOID lpParam);
BOOL HandleReuqest(std::wstring request, HANDLE pipe);

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
    if (m_Scanner == NULL) {
        m_Scanner = new xl_ds_api::CScanner();
    }
	LPTSTR pipeName = TEXT("\\\\.\\pipe\\xlspace_disk_scan_pipe");
    for(;;) {
		HANDLE pipe = CreateNamedPipe(
			pipeName,
			PIPE_ACCESS_DUPLEX |
			FILE_FLAG_OVERLAPPED,
			PIPE_TYPE_MESSAGE |
			PIPE_READMODE_MESSAGE |
			PIPE_WAIT,
			PIPE_UNLIMITED_INSTANCES,
			PIPE_BUF_SIZE,
			PIPE_BUF_SIZE,
			0,
			NULL);
        if (pipe == INVALID_HANDLE_VALUE) {
            continue;
		}

		OVERLAPPED connOverl;
		HANDLE connEvent  = CreateEvent(NULL, TRUE, TRUE, NULL);
		connOverl.hEvent = connEvent;

        BOOL connected = ConnectNamedPipe(pipe, &connOverl);
		DWORD lastError = GetLastError();
		if (lastError == ERROR_PIPE_CONNECTED) {
			connected = TRUE;
		}

		if (!connected) {
			// 如果没有客户端连接，进入超时等待
			DWORD waitRet = WaitForSingleObject(connEvent, TIMEOUT);
			if (waitRet == WAIT_TIMEOUT) {
				DisconnectNamedPipe(pipe);
				CloseHandle(pipe);
				// 等待客户端连接超时
				// 判断当前是否有客户进程存在，如果没有则退出程序，否则继续等待
				continue;
			}
			DWORD transBytes;
			connected = GetOverlappedResult(pipe, &connOverl, &transBytes, FALSE);
		}
        if (connected) {
            BOOL success = FALSE;
			std::wstring request;
			while(!success) {
				OVERLAPPED readOverl;
				HANDLE readEvent;
				HANDLE heap = GetProcessHeap();
				TCHAR* operate = (TCHAR*)HeapAlloc(heap, 0, PATH_BUF_SIZE*sizeof(TCHAR));
				DWORD readBytes = 0;
				readEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
				readOverl.hEvent = readEvent;
				readOverl.Offset = 0;
				readOverl.OffsetHigh = 0;
				request.clear();
				success = ReadFile(
					pipe,
					operate,
					PATH_BUF_SIZE*sizeof(TCHAR),
					&readBytes,
					&readOverl);
				if (!success) {
					DWORD transByts;
					DWORD waitResult = WaitForSingleObject(readEvent, TIMEOUT);
					if (waitResult == WAIT_TIMEOUT) {
						// 等待客户端命令超时
						// 判定当前是否有工作线程运行，如果没有工作线程运行，则超时退出，否则继续等待
						continue;
					}
					success = GetOverlappedResult(pipe, &readOverl, &transByts, FALSE);
				}
				request = operate;
			}
           if (!success || !HandleReuqest(request, pipe)) {
			   DisconnectNamedPipe(pipe);
			   CloseHandle(pipe);
           }
        }
    }
    delete m_Scanner;
    return 0;
}

BOOL HandleReuqest(std::wstring request, HANDLE pipe)
{
	BOOL result = FALSE;
	if (pipe != INVALID_HANDLE_VALUE) {
		WaitForSingleObject(m_Mutex,INFINITE);
		m_Pipes.push_back(pipe);
		ReleaseMutex(m_Mutex);
	}
	if (request == SCAN_REQUEST_IMG || request == SCAN_REQUEST_IMG_AFREAH) {	
		if (!m_ImgScanning) {
			m_ImgScanning = TRUE;
			if (request == SCAN_REQUEST_IMG_AFREAH) {
				m_Scanner->ClearResult();
			}
			DWORD tid = 0;
			m_Thread = CreateThread(
				NULL,
				0,
				ThreadExecute,
				NULL,
				0,
				&tid);
		}
		result = TRUE;
	} else if (request == SCAN_REQUEST_EXIT) {
		if (m_Thread != INVALID_HANDLE_VALUE) {
			TerminateThread(m_Thread, 0);
			CloseHandle(m_Thread);
		}
		ScanTargetCallback(SCAN_STOP, m_Scanner->m_ScanDirs, m_Scanner->m_TotalDirs, L"");
		result = TRUE;
		ExitProcess(0);
	}
	return result;
}

VOID ScanTargetCallback(INT eventCode, INT scanCount, INT totalCount, std::wstring directory) 
{
    wchar_t buf[PATH_BUF_SIZE] = {0};
    wsprintf(buf, L"%d|%d|%d|%s", eventCode, scanCount, totalCount, directory.c_str());

	WaitForSingleObject(m_Mutex,INFINITE);	
	std::vector<HANDLE>::iterator iter;
	for (iter=m_Pipes.begin(); iter!=m_Pipes.end(); iter++) {
		if (*iter != INVALID_HANDLE_VALUE) {
			DWORD write = (lstrlen(buf)+1) * sizeof(TCHAR);
			DWORD written;
			BOOL success = WriteFile(
				*iter,
				buf,
				write,
				&written,
				NULL);
			if (eventCode == SCAN_FINISH || eventCode == SCAN_STOP) {
				FlushFileBuffers(*iter);
				DisconnectNamedPipe(*iter);
				CloseHandle(*iter);
			}
		}
	}
	if (eventCode == SCAN_FINISH || eventCode == SCAN_STOP) {
		m_Pipes.clear();
	}
	ReleaseMutex(m_Mutex);
}

DWORD WINAPI ThreadExecute(LPVOID lpParam)
{
    if (!m_Scanner->m_Done) {
        m_Scanner->SetScanTargetCallback(ScanTargetCallback);
        m_Scanner->ScanTargetDir(&m_Scanner->m_PriorityDirs, m_Scanner->m_ImgDirectorys, TRUE);
        m_Scanner->ScanTargetDir(&m_Scanner->m_BaseDirs, m_Scanner->m_ImgDirectorys, FALSE);
        m_Scanner->m_Done = TRUE;
    }
    std::vector<std::wstring>::iterator iter;
    for (iter = m_Scanner->m_ImgDirectorys.begin(); iter != m_Scanner->m_ImgDirectorys.end(); iter++) {
        ScanTargetCallback(SCAN_RESULT, m_Scanner->m_ScanDirs, m_Scanner->m_TotalDirs, *iter);
    }
	ScanTargetCallback(SCAN_FINISH, m_Scanner->m_ScanDirs, m_Scanner->m_TotalDirs, L"");
	m_ImgScanning = FALSE;
    CloseHandle(m_Thread);
	return 0;
}
