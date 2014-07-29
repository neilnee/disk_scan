#ifndef _DISK_SCAN_H_
#define _DISK_SCAN_H_

#include <windows.h>

#define SCAN_MSG_IMG_PROCESS WM_USER + 500

#define SCAN_INFO_START_PATH 101
#define SCAN_INFO_FOUND_PATH 102
#define SCAN_INFO_FINISH 103

namespace xl_ds_api
{
    class CScanInfo
    {
    public :
        CScanInfo() {} 
        ~CScanInfo() {}

    public :
        INT m_Info;
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