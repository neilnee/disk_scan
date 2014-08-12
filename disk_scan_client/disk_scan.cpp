#include "stdafx.h"
#include <windows.h>
#include <vector>
#include <map>
#include <algorithm>
#include <ShlObj.h>
#include <time.h>
#include "disk_scan.h"
#include "disk_scan_util.h"
#include "disk_scan_db.h"

#define PIPE_BUF_SIZE 1024
#define TIMEOUT 12000
#define DELETE_INTERVAL 86440

#define SQL_EXEC_DELETE 1010

using namespace xl_ds_api;

static const std::wstring IMG_SUFFIX[] = {
	L".jpg", L".png", L".jpeg", L".bmp", L".gif", L".psd", L".hdr", L".pic", L".tga"};

BOOL m_PicDirScanning = FALSE;
BOOL m_PicScanning = FALSE;
BOOL m_PicManualScanning = FALSE;
HANDLE m_IPSMutex = CreateMutex(NULL, FALSE, NULL);
HANDLE m_MPMutex = CreateMutex(NULL, FALSE, NULL);
HANDLE m_MFMutex =CreateMutex(NULL, FALSE, NULL);
DiskScanResultNotify m_ResultNotifyCallback;

VOID ReadMonitoringPath(std::vector<std::wstring> &paths);
BOOL WriteMonitoringPath(std::vector<std::wstring> paths);
VOID ReadMonitoringFiles(std::vector<std::wstring> paths, std::map<std::wstring,xl_ds_api::CScanFileInfo> &scanFiles);
VOID WriteMonitoringFiles(std::vector<xl_ds_api::CScanFileInfo> scanFiles);
VOID NotifyDiskScanResult(xl_ds_api::CScanResultEvent* resultEvent);
BOOL FilterTargetFile(std::wstring path, WIN32_FIND_DATA findData, xl_ds_api::CScanFileInfo &fileInfo);

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
    m_ResultNotifyCallback = NULL;
	CloseHandle(m_IPSMutex);
	CloseHandle(m_MPMutex);
	CloseHandle(m_MFMutex);
}

VOID CDiskScan::StartPictureDirectoryScan(LPTSTR requestCode)
{
    if (!m_PicDirScanning) {
        xl_ds_api::CScanRequest* request = new xl_ds_api::CScanRequest();
        request->m_RequestCode = requestCode;
        DWORD threadID;
        CreateThread(
            NULL,
            0,
            PictureDirectoryScanExecute,
            (LPVOID) request,
            0,
            &threadID);
    }
}

VOID CDiskScan::StartPictrueAutoScan()
{
	if (!m_PicScanning && !m_PicManualScanning) {
        DWORD threadID;
		CreateThread(
			NULL,
			0,
			PictureAutoScanExecute,
			NULL,
			0,
			&threadID);
	}
}

VOID CDiskScan::StartPictureManualScan(std::vector<std::wstring> paths)
{
    if (!m_PicManualScanning) {
        xl_ds_api::CScanRequest* request = new xl_ds_api::CScanRequest();
        request->m_Paths = paths;
        DWORD threadID;
        CreateThread(
            NULL,
            0,
            PictureManualScanExecute,
            (LPVOID) request,
            0,
            &threadID);
    }
}

VOID CDiskScan::AddMonitoringDirectory(std::vector<std::wstring> paths)
{
    xl_ds_api::CScanRequest* request = new xl_ds_api::CScanRequest();
    request->m_Paths = paths;
    DWORD threadID;
    CreateThread(
        NULL,
        0,
        AddMonitoringDirectoryExecute,
        (LPVOID) request,
        0,
        &threadID);
}

VOID CDiskScan::LoadMonitoringDirectory()
{
    DWORD threadID;
    CreateThread(
        NULL,
        0,
        LoadMonitoringDirectoryExecute,
        NULL,
        0,
        &threadID);
}

VOID CDiskScan::SetResultNotifyCallback(DiskScanResultNotify resultNotify)
{
    WaitForSingleObject(m_IPSMutex, INFINITE);
    m_ResultNotifyCallback = resultNotify;
    ReleaseMutex(m_IPSMutex);
}

