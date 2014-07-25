#include "stdafx.h"

#define BUFF_SIZE 1024
#define COMM_TIMEOUT 5000

HANDLE m_Pipe = INVALID_HANDLE_VALUE;
LPTSTR m_PipeName = TEXT("\\\\.\\pipe\\xlspace_disk_scan_pipe");

// todo
// 封装独立线程处理连接、请求扫描、进度回调
// 合并请求，分发回调
// 处理进程通信的序列化
// 处理启动逻辑，增加调起扫描进程的逻辑
// 扫描进程增加自动退出的逻辑
// 增加扫描监控目录变化的模块
// 增加扫描进程对命令行参数的处理

int _tmain(int argc, _TCHAR* argv[])
{
	BOOL success = FALSE;
    for(;;) {
        m_Pipe = CreateFile(
            m_PipeName,
            GENERIC_READ |
            GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED,
            NULL);
        if (m_Pipe != INVALID_HANDLE_VALUE) {
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
        if (!WaitNamedPipe(m_PipeName, COMM_TIMEOUT)) {
            // 连接超时
            continue;
        }
    }

	DWORD dMode = PIPE_READMODE_MESSAGE;
	success = SetNamedPipeHandleState(
		m_Pipe,
		&dMode,
		NULL,
		NULL);
	if (!success) {
		return -1;
	}

    LPTSTR message = TEXT("scan_img");
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
			m_Pipe,
			message,
			dWrite,
			&dWritten,
			&overl);
		WaitForSingleObject(even, COMM_TIMEOUT);
		DWORD transBytes;
		success = GetOverlappedResult(m_Pipe, &overl, &transBytes, FALSE);
	} while (!success);

    TCHAR buf[BUFF_SIZE];
    DWORD dRead;
	do {
		success = ReadFile(
			m_Pipe,
			buf,
			BUFF_SIZE*sizeof(TCHAR),
			&dRead,
			NULL);
		std::wstring path = buf;
		_tprintf(TEXT("scan : %s \n"), path.c_str());
		if (path == L"scan_end") {
			break;
		}
	} while (success);

	CloseHandle(m_Pipe);

	return 0;
}