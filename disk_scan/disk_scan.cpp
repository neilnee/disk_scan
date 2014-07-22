// disk_scan.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "disk_scan_tool.h"

void callback(INT scan, std::wstring current) {

}

int _tmain(int argc, _TCHAR* argv[])
{
    // 过滤规则
    // 进度回调通知
    // 持久化监控目录
    // 持久化的文件信息扫描接口
    // 跨进程的通信
    xl_ds_api::CScanner* scanner = new xl_ds_api::CScanner();
	std::vector<std::wstring> picDirs;
    scanner->ScanPriorityDir();
    scanner->ScanBaseDir();
    scanner->ScanTargetDir(&scanner->m_BaseDirs, picDirs, callback);
    scanner->ScanTargetDir(&scanner->m_PriorityDirs, picDirs, callback);
	return 0;
}

