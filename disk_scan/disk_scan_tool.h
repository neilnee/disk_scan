#ifndef _DISK_SCAN_TOOL_H_
#define _DISK_SCAN_TOOL_H_

#include <windows.h>
#include <vector>
#include <string>
#include <ShlObj.h>

#define PRIORITY_DIRS {CSIDL_DESKTOP, CSIDL_MYDOCUMENTS, CSIDL_MYPICTURES, CSIDL_COMMON_PICTURES, CSIDL_COMMON_DOCUMENTS, CSIDL_COMMON_DESKTOPDIRECTORY};
#define INGORE_DIRS {}

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
		 * ɨ������ظ����̵�һ��Ŀ¼�����ݹ�����˵�����ɨ�跶Χ�ڵ�Ŀ¼��
		 *
		 * @param baseDirs : out, ɨ�赽��һ��Ŀ¼�б�
		 */
		void ScanBaseDir();

        /**
		 * ɨ�����Ҫ����ɨ���Ŀ¼
		 */
        void ScanPriorityDir();

		/**
		 * ���ݹ���ɨ�����Ҫ��Ŀ�����ʹ洢Ŀ¼
		 *
		 * @param baseDirs : in, ɨ���Ŀ��Ŀ¼�б�
		 * @param targetDir : out, ɨ�赽��Ŀ�����ʹ洢Ŀ¼�б�
		 */
		void ScanTargetDir(std::vector<std::wstring>* baseDir, std::vector<std::wstring> &targetDir, ScanTargetCallback callback);

    public :
        std::vector<std::wstring> m_BaseDirs;
        std::vector<std::wstring> m_PriorityDirs;

    private :
        INT m_ScanDirs;
    };
}

#endif