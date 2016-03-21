/*
*  Author: HIT CS HDMC team.
*  Create: 2010-3-13 9:20:04
*  Last modified: 2010-6-16 14:11:00
*/

#ifndef MEMORY_INJECTORTOOL_H_
#define MEMORY_INJECTORTOOL_H_

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
#include "memoryFault.h"

class InjectorTool
{
public:
	InjectorTool( );
	~InjectorTool( );
    void usage( );

    int initFaultTable( int argc, char **argv );
    int startInjection( );
    Injector* CreateInjector( int argc, char **argv );
    int initFaultTable( );
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
    //  ʹ��-cָ������ע���
    //  ʹ��-l[ע��λ��] -m[ģʽ, ������ǵ�ַ] -t[]ָ������ע������
    //
    bool                m_hasFaultTable;         // ���ʹ����-c������ʹ��ע���,��ֵΪtrue
    //  when m_hasFaultTable == false
    MemoryFault         m_memoryFault;           //
    //  when m_hasFaultTable == true
    string                  m_memoryFaultTablePath;
	vector <MemoryFault>    m_memoryFaultTable;
};

#endif	/* MEMORY_INJECTORTOOL_H_ */
