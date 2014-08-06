#include "stdafx.h"
#include <windows.h>
#include <vector>
#include <ShlObj.h>
#include "disk_scan.h"
#include "sqlite3.h"

#define PIPE_BUF_SIZE 1024
#define TIMEOUT 12000

using namespace xl_ds_api;

std::vector<DWORD> m_NotifyThreadIDs;
BOOL m_ImgProcessScanning = FALSE;
HANDLE m_IPSMutex = CreateMutex(NULL,FALSE,NULL);

DWORD WINAPI ScanImgProcessExecute(LPVOID lpParam);

CDiskScan::CDiskScan()
{
}

CDiskScan::~CDiskScan()
{
    m_NotifyThreadIDs.clear();
}

VOID CDiskScan::ScanImgInProcess(DWORD threadID, LPTSTR requestCode)
{
	xl_ds_api::CScanRequest* request = new xl_ds_api::CScanRequest();
	request->m_ThreadID = threadID;
	request->m_RequestCode = requestCode;
    
    CreateThread(
        NULL,
        0,
        ScanImgProcessExecute,
        (LPVOID) request,
        0,
        &threadID);
}

DWORD WINAPI ScanImgProcessExecute(LPVOID lpParam)
{
    BOOL success = FALSE;
	LPTSTR pipeName = TEXT("\\\\.\\pipe\\xlspace_disk_scan_pipe");
	xl_ds_api::CScanRequest* requestPtr = (xl_ds_api::CScanRequest*) lpParam;
	if (requestPtr == NULL) {
		return -1;
	}
	xl_ds_api::CScanRequest request = *requestPtr;
	delete requestPtr;

    WaitForSingleObject(m_IPSMutex, INFINITE);
    m_NotifyThreadIDs.push_back(request.m_ThreadID);
    if(m_ImgProcessScanning) {
		success = TRUE;
    } else {
		m_ImgProcessScanning = TRUE;
    }
    ReleaseMutex(m_IPSMutex);

    if (success) {
        return 0;
    }

    HANDLE pipe = INVALID_HANDLE_VALUE;
    for(;;) {
        pipe = CreateFile(
            pipeName,
            GENERIC_READ |
            GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED,
            NULL);
        if (pipe != INVALID_HANDLE_VALUE) {
            // 连接成功
            break;
        }
        if (GetLastError() == ERROR_FILE_NOT_FOUND) {
            // 没有扫描进程启动，启动扫描进程
			TCHAR path[MAX_PATH];
			GetModuleFileName(NULL, path, MAX_PATH);
			std::wstring parentPath = path;
			parentPath = parentPath.substr(0, parentPath.rfind(L"\\"));
			ShellExecute(NULL, NULL, L"disk_scan_process.exe", L"scan_img", parentPath.c_str(), 0);
            continue;
        } else if (GetLastError() != ERROR_PIPE_BUSY) {
            if (pipe != INVALID_HANDLE_VALUE) {
                CloseHandle(pipe);
            }
            CloseHandle(m_IPSMutex);
            return -1;
        }
        if (!WaitNamedPipe(pipeName, TIMEOUT)) {
            // 连接超时
            continue;
        }
    }

    DWORD dMode = PIPE_READMODE_MESSAGE;
    success = SetNamedPipeHandleState(
        pipe,
        &dMode,
        NULL,
        NULL);
    if (!success) {
        return -1;
    }

    DWORD dWrite;
    DWORD dWritten;
    OVERLAPPED overl;
    HANDLE even = CreateEvent(NULL,TRUE,TRUE,NULL);
    overl.hEvent = even;
    overl.Offset = 0;
    overl.OffsetHigh = 0;
    dWrite = (lstrlen(request.m_RequestCode)+1) * sizeof(TCHAR);
    do {
        success = WriteFile(
            pipe,
            request.m_RequestCode,
            dWrite,
            &dWritten,
            &overl);
        WaitForSingleObject(even, TIMEOUT);
        DWORD transBytes;
        success = GetOverlappedResult(pipe, &overl, &transBytes, FALSE);
    } while (!success);
    CloseHandle(even);
	even = INVALID_HANDLE_VALUE;

    TCHAR buf[PIPE_BUF_SIZE];
    DWORD dRead;
    do {
        success = ReadFile(
            pipe,
            buf,
            PIPE_BUF_SIZE*sizeof(TCHAR),
            &dRead,
            NULL);
        std::wstring data = buf;
		std::wstring path;
		INT eventCode = 0;
		INT scanCount = 0;
		INT totalCount = 0;
		std::wstring::size_type offset = 0;
        std::wstring::size_type length = 0;
		if ((length = data.find(L"|")) != std::wstring::npos) {
			eventCode = _ttoi((data.substr(offset, length)).c_str());
			offset = offset + length + 1;
		}
		if ((length = data.find(L"|", offset)) != std::wstring::npos) {
			scanCount = _ttoi((data.substr(offset, length - offset)).c_str());
			offset = length + 1;
		}
		if ((length = data.find(L"|", offset)) != std::wstring::npos) {
			totalCount = _ttoi((data.substr(offset, length - offset)).c_str());
			offset = length + 1;
		}
        path = &data[offset];

        WaitForSingleObject(m_IPSMutex, INFINITE);
        std::vector<DWORD>::iterator iter;
        for (iter = m_NotifyThreadIDs.begin(); iter != m_NotifyThreadIDs.end(); iter++) {
            xl_ds_api::CScanInfo* scanInfo = new xl_ds_api::CScanInfo();
            scanInfo->m_EventCode = eventCode;
			scanInfo->m_ScanCount = scanCount;
			scanInfo->m_TotalCount = totalCount;
			scanInfo->m_Path = path;
            PostThreadMessage(*iter, SCAN_MSG_IMG_PROCESS, reinterpret_cast<WPARAM>(scanInfo), 0);
        }
        ReleaseMutex(m_IPSMutex);

        if (eventCode == SCAN_FINISH || eventCode == SCAN_STOP) {
            WaitForSingleObject(m_IPSMutex, INFINITE);
            m_NotifyThreadIDs.clear();
            ReleaseMutex(m_IPSMutex);
            break;
        }
    } while (success);

    CloseHandle(m_IPSMutex);
	m_IPSMutex = INVALID_HANDLE_VALUE;
    CloseHandle(pipe);
	pipe = INVALID_HANDLE_VALUE;
    return 0;
}

