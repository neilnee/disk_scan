#include "stdafx.h"

#define BUFF_SIZE 1024
#define COMM_TIMEOUT 5000
#define MSG_WND_CLASS L"DSMsgWndClass"
#define MSG_WND_NAME L"DSMsgWnd"

HANDLE m_Pipe = INVALID_HANDLE_VALUE;
LPTSTR m_PipeName = TEXT("\\\\.\\pipe\\xlspace_disk_scan_pipe");

HINSTANCE m_Hinstance;
HWND m_HWND;
DWORD m_MainThreadID;

DWORD WINAPI ScanImgThread(LPVOID lpParam);
BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType);

// todo
// ��װ�����̴߳������ӡ�����ɨ�衢���Ȼص�
// �ϲ����󣬷ַ��ص�
// �������ͨ�ŵ����л�
// ���������߼������ӵ���ɨ����̵��߼�
// ɨ����������Զ��˳����߼�
// ����ɨ����Ŀ¼�仯��ģ��
// ����ɨ����̶������в����Ĵ���
// ������������Ŀ¼������ɨ��
// Ŀ¼���ʱ��ɾ������һ��ʱ��ļ�¼������

int _tmain(int argc, _TCHAR* argv[])
{
	MSG msg;
	BOOL ret;

	m_Hinstance = GetModuleHandle(NULL);
	m_HWND = GetConsoleWindow();
	m_MainThreadID = GetCurrentThreadId();

	DWORD tid = 0;
	CreateThread(
		NULL,
		0,
		ScanImgThread,
		NULL,
		0,
		&tid);

	SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
    while((ret = GetMessage( &msg, NULL, 0, 0 )) != 0) {
        if (ret == -1) {
            break;
        } else {
            TranslateMessage(&msg); 
            DispatchMessage(&msg); 
        }
    }
	SetConsoleCtrlHandler(ConsoleCtrlHandler, FALSE);

	return msg.wParam;
}

BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType)
{
	return FALSE;
}

DWORD WINAPI ScanImgThread(LPVOID lpParam)
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
		   // ���ӳɹ�
		   break;
	   }
	   if (GetLastError() == ERROR_FILE_NOT_FOUND) {
		   // û��ɨ���������������ɨ�����
		   continue;
	   }
	   if (GetLastError() != ERROR_PIPE_BUSY) {
		   return -1;
	   }
	   if (!WaitNamedPipe(m_PipeName, COMM_TIMEOUT)) {
		   // ���ӳ�ʱ
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
		BOOL ret = PostThreadMessage(m_MainThreadID, 1522, 200, 100);
		//_tprintf(TEXT("scan : %s \n"), path.c_str());
		if (path == L"scan_end") {
			break;
		}
	} while (success);

	CloseHandle(m_Pipe);
}