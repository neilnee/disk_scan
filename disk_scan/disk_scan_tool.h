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
		 * ɨ������ظ����̵�һ��Ŀ¼�����ݹ�����˵�����ɨ�跶Χ�ڵ�Ŀ¼��
		 *
		 * @param baseDirs : out, ɨ�赽��һ��Ŀ¼�б�
		 */
		void ScanBaseDir(std::vector<std::wstring> &baseDirs);

		/**
		 * ���ݹ���ɨ�����Ҫ��ͼƬ�洢Ŀ¼
		 *
		 * @param baseDirs : in, ɨ���Ŀ��Ŀ¼�б�
		 * @param picDir : out, ɨ�赽��ͼƬ�洢Ŀ¼�б�
		 */
		void ScanPicDir(std::vector<std::wstring>* baseDir, std::vector<std::wstring> &picDir);

    public :

    };
}

#endif