VOID CDiskScan::ScanImgChange(DWORD threadID)
{
    // 数据库写入分为两张表：目录监控表和文件监控表
    // 首先读入目录表，得到需要扫描的目录列表
    // 开始遍历目录列表中的每个目录
    // 读取文件表中该目录的所有监控状态
    // 遍历该目录下的全部文件，逐个将每个文件与读取的文件表做比对：
    // 1、文件路径相同，CID+FileSize相同，认为是同一文件
    // 处理比对结果：
    // 1、数据库有、扫描结果有的文件：
    // 2、数据库没有、扫描结果有的文件：加入上传队列
    // 3、数据库有，扫描结果没有的文件：标记已删除，如果首次标记已删除，则增加删除时间
    // 4、清理删除时间超过一定时间的记录

    // 遍历完全部目录将结果返回回调


    //TCHAR path[MAX_PATH];
    //GetModuleFileName(NULL, path, MAX_PATH);
    //std::wstring parentPath = path;
    //parentPath = parentPath.substr(0, parentPath.rfind(L"\\"));
    //parentPath.append(L"\\scan_result.dat");

    //sqlite3 *db;
    //INT ret = sqlite3_open16(parentPath.c_str(), &db);
    //if (ret != SQLITE_OK) {
    //    // 打开数据库失败
    //    return;
    //}
    //
    //sqlite3_stmt* stmt;
    //LPCWSTR lpszTail = NULL;
    //ret = sqlite3_prepare16_v2(db, L"SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='scan_result'", -1, &stmt, (const void**)&lpszTail);

    //ret = sqlite3_column_count(stmt);

    //INT i = 0;
}

BOOL OpenDB(sqlite3 &db, sqlite3_stmt &stmt)
{
    // 打开数据库，检测是否存在文件变化监控状态表，如果不存在则创建该表
    // 打开完成以后返回数据库和操作的stmt
    // 失败返回FALSE
    return FALSE;
}

BOOL ReadDB(sqlite3* db, sqlite3_stmt* stmt)
{
    // 读取一条监控数据，读完返回FALSE
    return FALSE;
}

BOOL WriteDB(sqlite3* db, sqlite3_stmt* stmt)
{
    // 将一条监控数据写入数据库
    return FALSE;
}

BOOL CloseDB(sqlite3* db, sqlite3_stmt* stmt)
{
    // 关闭数据库，释放资源
    return FALSE;
}
