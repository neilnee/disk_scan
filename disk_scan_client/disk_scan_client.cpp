#include "stdafx.h"

#define BUF_SIZE 512

HANDLE m_Pipe = INVALID_HANDLE_VALUE;
LPTSTR m_PipeName = TEXT("\\\\.\\pipe\\xlspace_disk_scan_pipe");

int _tmain(int argc, _TCHAR* argv[])
{
    TCHAR buf[BUF_SIZE];
    DWORD dRead;
    DWORD dWrite;
    DWORD dWritten;
    DWORD dMode;
    BOOL success;
    LPTSTR message = TEXT("scan_img");

    while(1) {
        m_Pipe = CreateFile(
            m_PipeName,
            GENERIC_READ |
            GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            0,
            NULL);

        if (m_Pipe != INVALID_HANDLE_VALUE) {
            break;
        }

        if (GetLastError() != ERROR_PIPE_BUSY) {
            return -1;
        }

        if (!WaitNamedPipe(m_PipeName, 20000)) {
            return -1;
        }
    }

    dMode = PIPE_READMODE_MESSAGE;
    success = SetNamedPipeHandleState(
        m_Pipe,
        &dMode,
        NULL,
        NULL);

    if (!success) {
        return -1;
    }

    dWrite = (lstrlen(message)+1) * sizeof(TCHAR);
    success = WriteFile(
        m_Pipe,
        message,
        dWrite,
        &dWritten,
        NULL);

    if (!success) {
        
        return -1;
    }

    do {
        success = ReadFile(
            m_Pipe,
            buf,
            BUF_SIZE*sizeof(TCHAR),
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

