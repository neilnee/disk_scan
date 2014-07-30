#include "stdafx.h"
#include "disk_scan.h"

DWORD m_MainThreadID;

// todo
// ���������߼������ӵ���ɨ����̵��߼�
// ɨ����������Զ��˳����߼�
// ����ɨ����̶������в����Ĵ���
// ������������Ŀ¼������ɨ��
// ����ɨ����Ŀ¼�仯��ģ��
// Ŀ¼���ʱ��ɾ������һ��ʱ��ļ�¼������

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

