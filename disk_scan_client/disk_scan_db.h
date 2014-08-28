#ifndef _DISK_SCAN_DB_
#define _DISK_SCAN_DB_

#include "sqlite3.h"

#define SQL_BUF 1024

namespace xl_ds_api
{
    class CDiskScanDB
    {
    public :
        CDiskScanDB();
        virtual ~CDiskScanDB();

    public :
        BOOL Open(const void* dbPath);
        BOOL CheckTable(const wchar_t* table);
        BOOL Exec(const char* sql);
        BOOL Prepare(const wchar_t* sql);
        BOOL Step();
        LPCSTR GetText(INT column);
        LPCWSTR GetText16(INT column);
        INT GetInt(INT column);
        DWORD GetInt64(INT column);
        BOOL Close();
	private :
		sqlite3* m_DB;
		sqlite3_stmt* m_Stmt;
    };
}

#endif