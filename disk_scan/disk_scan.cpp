// disk_scan.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "disk_scan_tool.h"

int _tmain(int argc, _TCHAR* argv[])
{
    xl_ds_api::CScanner* scanner = new xl_ds_api::CScanner();
    scanner->ScanDirectory();
	return 0;
}

