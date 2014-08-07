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
HANDLE m_IPSMutex = CreateMutex(NULL, FALSE, NULL);
HANDLE m_MPMutex = CreateMutex(NULL, FALSE, NULL);
HANDLE m_MFMutex =CreateMutex(NULL, FALSE, NULL);

std::wstring GetProcessPath();
std::wstring UTF8ToUTF16(const char*src);
std::string UTF16ToUTF8(const wchar_t* src);
VOID ReadMonitoringPath(std::vector<std::wstring> &paths);
VOID WriteMonitoringPath(std::vector<std::wstring> paths);
VOID ReadMonitoringFiles(std::vector<std::wstring> paths, std::vector<xl_ds_api::CScanFileInfo> &scanFiles);
VOID WriteMonitoringFiles(std::vector<xl_ds_api::CScanFileInfo> scanFiles);
DWORD WINAPI ScanImgProcessExecute(LPVOID lpParam);

CDiskScan::CDiskScan()
{
}

CDiskScan::~CDiskScan()
{
    m_NotifyThreadIDs.clear();
    if (m_IPSMutex != INVALID_HANDLE_VALUE) {
        CloseHandle(m_IPSMutex);
        m_IPSMutex = INVALID_HANDLE_VALUE;
    }
    if (m_MPMutex != INVALID_HANDLE_VALUE) {
        CloseHandle(m_MPMutex);
        m_MPMutex = INVALID_HANDLE_VALUE;
    }
    if (m_MFMutex != INVALID_HANDLE_VALUE) {
        CloseHandle(m_MFMutex);
        m_MFMutex = INVALID_HANDLE_VALUE;
    }
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
            std::wstring processPath = GetProcessPath();
			ShellExecute(NULL, NULL, L"disk_scan_process.exe", L"scan_img", processPath.c_str(), 0);
            continue;
        } else if (GetLastError() != ERROR_PIPE_BUSY) {
            if (pipe != INVALID_HANDLE_VALUE) {
                CloseHandle(pipe);
            }
            CloseHandle(m_IPSMutex);
            m_IPSMutex = INVALID_HANDLE_VALUE;
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
            xl_ds_api::CScanPathInfo* scanInfo = new xl_ds_api::CScanPathInfo();
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

VOID CDiskScan::ScanImgChange(std::vector<std::wstring> &paths)
{
    // 数据库写入分为两张表：目录监控表和文件监控表
    // 首先读入目录表，得到需要扫描的目录列表
    // 开始遍历目录列表中的每个目录
    // 读取文件表中该目录的所有监控状态
    // 遍历该目录下的全部文件，逐个将每个文件与读取的文件表做比对：
    // 1、根据路径匹配数据库记录
    // 2、如果没有匹配，计算文件CID再次匹配，如果匹配则认为已经上传
    // 2、如果匹配到路径，比较修改时间和文件大小，如果相同则认为已经上传
    // 3、如果不同，则比较CID，如果相同则认为已经上传
    // 4、如果不同，则认为没有上传
    // 处理比对结果：
    // 1、数据库有、扫描结果有的文件：
    // 2、数据库没有、扫描结果有的文件：加入上传队列
    // 3、数据库有，扫描结果没有的文件：标记已删除，如果首次标记已删除，则增加删除时间
    // 4、清理删除时间超过一定时间的记录

    // 数据库结构 cid, size, path, name, modify, state
    // state分为已上传、已删除
    // 遍历完全部目录将结果返回回调

    WriteMonitoringPath(paths);
    
    //ReadMonitoringPath(paths);
    
    //for (iter = paths.begin(); iter != paths.end(); iter++) {
    //    // 查询对应该目录的数据库文件备份记录
    //    // 遍历目录中的文件进行比对
    //    if (SetCurrentDirectory((*iter).c_str())) {
    //        WIN32_FIND_DATA findData;
    //        HANDLE handle = FindFirstFile(L"*", &findData);
    //        while (handle != INVALID_HANDLE_VALUE) {
    //            // 比对文件与数据库记录
    //            FindNextFile(handle, &findData);
    //        }
    //        FindClose(handle);
    //        handle = INVALID_HANDLE_VALUE;
    //    }
    //    // 更新数据库
    //}
}

VOID ReadMonitoringPath(std::vector<std::wstring> &paths) 
{
    sqlite3 *db;
    sqlite3_stmt *ppStmt;
    LPCWSTR pzTail = NULL;
    std::wstring dbPath = GetProcessPath();
    dbPath.append(L"\\scan_data.dat");

    WaitForSingleObject(m_MPMutex, INFINITE);
    if (sqlite3_open16(dbPath.c_str(), &db)) {
        goto ExitFree;
    }
    if ((sqlite3_prepare16_v2(
            db,
            L"SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='monitoring_path'", 
            -1, 
            &ppStmt, 
            (const void**)&pzTail) != SQLITE_OK) || (sqlite3_step(ppStmt) != SQLITE_ROW) || (sqlite3_column_int(ppStmt, 0) <= 0)) {
        goto ExitFree;
    }
    sqlite3_reset(ppStmt);
    if (sqlite3_prepare16_v2(
            db, 
            L"SELECT path FROM monitoring_path", 
            -1, 
            &ppStmt, 
            (const void**)&pzTail) != SQLITE_OK) {
        // 搜索监控目录失败
        goto ExitFree;
    }
    while(sqlite3_step(ppStmt) == SQLITE_ROW) {
        paths.push_back((LPCWSTR)sqlite3_column_text16(ppStmt, 0));
    }
ExitFree:
    sqlite3_finalize(ppStmt);
    sqlite3_close_v2(db);
    ReleaseMutex(m_MPMutex);
}

VOID WriteMonitoringPath(std::vector<std::wstring> paths)
{
    sqlite3 *db;
    sqlite3_stmt *ppStmt;
    LPCWSTR pzTail = NULL;
    std::wstring dbPath = GetProcessPath();
    dbPath.append(L"\\scan_data.dat");

    WaitForSingleObject(m_MPMutex, INFINITE);
    std::vector<std::wstring>::iterator iter;
    if (sqlite3_open16(dbPath.c_str(), &db)) {
        goto ExitFree;
    }
    if ((sqlite3_prepare16_v2(
        db,
        L"SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='monitoring_path'", 
        -1, 
        &ppStmt, 
        (const void**)&pzTail) != SQLITE_OK) || (sqlite3_step(ppStmt) != SQLITE_ROW) || (sqlite3_column_int(ppStmt, 0) <= 0)) {
            if (sqlite3_exec(
                db,
                "CREATE TABLE monitoring_path (path varchar(260) UNIQUE)",
                NULL,
                NULL,
                NULL) != SQLITE_OK) {
                goto ExitFree;
            }
    }
    for (iter = paths.begin(); iter != paths.end(); iter++) {
        CHAR buf[1024] = {0};
        sprintf(buf, "INSERT INTO monitoring_path VALUES ('%s')", UTF16ToUTF8((*iter).c_str()).c_str());
        sqlite3_exec(
            db,
            buf,
            NULL,
            NULL,
            NULL);
    }
ExitFree:
    sqlite3_finalize(ppStmt);
    sqlite3_close_v2(db);
    ReleaseMutex(m_IPSMutex);
}

VOID ReadMonitoringFiles(std::vector<std::wstring> paths, std::vector<xl_ds_api::CScanFileInfo> &scanFiles)
{

}

VOID WriteMonitoringFiles(std::vector<xl_ds_api::CScanFileInfo> scanFiles)
{

}

std::wstring GetProcessPath()
{
    TCHAR path[MAX_PATH];
    GetModuleFileName(NULL, path, MAX_PATH);
    std::wstring processPath = path;
    processPath = processPath.substr(0, processPath.rfind(L"\\"));
    return processPath;
}

std::string UTF16ToUTF8(const wchar_t* src)
{
    int cch2 = ::WideCharToMultiByte(CP_UTF8, 0, src, ::wcslen(src), 0, 0, 0, 0);
    char* str2  = new char[cch2 + 1];
    ::WideCharToMultiByte(CP_UTF8, 0, src, ::wcslen(src), str2, cch2 + 1, 0, 0);
    str2[cch2] = '\0';
    std::string destStr = str2;
    delete []str2;
    return destStr;
}

std::wstring UTF8ToUTF16(const char*src)
{
    int cch2 = ::MultiByteToWideChar(CP_UTF8, 0, src, ::strlen(src), NULL, 0);
    wchar_t* str2 = new wchar_t[cch2 + 1];
    ::MultiByteToWideChar(CP_UTF8, 0, src, ::strlen(src), str2, cch2);
    str2[cch2] = L'\0';
    std::wstring destStr = str2;
    delete []str2;
    return destStr;
}