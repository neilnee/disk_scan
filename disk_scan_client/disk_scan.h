#ifndef _DISK_SCAN_H_
#define _DISK_SCAN_H_

#include <vector>
#include <string>

#define DSMSG_DIR_SCAN WM_USER + 500
#define DSMSG_PIC_SCAN WM_USER + 501
#define DSMSG_LOAD_DIR WM_USER + 502
#define DSMSG_ADD_DIR WM_USER +503

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
        std::wstring m_FullPath;
        std::wstring m_Path;
        std::wstring m_Name;
        std::string m_CID;
		// 状态信息，0表示已上传；其它表示删除时间
        INT64 m_State;
		// 表示更新数据库时是什么操作（删除/更新）
		INT64 m_SqlExec;
        DWORD m_LastModifyHigh;
        DWORD m_LastModifyLow;
        DWORD m_FileSizeHigh;
        DWORD m_FileSizeLow;
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

		VOID LoadMonitoringDirectory();
    };
}

#endif