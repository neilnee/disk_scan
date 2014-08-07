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

    class CScanPathInfo
    {
    public :
        CScanPathInfo() {} 
        virtual ~CScanPathInfo() {}
    public :
        INT m_EventCode;
		INT m_ScanCount;
		INT m_TotalCount;
        std::wstring m_Path;
    };

    class CScanFileInfo
    {
    public :
        CScanFileInfo() {}
        virtual ~CScanFileInfo() {}
    public :
        std::wstring m_Path;
        std::wstring m_Name;
    };

    class CDiskScan
    {
    public :
        CDiskScan();
        virtual ~CDiskScan();
    public :
        VOID StartPictureDirectoryScan(DWORD threadID, LPTSTR request);

		VOID StartPictrueAutoScan();

		VOID StartPictureManualScan(std::vector<std::wstring> paths);

		VOID AddMonitoringDirectory(std::vector<std::wstring> paths);
    };
}

#endif