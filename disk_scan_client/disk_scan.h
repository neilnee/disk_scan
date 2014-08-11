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

#define SCAN_PATH_START 1
#define SCAN_PATH_FOUND 2
#define SCAN_PATH_RESULT 3
#define SCAN_PATH_FINISH 4
#define SCAN_PATH_STOP 5
#define LOAD_DIR_DONE 6
#define ADD_DIR_SUCCESS 7
#define ADD_DIR_FAILED 8
#define SCAN_IMG_MANUAL_SUCCESS 9;
#define SCAN_IMG_MANUAL_FAILED 10;

namespace xl_ds_api
{
	class CScanRequest
	{
	public :
		CScanRequest() {}
		virtual ~CScanRequest() {}
	public :
		LPTSTR m_RequestCode;
        std::vector<std::wstring> m_Paths;
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
        DWORD m_LastModifyHigh;
        DWORD m_LastModifyLow;
        DWORD m_FileSizeHigh;
        DWORD m_FileSizeLow;
        // ״̬��Ϣ��0��ʾ���ϴ���������ʾɾ��ʱ��
        INT64 m_State;
        // ��ʾ�������ݿ�ʱ��ʲô������ɾ��/���£�
        INT64 m_SqlExec;
    };

    class CScanResultEvent
    {
    public :
        CScanResultEvent() {} 
        virtual ~CScanResultEvent() {}
    public :
        UINT m_Msg;
        INT m_EventCode;
        INT m_ScanCount;
        INT m_TotalCount;
        std::vector<std::wstring> m_Paths;
        std::vector<xl_ds_api::CScanFileInfo> m_Files;
    };

    /**
     * ɨ�账���̹߳鲢���߳�
     */
    typedef void (*DiskScanResultNotify) (xl_ds_api::CScanResultEvent* result);

    class CDiskScan
    {
    public :
        CDiskScan();
        virtual ~CDiskScan();
    public :
        VOID StartPictureDirectoryScan(LPTSTR request);

		VOID StartPictrueAutoScan();

		VOID StartPictureManualScan(std::vector<std::wstring> paths);

		VOID AddMonitoringDirectory(std::vector<std::wstring> paths);

		VOID LoadMonitoringDirectory();

        VOID SetResultNotifyCallback(DiskScanResultNotify resultNotify);
    };
}

#endif