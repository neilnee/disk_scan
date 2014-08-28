#include "stdafx.h"
#include "disk_scan_db.h"

using namespace xl_ds_api;

CDiskScanDB::CDiskScanDB():
m_DB(NULL),
m_Stmt(NULL)
{
}

CDiskScanDB::~CDiskScanDB()
{
    Close();
}

BOOL CDiskScanDB::Open(const void * dbPath)
{
    return  sqlite3_open16(dbPath, &m_DB) == SQLITE_OK;
}

BOOL CDiskScanDB::CheckTable(const wchar_t* table)
{
    LPCWSTR pzTail = NULL;
    TCHAR buf[SQL_BUF] = {0};
    wsprintf(buf, L"SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='%s'", table);
    if (m_Stmt) {
        sqlite3_reset(m_Stmt);
    }
    return m_DB && ((sqlite3_prepare16_v2(
        m_DB,
        buf,
        -1,
        &m_Stmt,
        (const void**)&pzTail) == SQLITE_OK) && (sqlite3_step(m_Stmt) == SQLITE_ROW) && (sqlite3_column_int(m_Stmt, 0) > 0));
}

BOOL CDiskScanDB::Exec(const char* sql)
{
	int ret = sqlite3_exec(
		m_DB,
		sql,
		NULL,
		NULL,
		NULL);
    return m_DB && ret == SQLITE_OK;
}

BOOL CDiskScanDB::Prepare(const wchar_t* sql)
{
    LPCWSTR pzTail = NULL;
    if (m_Stmt) {
        sqlite3_reset(m_Stmt);
    }
    return m_DB && sqlite3_prepare16_v2(
        m_DB, 
        sql, 
        -1, 
        &m_Stmt, 
        (const void**)&pzTail) == SQLITE_OK;
}

BOOL CDiskScanDB::Step()
{
    return m_Stmt && sqlite3_step(m_Stmt) == SQLITE_ROW;
}

LPCSTR CDiskScanDB::GetText(INT column)
{
    if (m_Stmt) {
        return (LPCSTR)sqlite3_column_text(m_Stmt, column);
    } else {
        return NULL;
    }
}

LPCWSTR CDiskScanDB::GetText16(INT column)
{
    if (m_Stmt) {
        return (LPCWSTR)sqlite3_column_text16(m_Stmt, column);
    } else {
        return NULL;
    }
}

INT CDiskScanDB::GetInt(INT column)
{
    if (m_Stmt) {
        return sqlite3_column_int(m_Stmt, column);
    } else {
        return NULL;
    }
}

DWORD CDiskScanDB::GetInt64(INT column)
{
    if (m_Stmt) {
        return (DWORD)sqlite3_column_int64(m_Stmt, column);
    } else {
        return NULL;
    }
}

BOOL CDiskScanDB::Close()
{
    if (m_Stmt) {
        sqlite3_finalize(m_Stmt);
        m_Stmt = NULL;
    }
    if (m_DB) {
        sqlite3_close(m_DB);
        m_DB = NULL;
    }
    return TRUE;
}



