#include "stdafx.h"
#include <windows.h>
#include <vector>
#include <ShlObj.h>
#include "disk_scan.h"
#include "sqlite3.h"
#include "disk_scan_util.h"
#include "disk_scan_db.h"

#define PIPE_BUF_SIZE 1024
#define TIMEOUT 12000

using namespace xl_ds_api;

std::vector<DWORD> m_NotifyThreadIDs;
BOOL m_PicDirScanning = FALSE;
BOOL m_PicAutoScanning = FALSE;
HANDLE m_IPSMutex = CreateMutex(NULL, FALSE, NULL);
HANDLE m_MPMutex = CreateMutex(NULL, FALSE, NULL);
HANDLE m_MFMutex =CreateMutex(NULL, FALSE, NULL);

VOID ReadMonitoringPath(std::vector<std::wstring> &paths);
VOID WriteMonitoringPath(std::vector<std::wstring> paths);
VOID ReadMonitoringFiles(std::vector<std::wstring> paths, std::vector<xl_ds_api::CScanFileInfo> &scanFiles);
VOID WriteMonitoringFiles(std::vector<xl_ds_api::CScanFileInfo> scanFiles);

DWORD WINAPI PictureDirectoryScanExecute(LPVOID lpParam);
DWORD WINAPI PictureAutoScanExecute(LPVOID lpParam);
DWORD WINAPI PictureManualScanExecute(LPVOID lpParam);
DWORD WINAPI AddMonitoringDirectoryExecute(LPVOID lpParam);

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

CDiskScan::CDiskScan()
{
}

CDiskScan::~CDiskScan()
{
    m_NotifyThreadIDs.clear();
	CloseHandle(m_IPSMutex);
	CloseHandle(m_MPMutex);
	CloseHandle(m_MFMutex);
}

VOID CDiskScan::StartPictureDirectoryScan(DWORD threadID, LPTSTR requestCode)
{
	xl_ds_api::CScanRequest* request = new xl_ds_api::CScanRequest();
	request->m_ThreadID = threadID;
	request->m_RequestCode = requestCode;
    
    CreateThread(
        NULL,
        0,
        PictureDirectoryScanExecute,
        (LPVOID) request,
        0,
        &threadID);
}

VOID CDiskScan::StartPictrueAutoScan()
{
	// 启动文件的自动扫描和上传任务PictureAutoScanExecute线程
    std::vector<std::wstring> paths;
    std::vector<std::wstring>::iterator iter;
    std::vector<xl_ds_api::CScanFileInfo> files;
    ReadMonitoringPath(paths);
    for (iter = paths.begin(); iter != paths.end(); iter++) {
        if (SetCurrentDirectory((*iter).c_str())) {
            WIN32_FIND_DATA findData;
            HANDLE handle = FindFirstFile(L"*.jpg", &findData);
            if (handle != INVALID_HANDLE_VALUE) {
                BOOL finish = FALSE;
                do {
                    xl_ds_api::CScanFileInfo fileInfo;
                    fileInfo.m_Path = (*iter);
                    fileInfo.m_Name = findData.cFileName;
                    fileInfo.m_FullPath = (*iter) + findData.cFileName;
                    fileInfo.m_LastModifyHigh = findData.ftLastWriteTime.dwHighDateTime;
                    fileInfo.m_LastModifyLow = findData.ftLastWriteTime.dwLowDateTime;
                    fileInfo.m_FileSizeHigh = findData.nFileSizeHigh;
                    fileInfo.m_FileSizeLow = findData.nFileSizeLow;
                    files.push_back(fileInfo);
                    finish = !FindNextFile(handle, &findData);
                } while (!finish);
            }
            FindClose(handle);
        }
    }
    WriteMonitoringFiles(files);
    std::vector<xl_ds_api::CScanFileInfo> fileResult;
    ReadMonitoringFiles(paths, fileResult);
}

VOID CDiskScan::StartPictureManualScan(std::vector<std::wstring> paths)
{
	// 启动文件的手动扫描和上传任务PictureUploadExecute线程
}

VOID CDiskScan::AddMonitoringDirectory(std::vector<std::wstring> paths)
{
	// 启动添加监控目录AddMonitoringDirectoryExecute线程
	WriteMonitoringPath(paths);
}

DWORD WINAPI PictureDirectoryScanExecute(LPVOID lpParam)
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
	if(m_PicDirScanning) {
		success = TRUE;
	} else {
		m_PicDirScanning = TRUE;
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
		if (eventCode == SCAN_FINISH || eventCode == SCAN_STOP) {
			m_NotifyThreadIDs.clear();
			break;
		}
		ReleaseMutex(m_IPSMutex);
	} while (success);

	CloseHandle(pipe);
	pipe = INVALID_HANDLE_VALUE;
	return 0;
}

DWORD WINAPI PictureAutoScanExecute(LPVOID lpParam)
{
	return 0;
}

DWORD WINAPI PictureManualScanExecute(LPVOID lpParam)
{
	// 遍历目录创建任务
	// 添加目录进监控目录
	// 计算文件信息，更新数据库
	return 0;
}

DWORD WINAPI AddMonitoringDirectoryExecute(LPVOID lpParam)
{
	return 0;
}

