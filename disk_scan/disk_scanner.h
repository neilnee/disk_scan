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

static const DWORD BUF_SIZE  = 512;
static const std::wstring IMG_SUFFIX[] = {
    L".jpg", L".png", L".jpeg", L".bmp", L".tif", L".tiff", L".raw"};

namespace xl_ds_api
{
	/**
	 * ɨ������еĻص�����
	 * @param event : �ص��¼����� SCAN_START-��ʼɨ��Ŀ¼; SCAN_FOUND-�ҵ�Ŀ��Ŀ¼
	 * @param scan : �Ѿ�ɨ���Ŀ¼��
	 * @param directory : �ص��¼���Ӧ��Ŀ¼
	 */
    typedef void (*ScanTargetCallback) (INT event, INT scan, std::wstring directory);

    class CScanner
    {
    public :
        CScanner();
        virtual ~CScanner();

		/**
		 * ��ӻص��ӿ�
		 */
		void SetScanTargetCallback(ScanTargetCallback callback);

		/**
		 * ���ݹ���ɨ�����Ҫ��Ŀ�����ʹ洢Ŀ¼
		 * @param baseDirs : in, ɨ���Ŀ��Ŀ¼�б�
		 * @param targetDir : out, ɨ�赽��Ŀ�����ʹ洢Ŀ¼�б�
		 * @param priority : in, �Ƿ���ɨ������Ŀ¼
		 */
		void ScanTargetDir(std::vector<std::wstring>* baseDir, std::vector<std::wstring> &targetDir, BOOL priority);

	private :
		/**
		 * ��ʼ��
		 */
		void Init();

		/**
		 * ����ʼ��
		 */
		void UnInit();

		/**
		 * ���Ŀ¼�ĸ�������
		 */
		void PushBackDir(std::vector<std::wstring> &dirList, std::wstring &directory);

		/**
		 * ɨ������ظ����̵�һ��Ŀ¼
		 */
		void InitBaseDir();

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