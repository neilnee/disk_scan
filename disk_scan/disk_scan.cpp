// disk_scan.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "disk_scan_tool.h"

INT m_Count = 0;

void callback(INT scan, std::wstring current) {
	m_Count = scan;
}

int _tmain(int argc, _TCHAR* argv[])
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

