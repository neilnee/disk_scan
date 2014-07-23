#include "stdafx.h"
#include "disk_scan_tool.h"

INT m_Count = 0;

void callback(INT event, INT scan, std::wstring directory) {
    m_Count = scan;
}

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
    // 持久化监控目录
    // 持久化的文件信息扫描接口
    // 跨进程的通信
    xl_ds_api::CScanner* scanner = new xl_ds_api::CScanner();
    std::vector<std::wstring> picDirs;
    scanner->SetScanTargetCallback(callback);
    scanner->ScanTargetDir(&scanner->m_PriorityDirs, picDirs, TRUE);
    scanner->ScanTargetDir(&scanner->m_BaseDirs, picDirs, FALSE);
    return 0;
}