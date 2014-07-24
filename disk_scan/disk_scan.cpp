#include "stdafx.h"
#include "disk_scan_tool.h"

#define BUF_SIZE 512

HANDLE m_Pipe = INVALID_HANDLE_VALUE;
LPTSTR m_PipeName = TEXT("\\\\.\\pipe\\xlspace_disk_scan_pipe");
xl_ds_api::CScanner* m_Scanner = NULL;

void ScanTargetCallback(INT event, INT scan, std::wstring directory);

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
                PIPE_ACCESS_DUPLEX,
                PIPE_TYPE_MESSAGE |
                PIPE_READMODE_MESSAGE |
                PIPE_WAIT,
                PIPE_UNLIMITED_INSTANCES,
                BUF_SIZE,
                BUF_SIZE,
                0,
                NULL);
        }
        if (m_Pipe == INVALID_HANDLE_VALUE) {
            return -1;
        }

        BOOL connected = ConnectNamedPipe(m_Pipe, NULL) ? 
            TRUE : (GetLastError()==ERROR_PIPE_CONNECTED);

        if (connected) {
            BOOL success = FALSE;
            HANDLE heap = GetProcessHeap();
            TCHAR* operate = (TCHAR*)HeapAlloc(heap, 0, BUF_SIZE*sizeof(TCHAR));
            DWORD readBytes = 0;
            
            success = ReadFile(
                m_Pipe,
                operate,
                BUF_SIZE*sizeof(TCHAR),
                &readBytes,
                NULL);
            std::wstring operateStr = operate;

           if (success && operateStr == L"scan_img") {
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
           } else {
               DisconnectNamedPipe(m_Pipe);
           }
        } else {
            DisconnectNamedPipe(m_Pipe);
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
