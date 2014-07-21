#ifndef _DISK_SCAN_TOOL_H_
#define _DISK_SCAN_TOOL_H_

#include <vector>
#include <string>

namespace xl_ds_api
{
    class CScanner
    {
    public :
        CScanner();
        virtual ~CScanner();

        void ScanDirectory();

    public :
        std::vector<std::wstring> m_RootPaths;
    };
}

#endif