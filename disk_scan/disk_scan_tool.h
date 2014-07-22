#ifndef _DISK_SCAN_TOOL_H_
#define _DISK_SCAN_TOOL_H_

#include <windows.h>
#include <vector>
#include <string>
#include <ShlObj.h>

#define PRIORITY_DIRS {CSIDL_DESKTOP, CSIDL_MYDOCUMENTS, CSIDL_MYPICTURES, CSIDL_COMMON_PICTURES, CSIDL_COMMON_DOCUMENTS, CSIDL_COMMON_DESKTOPDIRECTORY}
#define IGNORE_DIRS {CSIDL_WINDOWS, CSIDL_PROGRAM_FILES, CSIDL_TEMPLATES, CSIDL_PROGRAM_FILESX86, CSIDL_APPDATA, CSIDL_COMMON_APPDATA, CSIDL_COOKIES, CSIDL_INTERNET_CACHE, CSIDL_LOCAL_APPDATA}

static const DWORD BUF_SIZE  = 512;
static const std::wstring IMG_SUFFIX[] = {
    L".jpg", L".png", L".jpeg", L".bmp", L".tif", L".tiff"};

namespace xl_ds_api
{
    typedef void (*ScanTargetCallback) (INT scan, std::wstring current);

    class CScanner
    {
    public :
        CScanner();
        virtual ~CScanner();

		/**
		 * 添加回调接口
		 */
		void SetScanTargetCallback(ScanTargetCallback callback);

		/**
		 * 根据规则扫描出主要的目标类型存储目录
		 *
		 * @param baseDirs : in, 扫描的目标目录列表
		 * @param targetDir : out, 扫描到的目标类型存储目录列表
		 * @param priority : in, 是否是扫描优先目录
		 */
		void ScanTargetDir(std::vector<std::wstring>* baseDir, std::vector<std::wstring> &targetDir, BOOL priority);

	private :
		/**
		 * 初始化
		 */
		void Init();

		/**
		 * 反初始化
		 */
		void UnInit();

		void PushBackDir(std::vector<std::wstring> &dirList, std::wstring &directory);

		/**
		 * 扫描出本地各磁盘的一级目录（根据规则过滤掉不在扫描范围内的目录）
		 *
		 * @param baseDirs : out, 扫描到的一级目录列表
		 */
		void ScanBaseDir();

    public :
        std::vector<std::wstring> m_BaseDirs;
        std::vector<std::wstring> m_PriorityDirs;

    private :
		INT m_ScanDirs;
		ScanTargetCallback m_ScanTargetCallback;
		std::vector<std::wstring> m_IgnoreDirs;
    };
}

#endif