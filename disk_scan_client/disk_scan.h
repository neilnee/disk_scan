#ifndef _DISK_SCAN_H_
#define _DISK_SCAN_H_

#include <windows.h>

#define SCAN_MSG_IMG_PROCESS WM_USER + 500

#define SCAN_START 1
#define SCAN_FOUND 2
#define SCAN_RESULT 3
#define SCAN_FINISH 4

namespace xl_ds_api
{
    class CScanInfo
    {
    public :
        CScanInfo() {} 
        ~CScanInfo() {}

    public :
        INT m_EventCode;
		INT m_ScanCount;
		INT m_TotalCount;
        std::wstring m_Path;
    };

    class CDiskScan
    {
    public :
        CDiskScan();
        ~CDiskScan();

    public :
        void ScanImgInProcess(DWORD notifyThreadID);

    };
}

#endif