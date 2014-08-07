#ifndef _DISK_SCAN_H_
#define _DISK_SCAN_H_

#include <vector>
#include <string>

#define SCAN_MSG_IMG_PROCESS WM_USER + 500

#define SCAN_REQUEST_IMG L"scan_img"
#define SCAN_REQUEST_IMG_AFREAH L"scan_img_afreah"
#define SCAN_REQUEST_EXIT L"scan_exit"

#define SCAN_START 1
#define SCAN_FOUND 2
#define SCAN_RESULT 3
#define SCAN_FINISH 4
#define SCAN_STOP 5

namespace xl_ds_api
{
	class CScanRequest
	{
	public :
		CScanRequest() {}
		virtual ~CScanRequest() {}
	public :
		DWORD m_ThreadID;
		LPTSTR m_RequestCode;
	};

    class CScanInfo
    {
    public :
        CScanInfo() {} 
        virtual ~CScanInfo() {}
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
        virtual ~CDiskScan();
    public :
        VOID ScanImgInProcess(DWORD threadID, LPTSTR request);

        VOID ScanImgChange(std::vector<std::wstring> &paths);
    };
}

#endif