#include "stdafx.h"
#include "disk_scan_tool.h"

void ScanTargetCallback(INT event, INT scan, std::wstring directory) {
    
}

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
    // �־û����Ŀ¼
    // �־û����ļ���Ϣɨ��ӿ�
    // ����̵�ͨ��
    //xl_ds_api::CScanner* scanner = new xl_ds_api::CScanner();
    //std::vector<std::wstring> picDirs;
    //scanner->SetScanTargetCallback(ScanTargetCallback);
    //scanner->ScanTargetDir(&scanner->m_PriorityDirs, picDirs, TRUE);
    //scanner->ScanTargetDir(&scanner->m_BaseDirs, picDirs, FALSE);
    return 0;
}