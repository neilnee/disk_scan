#include "stdafx.h"
#include <windows.h>
#include <vector>
#include <map>
#include <algorithm>
#include <ShlObj.h>
#include "disk_scan.h"
#include "disk_scan_util.h"
#include "disk_scan_db.h"

#define PIPE_BUF_SIZE 1024
#define TIMEOUT 12000

using namespace xl_ds_api;

static const std::wstring IMG_SUFFIX[] = {
	L".jpg", L".png", L".jpeg", L".bmp", L".tif", L".tiff", L".raw"};

std::vector<DWORD> m_NotifyThreadIDs;
BOOL m_PicDirScanning = FALSE;
BOOL m_PicScanning = FALSE;
HANDLE m_IPSMutex = CreateMutex(NULL, FALSE, NULL);
HANDLE m_MPMutex = CreateMutex(NULL, FALSE, NULL);
HANDLE m_MFMutex =CreateMutex(NULL, FALSE, NULL);

VOID ReadMonitoringPath(std::vector<std::wstring> &paths);
VOID WriteMonitoringPath(std::vector<std::wstring> paths);
VOID ReadMonitoringFiles(std::vector<std::wstring> paths, std::map<std::wstring,xl_ds_api::CScanFileInfo > &scanFiles);
VOID WriteMonitoringFiles(std::vector<xl_ds_api::CScanFileInfo> scanFiles);
VOID DeleteMonitoringFiles(std::vector<std::wstring> fullPaths);

DWORD WINAPI PictureDirectoryScanExecute(LPVOID lpParam);
DWORD WINAPI PictureAutoScanExecute(LPVOID lpParam);
DWORD WINAPI PictureManualScanExecute(LPVOID lpParam);
DWORD WINAPI AddMonitoringDirectoryExecute(LPVOID lpParam);
DWORD WINAPI LoadMonitoringDirectoryExecute(LPVOID lpParam);

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
	//读取监控目录
    std::vector<std::wstring> paths;
	ReadMonitoringPath(paths);

	//读取数据库中的监控文件信息
	std::map<std::wstring, xl_ds_api::CScanFileInfo> files;
	ReadMonitoringFiles(paths, files);
    
	std::vector<std::wstring>::iterator iter;
    for (iter = paths.begin(); iter != paths.end(); iter++) {
		// 遍历监控目录，记录当前目录下需要更新的文件（新增和更新）
		
		std::vector<xl_ds_api::CScanFileInfo> updateFiles;
        if (SetCurrentDirectory((*iter).c_str())) {
            WIN32_FIND_DATA findData;
            HANDLE handle = FindFirstFile(L"*.*", &findData);
            if (handle != INVALID_HANDLE_VALUE) {
                BOOL finish = FALSE;
                do {
					std::wstring strFileName = findData.cFileName;
					if (strFileName == L"." || strFileName == L"..") {
						finish = !FindNextFile(handle, &findData);
						continue;
					}
					// 过滤非目标图片
					std::wstring::size_type suffixPos = strFileName.rfind(L".");
					std::wstring suffix = strFileName.substr(suffixPos);
					std::transform(suffix.begin(), suffix.end(), suffix.begin(), tolower);
					if (IMG_SUFFIX->find(suffix) == std::wstring::npos) {
						finish = !FindNextFile(handle, &findData);
						continue;
					}

					xl_ds_api::CScanFileInfo fileInfo;
					fileInfo.m_Path = (*iter);
					fileInfo.m_Name = findData.cFileName;
					fileInfo.m_FullPath = (*iter) + findData.cFileName;
					fileInfo.m_LastModifyHigh = findData.ftLastWriteTime.dwHighDateTime;
					fileInfo.m_LastModifyLow = findData.ftLastWriteTime.dwLowDateTime;
					fileInfo.m_FileSizeHigh = findData.nFileSizeHigh;
					fileInfo.m_FileSizeLow = findData.nFileSizeLow;
					fileInfo.m_State = 0;

					// 1、根据路径匹配数据库记录
					std::map<std::wstring, xl_ds_api::CScanFileInfo>::iterator foundIter = files.find(fileInfo.m_FullPath);
					if (foundIter == files.end()) {
						// 2、如果没有匹配，认为是新增文件（加入上传）
						updateFiles.push_back(fileInfo);
					} else {
						// 3、如果匹配到路径，比较修改时间、文件大小、CID，来确认是否是同一个文件
						xl_ds_api::CScanFileInfo foundInfo = (*foundIter).second;
						if (foundInfo.m_LastModifyHigh == fileInfo.m_LastModifyHigh 
							&& foundInfo.m_LastModifyLow == fileInfo.m_LastModifyLow
							&& foundInfo.m_FileSizeHigh == fileInfo.m_FileSizeHigh
							&& foundInfo.m_FileSizeLow == fileInfo.m_FileSizeLow
							&& foundInfo.m_CID == (fileInfo.m_CID = CIDCalculate(fileInfo.m_FullPath))) {
								// 4、如果是同一个文件，若之前删除了这个文件，则要重新上传
								if (foundInfo.m_State > 0) {
									updateFiles.push_back(fileInfo);
								}
						} else {
							// 5、如果不是同一个文件，则需要上传
							updateFiles.push_back(fileInfo);
						}
						files.erase(foundIter);
					}
                    finish = !FindNextFile(handle, &findData);
                } while (!finish);
            }
            FindClose(handle);
        }
		if (updateFiles.size() > 0 && CreateDownloadTask(updateFiles)) {
			WriteMonitoringFiles(updateFiles);
		}
    }
	// 遍历files中剩余的没有被匹配过的数据，根据删除时间更新数据库
	// 删除时间<=0则表示本次扫描时发现被删除，更新删除时间为当前时间
	// 删除时间>0则比较是否符合删除条件（超过一定时间显示）
	// 符合则从数据库中删除，不符合则不更新数据库
}