VOID ReadMonitoringPath(std::vector<std::wstring> &paths) 
{
    xl_ds_api::CDiskScanDB db;
    std::wstring dbPath = GetProcessPath();
    dbPath.append(L"\\scan_path.dat");

    WaitForSingleObject(m_MPMutex, INFINITE);
    if (!db.Open(dbPath.c_str())) {
        goto ExitFree;
    }
    if (!db.CheckTable(L"monitoring_path")) {
        goto ExitFree;
    }
    if (!db.Prepare(L"SELECT path FROM monitoring_path")) {
        goto ExitFree;
    }
    while(db.Step()) {
        if (db.GetText16(0)) {
            paths.push_back(db.GetText16(0));
        }
    }
ExitFree:
    db.Close();
    ReleaseMutex(m_MPMutex);
}

VOID WriteMonitoringPath(std::vector<std::wstring> paths)
{
    xl_ds_api::CDiskScanDB db;
    std::wstring dbPath = GetProcessPath();
    dbPath.append(L"\\scan_path.dat");
    std::vector<std::wstring>::iterator iter;

    WaitForSingleObject(m_MPMutex, INFINITE);
    if (!db.Open(dbPath.c_str())) {
        goto ExitFree;
    }
    if (!db.CheckTable(L"monitoring_path")) {
            if (!db.Exec("CREATE TABLE monitoring_path (path varchar(260) UNIQUE)")) {
                goto ExitFree;
            }
    }
    for (iter = paths.begin(); iter != paths.end(); iter++) {
        CHAR sql[SQL_BUF] = {0};
        sprintf(sql, "INSERT INTO monitoring_path VALUES ('%s')", UTF16ToUTF8((*iter).c_str()).c_str());
        db.Exec(sql);
    }
ExitFree:
    db.Close();
    ReleaseMutex(m_IPSMutex);
}

VOID ReadMonitoringFiles(std::vector<std::wstring> paths, std::vector<xl_ds_api::CScanFileInfo> &scanFiles)
{
    xl_ds_api::CDiskScanDB db;
    std::wstring dbPath = GetProcessPath();
    dbPath.append(L"\\scan_file.dat");
    std::wstring sql;
    std::vector<std::wstring>::iterator iter;

    WaitForSingleObject(m_MPMutex, INFINITE);
    if (!db.Open(dbPath.c_str())) {
        goto ExitFree;
    }
    if (!db.CheckTable(L"monitoring_file")) {
        goto ExitFree;
    }
    sql.append(L"SELECT * FROM monitoring_file");
    /*if (paths.size() > 0) {
        sql.append(L"WHERE path IN(");
        for (iter = paths.begin(); iter != paths.end(); iter++) {
            sql.append(L"'");
            sql.append(*iter);
            sql.append(L"'");
        }
    }*/
    if (!db.Prepare(sql.c_str())) {
        goto ExitFree;
    }
    while(db.Step()) {
        xl_ds_api::CScanFileInfo fileInfo;
        fileInfo.m_FullPath = db.GetText16(0);
        fileInfo.m_Path = db.GetText16(1);
        fileInfo.m_Name = db.GetText16(2);
        fileInfo.m_CID = db.GetText(3);
        fileInfo.m_State = db.GetInt(4);
        fileInfo.m_LastModifyHigh = db.GetInt64(5);
        fileInfo.m_LastModifyLow = db.GetInt64(6);
        fileInfo.m_FileSizeHigh = db.GetInt64(7);
        fileInfo.m_FileSizeLow = db.GetInt64(8);
        scanFiles.push_back(fileInfo);
    }
ExitFree:
    db.Close();
    ReleaseMutex(m_MPMutex);
}

VOID WriteMonitoringFiles(std::vector<xl_ds_api::CScanFileInfo> scanFiles)
{
    xl_ds_api::CDiskScanDB db;
    std::wstring dbPath = GetProcessPath();
    dbPath.append(L"\\scan_file.dat");
    std::vector<xl_ds_api::CScanFileInfo>::iterator iter;

    WaitForSingleObject(m_MFMutex, INFINITE);
    if (!db.Open(dbPath.c_str())) {
        goto ExitFree;
    }
    if (!db.CheckTable(L"monitoring_file")) {
        if (!db.Exec("CREATE TABLE monitoring_file (fullpath VARCHAR(255) PRIMARY KEY, path VARCHAR(255), name VARCHAR(255), cid VARCHAR(255), state INT, modify_h UNSIGNED BIG INT, modify_l UNSIGNED BIG INT, filesize_h UNSIGNED BIG INT, filesize_l UNSIGNED BIG INT)")) {
            goto ExitFree;
        }
    }
    for (iter = scanFiles.begin(); iter != scanFiles.end(); iter++) {
        CHAR sql[1024] = {0};
        sprintf(sql, "INSERT INTO monitoring_file VALUES ('%s','%s','%s','%s','%d','%d','%d','%d','%d')",
            UTF16ToUTF8((*iter).m_FullPath.c_str()).c_str(),
            UTF16ToUTF8((*iter).m_Path.c_str()).c_str(),
            UTF16ToUTF8((*iter).m_Name.c_str()).c_str(),
            (*iter).m_CID.c_str(),
            (*iter).m_State,
            (*iter).m_LastModifyHigh,
            (*iter).m_LastModifyLow,
            (*iter).m_FileSizeHigh,
            (*iter).m_FileSizeLow);
        db.Exec(sql);
    }
ExitFree:
    db.Close();
    ReleaseMutex(m_MFMutex);
}