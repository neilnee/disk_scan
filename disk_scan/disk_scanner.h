#ifndef _DISK_SCANNER_H_
#define _DISK_SCANNER_H_

#include <windows.h>
#include <vector>
#include <string>
#include <ShlObj.h>

#define PRIORITY_DIRS {CSIDL_DESKTOP, CSIDL_MYDOCUMENTS, CSIDL_MYPICTURES, CSIDL_COMMON_PICTURES, CSIDL_COMMON_DOCUMENTS, CSIDL_COMMON_DESKTOPDIRECTORY}
#define IGNORE_DIRS {CSIDL_WINDOWS, CSIDL_PROGRAM_FILES, CSIDL_TEMPLATES, CSIDL_PROGRAM_FILESX86, CSIDL_APPDATA, CSIDL_COMMON_APPDATA, CSIDL_COOKIES, CSIDL_INTERNET_CACHE, CSIDL_LOCAL_APPDATA}

#define SCAN_START 1
#define SCAN_FOUND 2
#define SCAN_RESULT 3
#define SCAN_FINISH 4
#define SCAN_STOP 5

static const DWORD PATH_BUF_SIZE  = 512;
static const std::wstring IMG_SUFFIX[] = {
    L".jpg", L".png", L".jpeg", L".bmp", L".tif", L".tiff", L".raw"};

namespace xl_ds_api
{
	/**
	 * 扫描过程中的回调函数
	 * @param eventCode : 回调事件类型 SCAN_START-开始扫描目录; SCAN_FOUND-找到目标目录 SCAN_FINISH-扫描完成
	 * @param scanCount : 已经扫描的目录数
     * @param totalCount : 需要扫描的目录总数
	 * @param directory : 回调事件对应的目录
	 */
    typedef void (*ScanTargetCallback) (INT eventCode, INT scanCount, INT totalCount, std::wstring directory);

    class CScanner
    {
    public :
        CScanner();
        virtual ~CScanner();
		/**
		 * 添加回调接口
		 */
		VOID SetScanTargetCallback(ScanTargetCallback callback);
		/**
		 * 清空缓存的扫描结果
		 */
        VOID ClearResult();
        /**
		 * 根据规则扫描出主要的目标类型存储目录
		 * @param baseDirs : in, 扫描的目标目录列表
		 * @param targetDir : out, 扫描到的目标类型存储目录列表
		 * @param priority : in, 是否是扫描优先目录
		 */
		VOID ScanTargetDir(std::vector<std::wstring>* baseDir, std::vector<std::wstring> &targetDir, BOOL priority);

        VOID SaveImgScanResult(std::vector<std::wstring>* imgDirectorys);

        BOOL LoadImgScanResult(std::vector<std::wstring> &imgDirectorys);
	private :
		VOID Init();
		VOID UnInit();
		/**
		 * 添加目录的辅助方法
		 */
		BOOL PushBackDir(std::vector<std::wstring> &dirList, std::wstring &directory);
		/**
		 * 扫描出本地各磁盘的一级目录
		 */
		VOID InitBaseDir();
    public :
        std::vector<std::wstring> m_BaseDirs;
        std::vector<std::wstring> m_PriorityDirs;
        std::vector<std::wstring> m_ImgDirectorys;
        INT m_ScanDirs;
        INT m_TotalDirs;
        BOOL m_Done;
        BOOL m_Exit;
    private :
		ScanTargetCallback m_ScanTargetCallback;
		std::vector<std::wstring> m_IgnoreDirs;
        INT m_FirstDirs;
    };
}

#endif