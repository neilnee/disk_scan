#include "stdafx.h"
#include "disk_scan.h"

DWORD m_MainThreadID;

// todo
// 处理启动逻辑，增加调起扫描进程的逻辑
// 扫描进程增加自动退出的逻辑
// 增加扫描进程对命令行参数的处理
// 处理对最近访问目录的优先扫描
// 增加扫描监控目录变化的模块
// 目录监控时对删除超过一定时间的记录的清理

int _tmain(int argc, _TCHAR* argv[])
{
	MSG msg;
	BOOL ret;

	m_MainThreadID = GetCurrentThreadId();

    xl_ds_api::CDiskScan* diskScan = new xl_ds_api::CDiskScan();
    diskScan->ScanImgInProcess(m_MainThreadID, SCAN_REQUEST_IMG_AFREAH);

    while((ret = GetMessage( &msg, NULL, 0, 0 )) != 0) {
        if (ret == -1) {
            break;
        } else {
            if (msg.message > WM_USER) {
				if (msg.message == SCAN_MSG_IMG_PROCESS) {
					xl_ds_api::CScanInfo* infoPtr = reinterpret_cast<xl_ds_api::CScanInfo*>(msg.wParam);
					if (infoPtr == NULL) {
						continue;
					}
					xl_ds_api::CScanInfo info = *infoPtr;
					delete infoPtr;

					_tprintf(TEXT("%d, %d, %d, %s\n"), info.m_EventCode, info.m_ScanCount, info.m_TotalCount, info.m_Path.c_str());
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