DWORD WINAPI PictureDirectoryScanExecute(LPVOID lpParam)
{
    m_PicDirScanning = TRUE;
	BOOL success = FALSE;
	LPTSTR pipeName = TEXT("\\\\.\\pipe\\xlspace_disk_scan_pipe");
	xl_ds_api::CScanRequest* requestPtr = (xl_ds_api::CScanRequest*) lpParam;
	if (requestPtr == NULL) {
		return -1;
	}
	xl_ds_api::CScanRequest request = *requestPtr;
	delete requestPtr;

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
	for (;;) {
        if (!ReadFile(
            pipe,
            buf,
            PIPE_BUF_SIZE*sizeof(TCHAR),
            &dRead,
            NULL)) {
            break;
        }
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

        xl_ds_api::CScanResultEvent* resultEvent = new xl_ds_api::CScanResultEvent();
        resultEvent->m_Msg = DSMSG_DIR_SCAN;
        resultEvent->m_EventCode = eventCode;
        resultEvent->m_ScanCount = scanCount;
        resultEvent->m_TotalCount = totalCount;
        resultEvent->m_Paths.push_back(path);
        NotifyDiskScanResult(resultEvent);
	}

	CloseHandle(pipe);
	pipe = INVALID_HANDLE_VALUE;
	return 0;
}

