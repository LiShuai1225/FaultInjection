/*
*  Author: HIT CS HDMC team.
*  Create: 2010-3-12 8:50
*  Last modified: 2010-6-13 14:06:20
*  Description:
*  	Memory fault injection engine running as a kernel module.
*		This module will create "/proc/memoryEngine/" directory and 9 proc nodes.
*   Write paramenters and request to these proc nodes and read the output from related proc node.
*/

#include "memoryEngine.h"

/*
*	proc entries
*
struct proc_dir_entry *dir = NULL;                  /// the directory of the MEMORY INJECT Moudle
struct proc_dir_entry *proc_pid = NULL;				/// write only
struct proc_dir_entry *proc_va = NULL;				/// write only
struct proc_dir_entry *proc_ctl = NULL;				/// write only
struct proc_dir_entry *proc_kFuncName = NULL;	    /// write only
struct proc_dir_entry *proc_val = NULL;				/// rw
struct proc_dir_entry *proc_signal = NULL;		    /// rw
struct proc_dir_entry *proc_pa = NULL;				/// read only
struct proc_dir_entry *proc_taskInfo = NULL;    	/// read only
*/


/*
* proc node values
*
int             pid;								/// pid
unsigned long   va;					                /// virtual  Address
unsigned long   pa;					                /// physical Addreess
int             ctl;								/// ctl
int             signal;								/// signal
char            kFuncName[MAX_LINE];                /// kFuncName
long            memVal;							    /// memVal

unsigned long   ack_pa;			                    /// physical Address
unsigned long   ack_va;			                    /// virtual  Address
int             ack_signal;						    /// signal
int             ret;                                /// return value
char            taskInfo[PAGE_SIZE];	            /// taskInfo

///////////////////////////////////////////////

unsigned long   userspace_phy_mem;                  /// user space physical memory
long            orig_pa_data;                       /// origin data of the physics memory
long            new_pa_data;                        /// the new data you want write to physics memory

int             faultInterval;
*/


extern struct proc_dir_entry    *proc_pid;				/// write only

extern int                      pid;								/// pid
/*
*
*/
int proc_write_pid( struct file *file,
                    const char __user *buffer,
                    unsigned long count,
                    void * data)
{
	int iRet;
	char sPid[MAX_LINE];

	if(count <= 0)
    {
        return FAIL;
    }

    memset(sPid, '\0', sizeof(sPid));
    /////////////////////////////////////////////////////////////////////
    //
    //  copy_from_user������Ŀ���Ǵ��û��ռ俽�����ݵ��ں˿ռ䣬
    //  ʧ�ܷ���û�б��������ֽ������ɹ�����0.
    //  ��ô�򵥵�һ������ȴ�������������ں˷����֪ʶ,
    //  �����ں˹����쳣����Ĵ���.
    //  ���û��ռ俽�����ݵ��ں���ʱ�����С��,
    //  �����û��ռ�����ݵ�ַ�Ǹ��Ƿ��ĵ�ַ,���ǳ����û��ռ�ķ�Χ��
    //  ������Щ��ַ��û�б�ӳ�䵽�������ܶ��ں˲����ܴ��Ӱ�죬
    //  ��oops�������ϵͳ��ȫ��Ӱ��.
    //  ����copy_from_user�����Ĺ��ܾͲ�ֻ�Ǵ��û��ռ俽�������������ˣ�
    //  ����Ҫ��һЩָ������ͬ������Щ����ķ���.
    //
    //  ����ԭ����[arch/i386/lib/usercopy.c]��
    //  unsigned long
    //  copy_from_user( void *to,
    //                  const void __user *from,
    //                  unsigned long n)
    //
    /////////////////////////////////////////////////////////////////////
    //
    //  ���û��ռ���, ��ַbuffrָ���count�����ݿ������ں˿ռ��ַsPid��
    iRet = copy_from_user(sPid, buffer, count);
	if(iRet != 0)
    {
        dbginfo("Error when copy_from_user...\n");
        return FAIL;
    }

    iRet = sscanf(sPid, "%d", &pid);        //  ��������������sPid��ֵ��ģ���ȫ�ֱ���pid
	if(iRet != 1)
    {
        return FAIL;
    }
	dbginfo("Rcv pid:%d\n",pid);

    return count;
}

