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

// ���ݿ�д���Ϊ���ű�Ŀ¼��ر���ļ���ر�
// ���ȶ���Ŀ¼���õ���Ҫɨ���Ŀ¼�б�
// ��ʼ����Ŀ¼�б��е�ÿ��Ŀ¼
// ��ȡ�ļ����и�Ŀ¼�����м��״̬
// ������Ŀ¼�µ�ȫ���ļ��������ÿ���ļ����ȡ���ļ������ȶԣ�
// 1������·��ƥ�����ݿ��¼
// 2�����û��ƥ�䣬�����ļ�CID�ٴ�ƥ�䣬���ƥ������Ϊ�Ѿ��ϴ�
// 2�����ƥ�䵽·�����Ƚ��޸�ʱ����ļ���С�������ͬ����Ϊ�Ѿ��ϴ�
// 3�������ͬ����Ƚ�CID�������ͬ����Ϊ�Ѿ��ϴ�
// 4�������ͬ������Ϊû���ϴ�
// ����ȶԽ����
// 1�����ݿ��С�ɨ�����е��ļ���
// 2�����ݿ�û�С�ɨ�����е��ļ��������ϴ�����
// 3�����ݿ��У�ɨ����û�е��ļ��������ɾ��������״α����ɾ����������ɾ��ʱ��
// 4������ɾ��ʱ�䳬��һ��ʱ��ļ�¼

// ���ݿ�ṹ cid, size, path, name, modify, state
// state��Ϊ���ϴ�����ɾ��
// ������ȫ��Ŀ¼��������ػص�

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
	// �����ļ����Զ�ɨ����ϴ�����PictureAutoScanExecute�߳�
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
	// �����ļ����ֶ�ɨ����ϴ�����PictureUploadExecute�߳�
}

VOID CDiskScan::AddMonitoringDirectory(std::vector<std::wstring> paths)
{
	// ������Ӽ��Ŀ¼AddMonitoringDirectoryExecute�߳�
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
			// ���ӳɹ�
			break;
		}
		if (GetLastError() == ERROR_FILE_NOT_FOUND) {
			// û��ɨ���������������ɨ�����
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
			// ���ӳ�ʱ
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
	// ����Ŀ¼��������
	// ���Ŀ¼�����Ŀ¼
	// �����ļ���Ϣ���������ݿ�
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