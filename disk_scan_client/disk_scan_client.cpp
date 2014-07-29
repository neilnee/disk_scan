#include "stdafx.h"
#include "disk_scan.h"

HINSTANCE m_Hinstance;
HWND m_HWND;
DWORD m_MainThreadID;

// todo
// 处理进程通信的序列化
// 处理启动逻辑，增加调起扫描进程的逻辑
// 扫描进程增加自动退出的逻辑
// 增加扫描监控目录变化的模块
// 增加扫描进程对命令行参数的处理
// 处理对最近访问目录的优先扫描
// 目录监控时对删除超过一定时间的记录的清理

int _tmain(int argc, _TCHAR* argv[])
{
	MSG msg;
	BOOL ret;

	m_Hinstance = GetModuleHandle(NULL);
	m_HWND = GetConsoleWindow();
	m_MainThreadID = GetCurrentThreadId();

    xl_ds_api::CDiskScan* diskScan = new xl_ds_api::CDiskScan();
    diskScan->ScanImgInProcess(m_MainThreadID);

    while((ret = GetMessage( &msg, NULL, 0, 0 )) != 0) {
        if (ret == -1) {
            break;
        } else {
            if (msg.message > WM_USER) {
                xl_ds_api::CScanInfo* infoPtr = reinterpret_cast<xl_ds_api::CScanInfo*>(msg.wParam);
                if (infoPtr == NULL) {
                    continue;
                }
                xl_ds_api::CScanInfo info = *infoPtr;
                delete infoPtr;
                if (info.m_Info == SCAN_INFO_START_PATH) {
                    _tprintf(TEXT("scan : %s \n"), info.m_Path.c_str());
                } else if (info.m_Info == SCAN_INFO_FINISH) {
                    _tprintf(TEXT("scan finish"));
                }
            } else {
                TranslateMessage(&msg); 
                DispatchMessage(&msg);
            }
        }
    }
    delete diskScan;
	return msg.wParam;
}