VOID CDiskScan::StartPictureManualScan(std::vector<std::wstring> paths)
{
	// 启动文件的手动扫描和上传任务PictureUploadExecute线程
	// 过滤掉已经监控的目录
	// 遍历目标目录全部图片文件信息，创建任务
	// 创建任务成功则将目标目录添入监控
	// 依次计算文件CID，添加到任务信息中
	// 更新数据库
}

VOID CDiskScan::AddMonitoringDirectory(std::vector<std::wstring> paths)
{
	WriteMonitoringPath(paths);
}

VOID CDiskScan::LoadMonitoringDirectory()
{

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
			PostThreadMessage(*iter, DSMSG_DIR_SCAN, reinterpret_cast<WPARAM>(scanInfo), 0);
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
	m_PicScanning = TRUE;

	m_PicScanning = FALSE;
	return 0;
}

DWORD WINAPI PictureManualScanExecute(LPVOID lpParam)
{
	m_PicScanning = TRUE;

	m_PicScanning = FALSE;
	return 0;
}

DWORD WINAPI AddMonitoringDirectoryExecute(LPVOID lpParam)
{
	return 0;
}

DWORD WINAPI LoadMonitoringDirectoryExecute(LPVOID lpParam)
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
	db.Exec("BEGIN TRANSACTION");
    for (iter = paths.begin(); iter != paths.end(); iter++) {
        CHAR sql[SQL_BUF] = {0};
        sprintf(sql, "INSERT INTO monitoring_path VALUES ('%s')", UTF16ToUTF8((*iter).c_str()).c_str());
        db.Exec(sql);
    }
	db.Exec("COMMIT TRANSACTION");
ExitFree:
    db.Close();
    ReleaseMutex(m_IPSMutex);
}

VOID ReadMonitoringFiles(std::vector<std::wstring> paths, std::map<std::wstring,xl_ds_api::CScanFileInfo > &files)
{
    xl_ds_api::CDiskScanDB db;
    std::wstring dbPath = GetProcessPath();
    dbPath.append(L"\\scan_file.dat");
    std::wstring sql;

    WaitForSingleObject(m_MPMutex, INFINITE);
    if (!db.Open(dbPath.c_str())) {
        goto ExitFree;
    }
    if (!db.CheckTable(L"monitoring_file")) {
        goto ExitFree;
    }
    sql.append(L"SELECT * FROM monitoring_file");
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
		files.insert(std::map<std::wstring, xl_ds_api::CScanFileInfo>::value_type(fileInfo.m_FullPath, fileInfo));
    }
ExitFree:
    db.Close();
    ReleaseMutex(m_MPMutex);
}

VOID WriteMonitoringFiles(std::vector<xl_ds_api::CScanFileInfo> files)
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
	db.Exec("BEGIN TRANSACTION");
    for (iter = files.begin(); iter != files.end(); iter++) {
        CHAR sql[1024] = {0};
        sprintf(sql, "INSERT OR REPLACE INTO monitoring_file VALUES ('%s','%s','%s','%s','%d','%d','%d','%d','%d')",
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
	db.Exec("COMMIT TRANSACTION");
ExitFree:
    db.Close();
    ReleaseMutex(m_MFMutex);
}

VOID DeleteMonitoringFiles(std::vector<std::wstring> fullPaths)
{

}