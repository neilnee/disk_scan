#include "stdafx.h"
#include "disk_scan_tool.h"

#define PIPE_BUF_SIZE 512

BOOL m_Connected = FALSE;
HANDLE m_Pipe = INVALID_HANDLE_VALUE;
LPTSTR m_PipeName = TEXT("\\\\.\\pipe\\xlspace_disk_scan_pipe");

void ScanTargetCallback(INT event, INT scan, std::wstring directory) {
    
}

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
    // 持久化监控目录
    // 持久化的文件信息扫描接口
    // 跨进程的通信
    //xl_ds_api::CScanner* scanner = new xl_ds_api::CScanner();
    //std::vector<std::wstring> picDirs;
    //scanner->SetScanTargetCallback(ScanTargetCallback);
    //scanner->ScanTargetDir(&scanner->m_PriorityDirs, picDirs, TRUE);
    //scanner->ScanTargetDir(&scanner->m_BaseDirs, picDirs, FALSE);

    for(;;) {
        m_Pipe = CreateNamedPipe(
            m_PipeName,
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE |
            PIPE_READMODE_MESSAGE |
            PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            PIPE_BUF_SIZE,
            PIPE_BUF_SIZE,
            0,
            NULL);

        if (m_Pipe == INVALID_HANDLE_VALUE) {
            return -1;
        }

        m_Connected = ConnectNamedPipe(m_Pipe, NULL) ? 
            TRUE : (GetLastError()==ERROR_PIPE_CONNECTED);

        if (m_Connected) {

        } else {
            CloseHandle(m_Pipe);
        }
    }

    return 0;
}