DWORD WINAPI PictureAutoScanExecute(LPVOID lpParam)
{
	m_PicScanning = TRUE;
	time_t curTime = time(NULL);
	//��ȡ���Ŀ¼
	std::vector<std::wstring> paths;
	ReadMonitoringPath(paths);
	//��ȡ���ݿ��еļ���ļ���Ϣ
	std::map<std::wstring, xl_ds_api::CScanFileInfo> files;
	ReadMonitoringFiles(paths, files);
	// �������Ŀ¼����¼��ǰĿ¼����Ҫ���µ��ļ��������͸��£�
	std::vector<std::wstring>::iterator iter;
	for (iter = paths.begin(); iter != paths.end(); iter++) {
		std::vector<xl_ds_api::CScanFileInfo> updateFiles;
		if (SetCurrentDirectory((*iter).c_str())) {
			WIN32_FIND_DATA findData;
			HANDLE handle = FindFirstFile(L"*.*", &findData);
			if (handle != INVALID_HANDLE_VALUE) {
				BOOL finish = FALSE;
				do {
                    xl_ds_api::CScanFileInfo fileInfo;
                    // ���˷�Ŀ���ļ�
                    if (!FilterTargetFile(*iter, findData, fileInfo)) {
                        finish = !FindNextFile(handle, &findData);
                        continue;
                    }
					// 1������·��ƥ�����ݿ��¼
					std::map<std::wstring, xl_ds_api::CScanFileInfo>::iterator foundIter = files.find(fileInfo.m_FullPath);
					if (foundIter == files.end()) {
						// 2�����û��ƥ�䣬��Ϊ�������ļ��������ϴ���
						updateFiles.push_back(fileInfo);
					} else {
						// 3�����ƥ�䵽·�����Ƚ��޸�ʱ�䡢�ļ���С��CID����ȷ���Ƿ���ͬһ���ļ�
						xl_ds_api::CScanFileInfo foundInfo = (*foundIter).second;
						if (foundInfo.m_LastModifyHigh == fileInfo.m_LastModifyHigh 
							&& foundInfo.m_LastModifyLow == fileInfo.m_LastModifyLow
							&& foundInfo.m_FileSizeHigh == fileInfo.m_FileSizeHigh
							&& foundInfo.m_FileSizeLow == fileInfo.m_FileSizeLow
							&& foundInfo.m_CID == (fileInfo.m_CID = CIDCalculate(fileInfo.m_FullPath))) {
								// 4�������ͬһ���ļ�����֮ǰɾ��������ļ�����Ҫ�����ϴ�
								if (foundInfo.m_State > 0) {
									updateFiles.push_back(fileInfo);
								}
						} else {
							// 5���������ͬһ���ļ�������Ҫ�ϴ�
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
	// ����files��ʣ���û�б�ƥ��������ݣ�����ɾ��ʱ��������ݿ�
	std::map<std::wstring, xl_ds_api::CScanFileInfo>::iterator deleteIter;
	std::vector<xl_ds_api::CScanFileInfo> remainFiles;
	for (deleteIter = files.begin(); deleteIter != files.end(); deleteIter++) {
		xl_ds_api::CScanFileInfo foundInfo = (*deleteIter).second;
		if (foundInfo.m_State > 0) {
			// ɾ��ʱ��>0��Ƚ��Ƿ����ɾ������������һ��ʱ�����ƣ�
			// ����������ݿ���ɾ�����������򲻸������ݿ�
			if ((curTime - foundInfo.m_State) > DELETE_INTERVAL) {
				foundInfo.m_SqlExec = SQL_EXEC_DELETE;
				remainFiles.push_back(foundInfo);
			}
		} else {
			// ɾ��ʱ��<=0���ʾ����ɨ��ʱ���ֱ�ɾ��������ɾ��ʱ��Ϊ��ǰʱ��
			foundInfo.m_State = curTime;
			remainFiles.push_back(foundInfo);
		}
	}
	if (remainFiles.size() > 0) {
		WriteMonitoringFiles(remainFiles);
	}
	m_PicScanning = FALSE;
	return 0;
}

DWORD WINAPI PictureManualScanExecute(LPVOID lpParam)
{
	m_PicManualScanning = TRUE;
    xl_ds_api::CScanRequest* requestPtr = (xl_ds_api::CScanRequest*) lpParam;
    if (requestPtr == NULL) {
        return -1;
    }
    xl_ds_api::CScanRequest request = *requestPtr;
    delete requestPtr;

    // ��ȡ�Ѿ���ص�Ŀ¼
    std::vector<std::wstring> monitoringPaths;
    ReadMonitoringPath(monitoringPaths);
    // ���˵��Ѿ���ص�Ŀ¼
    std::vector<std::wstring>::iterator iter = request.m_Paths.begin();
    while(iter != request.m_Paths.end()) {
        std::vector<std::wstring>::iterator foundIter = std::find(monitoringPaths.begin(), monitoringPaths.end(), *iter);
        if (foundIter != monitoringPaths.end()) {
            iter = request.m_Paths.erase(iter);
        } else {
            iter++;
        }
    }
    // ����Ŀ��Ŀ¼ȫ��ͼƬ�ļ���Ϣ
    std::vector<xl_ds_api::CScanFileInfo> uploadFiles;
    for (iter = request.m_Paths.begin(); iter != request.m_Paths.end(); iter++) {
        if (SetCurrentDirectory((*iter).c_str())) {
            WIN32_FIND_DATA findData;
            HANDLE handle = FindFirstFile(L"*.*", &findData);
            if (handle != INVALID_HANDLE_VALUE) {
                BOOL finish = FALSE;
                do {
                    xl_ds_api::CScanFileInfo fileInfo;
                    // ���˷�Ŀ���ļ�
                    if (FilterTargetFile(*iter, findData, fileInfo)) {
                        uploadFiles.push_back(fileInfo);
                    }
                    finish = !FindNextFile(handle, &findData);
                } while (!finish);
            }
        }
    }
    // �������񣬴�������ɹ���Ŀ��Ŀ¼�����أ��ص�֪ͨ
    BOOL createTaskRet = FALSE;
    xl_ds_api::CScanResultEvent* resultEvent = new xl_ds_api::CScanResultEvent();
    resultEvent->m_Msg = DSMSG_PIC_SCAN;
    if (uploadFiles.size() > 0 && CreateDownloadTask(uploadFiles)) {
        WriteMonitoringPath(request.m_Paths);
        createTaskRet = TRUE;
        resultEvent->m_EventCode = SCAN_IMG_MANUAL_SUCCESS;
    } else {
        resultEvent->m_EventCode = SCAN_IMG_MANUAL_FAILED;
    }
    NotifyDiskScanResult(resultEvent);
    if (createTaskRet) {
        // ���μ����ļ�CID����ӵ�������Ϣ��
        std::vector<xl_ds_api::CScanFileInfo>::iterator fileIter;
        for (fileIter = uploadFiles.begin(); fileIter != uploadFiles.end(); fileIter++) {
            (*fileIter).m_CID = CIDCalculate((*fileIter).m_FullPath);
            SetDownloadInfo(*fileIter);
        }
        // �������ݿ�
        WriteMonitoringFiles(uploadFiles);
    }
	m_PicManualScanning = FALSE;
	return 0;
}

DWORD WINAPI AddMonitoringDirectoryExecute(LPVOID lpParam)
{
    xl_ds_api::CScanRequest* requestPtr = (xl_ds_api::CScanRequest*) lpParam;
    if (requestPtr == NULL) {
        return -1;
    }
    xl_ds_api::CScanRequest request = *requestPtr;
    BOOL optRet = FALSE;
    delete requestPtr;

    if (request.m_Paths.size() > 0) {
        optRet = WriteMonitoringPath(request.m_Paths);
    }
    xl_ds_api::CScanResultEvent* resultEvent = new xl_ds_api::CScanResultEvent();
    resultEvent->m_Msg = DSMSG_ADD_DIR;
    if (optRet) {
        resultEvent->m_EventCode = ADD_DIR_SUCCESS;
    } else {
        resultEvent->m_EventCode = ADD_DIR_FAILED;
    }
    NotifyDiskScanResult(resultEvent);
	return 0;
}

DWORD WINAPI LoadMonitoringDirectoryExecute(LPVOID lpParam)
{
    std::vector<std::wstring> paths;
    ReadMonitoringPath(paths);
    xl_ds_api::CScanResultEvent* resultEvent = new xl_ds_api::CScanResultEvent();
    resultEvent->m_Msg = DSMSG_LOAD_DIR;
    resultEvent->m_EventCode = LOAD_DIR_DONE;
    resultEvent->m_Paths = paths;
    NotifyDiskScanResult(resultEvent);
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

BOOL WriteMonitoringPath(std::vector<std::wstring> paths)
{
    BOOL result = FALSE;
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
	result = db.Exec("COMMIT TRANSACTION");
ExitFree:
    db.Close();
    ReleaseMutex(m_IPSMutex);
    return result;
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
		if ((*iter).m_SqlExec == SQL_EXEC_DELETE) {
			sprintf(sql, "DELETE FROM monitoring_file WHERE fullPath = '%s'", UTF16ToUTF8((*iter).m_FullPath.c_str()).c_str());
		} else {
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
		}
        db.Exec(sql);
    }
	db.Exec("COMMIT TRANSACTION");
ExitFree:
    db.Close();
    ReleaseMutex(m_MFMutex);
}

VOID NotifyDiskScanResult(xl_ds_api::CScanResultEvent* resultEvent)
{
    WaitForSingleObject(m_IPSMutex, INFINITE);
    if (m_ResultNotifyCallback != NULL) {
        m_ResultNotifyCallback(resultEvent);
    }
    ReleaseMutex(m_IPSMutex);
}

BOOL FilterTargetFile(std::wstring path, WIN32_FIND_DATA findData, xl_ds_api::CScanFileInfo &fileInfo)
{
    // ����Ŀ¼
    std::wstring strFileName = findData.cFileName;
    if (strFileName == L"." || strFileName == L".." || (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        return FALSE;
    }
    // ���˷�Ŀ��ͼƬ
    std::wstring::size_type suffixPos = strFileName.rfind(L".");
    std::wstring suffix = strFileName.substr(suffixPos);
    std::transform(suffix.begin(), suffix.end(), suffix.begin(), tolower);
    if (IMG_SUFFIX->find(suffix) == std::wstring::npos) {
        return FALSE;
    }
    fileInfo.m_Path = path;
    fileInfo.m_Name = findData.cFileName;
    fileInfo.m_FullPath = path + findData.cFileName;
    fileInfo.m_LastModifyHigh = findData.ftLastWriteTime.dwHighDateTime;
    fileInfo.m_LastModifyLow = findData.ftLastWriteTime.dwLowDateTime;
    fileInfo.m_FileSizeHigh = findData.nFileSizeHigh;
    fileInfo.m_FileSizeLow = findData.nFileSizeLow;
    fileInfo.m_State = 0;
    return TRUE;
}