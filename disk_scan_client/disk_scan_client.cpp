#include "stdafx.h"
#include "disk_scan.h"

DWORD m_MainThreadID;

VOID DiskScanResultNotify(xl_ds_api::CScanResultEvent* result)
{
    if (result != NULL) {
        PostThreadMessage(m_MainThreadID, result->m_Msg, reinterpret_cast<WPARAM>(result), 0);
    }
}

int _tmain(int argc, _TCHAR* argv[])
{
	MSG msg;
	BOOL ret;

	m_MainThreadID = GetCurrentThreadId();

    xl_ds_api::CDiskScan* diskScan = new xl_ds_api::CDiskScan();
    diskScan->SetResultNotifyCallback(DiskScanResultNotify);
    diskScan->StartPictureDirectoryScan(SCAN_REQUEST_IMG);
    //diskScan->StartPictrueAutoScan();

    std::vector<std::wstring> paths;
    while((ret = GetMessage( &msg, NULL, 0, 0 )) != 0) {
        if (ret == -1) {
            break;
        } else {
            if (msg.message > WM_USER) {
				if (msg.message == DSMSG_DIR_SCAN) {
					xl_ds_api::CScanResultEvent* infoPtr = reinterpret_cast<xl_ds_api::CScanResultEvent*>(msg.wParam);
					if (infoPtr == NULL) {
						continue;
					}
					xl_ds_api::CScanResultEvent info = *infoPtr;
					delete infoPtr;
                    _tprintf(TEXT("%d, %d, %d, %s\n"), info.m_EventCode, info.m_ScanCount, info.m_TotalCount, info.m_Paths.at(0).c_str());
                    if (info.m_EventCode == SCAN_PATH_RESULT) {
                        paths.push_back(info.m_Paths.at(0));
                    }
                    if (info.m_EventCode == SCAN_PATH_FINISH) {
                        diskScan->AddMonitoringDirectory(paths);
                    }
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

