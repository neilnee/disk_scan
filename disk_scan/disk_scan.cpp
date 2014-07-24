#include "stdafx.h"
#include "disk_scan_tool.h"

#define BUFF_SIZE 1024
#define COMM_TIMEOUT 5000

HANDLE m_Pipe = INVALID_HANDLE_VALUE;
HANDLE m_Thread =INVALID_HANDLE_VALUE;
LPTSTR m_PipeName = TEXT("\\\\.\\pipe\\xlspace_disk_scan_pipe");
xl_ds_api::CScanner* m_Scanner = NULL;
//BOOL m_Executing = FALSE;

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
        if (m_Pipe == INVALID_HANDLE_VALUE) {
            m_Pipe = CreateNamedPipe(
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
        }
        if (m_Pipe == INVALID_HANDLE_VALUE) {
            return -1;
        }

		OVERLAPPED connOverl;
		HANDLE connEvent  = CreateEvent(NULL, TRUE, TRUE, NULL);
		connOverl.hEvent = connEvent;

        BOOL connected = ConnectNamedPipe(m_Pipe, &connOverl);
		DWORD lastError = GetLastError();
		if (lastError == ERROR_PIPE_CONNECTED) {
			connected = TRUE;
		}

		if (!connected) {
			// 如果没有客户端连接，进入超时等待
			DWORD waitRet = WaitForSingleObject(connEvent, COMM_TIMEOUT);
			if (waitRet == WAIT_TIMEOUT) {
				DisconnectNamedPipe(m_Pipe);
				// 判断当前是否有客户进程存在，如果没有则退出程序，否则继续等待
				continue;
			}
			DWORD transBytes;
			connected = GetOverlappedResult(m_Pipe, &connOverl, &transBytes, FALSE);
		}

        if (connected) {
			// 获取操作命令是否成功
            BOOL success = FALSE;
			OVERLAPPED readOverl;
			HANDLE readEvent;
            HANDLE heap = GetProcessHeap();
            TCHAR* operate = (TCHAR*)HeapAlloc(heap, 0, BUF_SIZE*sizeof(TCHAR));
            DWORD readBytes = 0;
			std::wstring operateStr;

			do {
				readEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
				readOverl.hEvent = readEvent;
				readOverl.Offset = 0;
				readOverl.OffsetHigh = 0;
				operateStr.clear();
				success = ReadFile(
					m_Pipe,
					operate,
					BUF_SIZE*sizeof(TCHAR),
					&readBytes,
					&readOverl);
				if (!success) {
					DWORD transByts;
					DWORD waitResult = WaitForSingleObject(readEvent, COMM_TIMEOUT);
					if (waitResult == WAIT_TIMEOUT) {
						// 判定当前是否有工作线程运行，如果没有工作线程运行，则超时退出，否则继续等待
						continue;
					}
					success = GetOverlappedResult(m_Pipe, &readOverl, &transByts, FALSE);
				}
				operateStr = operate;
			} while (!success);
            
           if (success && operateStr == L"scan_img") {
			   // 执行命令使用线程处理
		   		std::vector<std::wstring> picDirs;
		   		m_Scanner->SetScanTargetCallback(ScanTargetCallback);
		   		m_Scanner->ScanTargetDir(&m_Scanner->m_PriorityDirs, picDirs, TRUE);
		   		m_Scanner->ScanTargetDir(&m_Scanner->m_BaseDirs, picDirs, FALSE);
			   
		   		LPTSTR endMsg = TEXT("scan_end");
		   		DWORD write = (lstrlen(endMsg)+1) * sizeof(TCHAR);
		   		DWORD written;
		   		WriteFile(
		   			m_Pipe,
		   			endMsg,
		   			write,
		   			&written,
		   			NULL);
		   		DisconnectNamedPipe(m_Pipe);

			   /*DWORD tid = 0;
			   m_Thread = CreateThread(
				   NULL,
				   0,
				   ThreadExecute,
				   NULL,
				   0,
				   &tid);*/
           } else {
               DisconnectNamedPipe(m_Pipe);
           }
        }
    }

    delete m_Scanner;

    return 0;
}

void ScanTargetCallback(INT event, INT scan, std::wstring directory) {
    if (m_Pipe != INVALID_HANDLE_VALUE) {
        DWORD write = (lstrlen(directory.c_str())+1) * sizeof(TCHAR);
        DWORD written;
        BOOL success = WriteFile(
            m_Pipe,
            directory.c_str(),
            write,
            &written,
            NULL);
    }
}

//DWORD WINAPI ThreadExecute(LPVOID lpParam)
//{
//	std::vector<std::wstring> picDirs;
//	m_Scanner->SetScanTargetCallback(ScanTargetCallback);
//	m_Scanner->ScanTargetDir(&m_Scanner->m_PriorityDirs, picDirs, TRUE);
//	m_Scanner->ScanTargetDir(&m_Scanner->m_BaseDirs, picDirs, FALSE);
//
//	LPTSTR endMsg = TEXT("scan_end");
//	DWORD write = (lstrlen(endMsg)+1) * sizeof(TCHAR);
//	DWORD written;
//	WriteFile(
//		m_Pipe,
//		endMsg,
//		write,
//		&written,
//		NULL);
//	DisconnectNamedPipe(m_Pipe);
//	return 0;
//}
