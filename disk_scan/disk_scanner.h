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
	 * ɨ������еĻص�����
	 * @param eventCode : �ص��¼����� SCAN_START-��ʼɨ��Ŀ¼; SCAN_FOUND-�ҵ�Ŀ��Ŀ¼ SCAN_FINISH-ɨ�����
	 * @param scanCount : �Ѿ�ɨ���Ŀ¼��
     * @param totalCount : ��Ҫɨ���Ŀ¼����
	 * @param directory : �ص��¼���Ӧ��Ŀ¼
	 */
    typedef void (*ScanTargetCallback) (INT eventCode, INT scanCount, INT totalCount, std::wstring directory);

    class CScanner
    {
    public :
        CScanner();
        virtual ~CScanner();
		/**
		 * ��ӻص��ӿ�
		 */
		VOID SetScanTargetCallback(ScanTargetCallback callback);
		/**
		 * ��ջ����ɨ����
		 */
        VOID ClearResult();
        /**
		 * ���ݹ���ɨ�����Ҫ��Ŀ�����ʹ洢Ŀ¼
		 * @param baseDirs : in, ɨ���Ŀ��Ŀ¼�б�
		 * @param targetDir : out, ɨ�赽��Ŀ�����ʹ洢Ŀ¼�б�
		 * @param priority : in, �Ƿ���ɨ������Ŀ¼
		 */
		VOID ScanTargetDir(std::vector<std::wstring>* baseDir, std::vector<std::wstring> &targetDir, BOOL priority);

        VOID SaveImgScanResult(std::vector<std::wstring>* imgDirectorys);

        BOOL LoadImgScanResult(std::vector<std::wstring> &imgDirectorys);
	private :
		VOID Init();
		VOID UnInit();
		/**
		 * ���Ŀ¼�ĸ�������
		 */
		BOOL PushBackDir(std::vector<std::wstring> &dirList, std::wstring &directory);
		/**
		 * ɨ������ظ����̵�һ��Ŀ¼
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