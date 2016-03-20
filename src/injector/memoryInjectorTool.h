/*
*  Author: HIT CS HDMC team.
*  Create: 2010-3-13 9:20:04
*  Last modified: 2010-6-16 14:11:00
*/

#ifndef MEMORY_INJECTOR_H_
#define MEMORY_INJECTOR_H_

/*
*	common header files
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>	//�䳤����
#include <time.h>
#include <sys/time.h>

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

using namespace std;

#include "ptrace.h"
#include "memoryInjector.h"


class InjectorTool
{
public:
	InjectorTool();
	~InjectorTool();



protected :

    //
    //  ����ע�빤��
    //
    Injector            *m_injector;             // the memory fault injector object

    int                 m_targetPid;      //  target is an existing process.
    char                **m_exeArguments;


    //
    //  ���յ��Ŀ���̨����
    //
    int                 m_argc;                  // the count of the arguement
    char                **m_argv;                // the arguement list

    //
    //  ʹ��-cָ������ע�����ʹ��-l -m -tָ������ע������
    //
    bool                m_hasFaultTable;         // ���ʹ����-c������ʹ��ע���,��ֵΪtrue
    //  when m_hasFaultTable == false
    MemoryFault         m_memoryFault;           //
    //  when m_hasFaultTable == true
    string              m_faultTablePath;
	vector <memFault>   m_faultTable;
};

#endif	/* MEMORY_INJECTOR_H_ */
