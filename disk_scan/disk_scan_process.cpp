#include "stdafx.h"
#include "disk_scanner.h"

#define BUFF_SIZE 1024
#define COMM_TIMEOUT 5000

BOOL m_ImgScanning = FALSE;
LPTSTR m_PipeName = TEXT("\\\\.\\pipe\\xlspace_disk_scan_pipe");
HANDLE m_Thread = INVALID_HANDLE_VALUE;
HANDLE m_Mutex = CreateMutex(NULL,FALSE,NULL);
xl_ds_api::CScanner* m_Scanner = NULL;
std::vector<HANDLE> m_Pipes;

void ScanTargetCallback(INT event, INT scan, std::wstring directory);
DWORD WINAPI ThreadExecute(LPVOID lpParam);

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
    if (m_Scanner == NULL) {
        m_Scanner = new xl_ds_api::CScanner();
    }
    for(;;) {
		HANDLE pipe = CreateNamedPipe(
			m_PipeName,
			PIPE_ACCESS_DUPLEX |
			FILE_FLAG_OVERLAPPED,
			PIPE_TYPE_MESSAGE |
			PIPE_READMODE_MESSAGE |
			PIPE_WAIT,
			PIPE_UNLIMITED_INSTANCES,
			BUFF_SIZE,
			BUFF_SIZE,
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
			DWORD waitRet = WaitForSingleObject(connEvent, COMM_TIMEOUT);
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
			std::wstring operateStr;
			while(!success) {
				OVERLAPPED readOverl;
				HANDLE readEvent;
				HANDLE heap = GetProcessHeap();
				TCHAR* operate = (TCHAR*)HeapAlloc(heap, 0, BUF_SIZE*sizeof(TCHAR));
				DWORD readBytes = 0;
				readEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
				readOverl.hEvent = readEvent;
				readOverl.Offset = 0;
				readOverl.OffsetHigh = 0;
				operateStr.clear();
				success = ReadFile(
					pipe,
					operate,
					BUF_SIZE*sizeof(TCHAR),
					&readBytes,
					&readOverl);
				if (!success) {
					DWORD transByts;
					DWORD waitResult = WaitForSingleObject(readEvent, COMM_TIMEOUT);
					if (waitResult == WAIT_TIMEOUT) {
						// 等待客户端命令超时
						// 判定当前是否有工作线程运行，如果没有工作线程运行，则超时退出，否则继续等待
						continue;
					}
					success = GetOverlappedResult(pipe, &readOverl, &transByts, FALSE);
				}
				operateStr = operate;
			}
           if (success) {
               if (operateStr == L"scan_img") {
                   if (!m_ImgScanning) {
                       m_ImgScanning = TRUE;
                       DWORD tid = 0;
                       m_Thread = CreateThread(
                           NULL,
                           0,
                           ThreadExecute,
                           NULL,
                           0,
                           &tid);
                   }
                   WaitForSingleObject(m_Mutex,INFINITE);
                   m_Pipes.push_back(pipe);
                   ReleaseMutex(m_Mutex);
               } else {
                   DisconnectNamedPipe(pipe);
                   CloseHandle(pipe);
               }
           } else {
               DisconnectNamedPipe(pipe);
               CloseHandle(pipe);
           }
        }
    }
    delete m_Scanner;
    return 0;
}

void ScanTargetCallback(INT event, INT scan, std::wstring directory) {
	WaitForSingleObject(m_Mutex,INFINITE);	
	std::vector<HANDLE>::iterator iter;
	for (iter=m_Pipes.begin(); iter!=m_Pipes.end(); iter++) {
		if (*iter != INVALID_HANDLE_VALUE) {
			DWORD write = (lstrlen(directory.c_str())+1) * sizeof(TCHAR);
			DWORD written;
			BOOL success = WriteFile(
				*iter,
				directory.c_str(),
				write,
				&written,
				NULL);
			if (directory == L"scan_end") {
				DisconnectNamedPipe(*iter);
				CloseHandle(*iter);
			}
		}
	}
	if (directory == L"scan_end") {
		m_Pipes.clear();
	}
	ReleaseMutex(m_Mutex);
}

DWORD WINAPI ThreadExecute(LPVOID lpParam)
{
	std::vector<std::wstring> picDirs;
	m_Scanner->SetScanTargetCallback(ScanTargetCallback);
	m_Scanner->ScanTargetDir(&m_Scanner->m_PriorityDirs, picDirs, TRUE);
	m_Scanner->ScanTargetDir(&m_Scanner->m_BaseDirs, picDirs, FALSE);

	ScanTargetCallback(0,0,L"scan_end");
	m_ImgScanning = FALSE;
	return 0;
}
