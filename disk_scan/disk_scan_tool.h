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

		/**
		 * 扫描出本地各磁盘的一级目录（根据规则过滤掉不在扫描范围内的目录）
		 *
		 * @param baseDirs : out, 扫描到的一级目录列表
		 */
		void ScanBaseDir(std::vector<std::wstring> &baseDirs);

		/**
		 * 根据规则扫描出主要的图片存储目录
		 *
		 * @param baseDirs : in, 扫描的目标目录列表
		 * @param picDir : out, 扫描到的图片存储目录列表
		 */
		void ScanPicDir(std::vector<std::wstring>* baseDir, std::vector<std::wstring> &picDir);

    public :

    };
}

#endif