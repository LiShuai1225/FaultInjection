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
*/
struct proc_dir_entry *dir = NULL;                  /// the directory of the MEMORY INJECT Moudle
struct proc_dir_entry *proc_pid = NULL;				/// write only
struct proc_dir_entry *proc_va = NULL;				/// write only
struct proc_dir_entry *proc_ctl = NULL;				/// write only
struct proc_dir_entry *proc_kFuncName = NULL;	    /// write only
struct proc_dir_entry *proc_val = NULL;				/// rw
struct proc_dir_entry *proc_signal = NULL;		    /// rw
struct proc_dir_entry *proc_pa = NULL;				/// read only
struct proc_dir_entry *proc_taskInfo = NULL;    	/// read only

/*
* proc node values
*/
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

/*
*	kprobe
*/
static struct kprobe kp_kFunc;

struct jprobe jprobe1 =
{
    .entry	= jforce_sig_info,
    .kp     =
    {
        .symbol_name = "force_sig_info",
    },
};

//ʱ���ж�������
static int count = 0;

//����ԭʼ����
static long orig_code = 0;

/*
*  process the request
*/
void do_request(void)
{
	struct task_struct *task = NULL;
	unsigned long pa = 0;
	long kernel_va = 0;
	int status;

    int temp = 0;
	/// get a task's memory map information
	if(ctl == REQUEST_TASK_INFO)
	{
		dbginfo("Rcv request:Get task info\n");

        memset(taskInfo,'\0',sizeof(taskInfo));

        if(pid <= 0)
		{
			ack_signal = ACK_TASK_INFO;
			return;
		}
		task = findTaskByPid(pid);

        dprint("find task %p\n", task);

        if( task != NULL )
		{
			getTaskInfo(task, taskInfo, sizeof(taskInfo));
		    dbginfo("%s", taskInfo);
        }
        else
		{
			dbginfo("No such process\n");
        }
		ack_signal = ACK_TASK_INFO;

		return;
	}
	/// convert a process's linear address to physical address
	else if(ctl == REQUEST_V2P)
	{
		task = findTaskByPid(pid);
		if( task == NULL )
		{
			dbginfo("No such process\n");
			ack_pa = -1;
			ack_signal = ACK_V2P;
			return;
		}
		if( task->mm == NULL )
		{
            dprint("the task %d:%s a kernel thread[task->mm == NULL]...\n", task->pid, task->comm);
			ack_pa = -1;
			ack_signal = ACK_V2P;
			return;
		}
		ack_pa = v2p(task->mm,va,&status);
		if(ack_pa == FAIL)
		{
			dbginfo("No physical address\n");
		}
		ack_signal = ACK_V2P;
		return;
	}
	/// convert kernel virtual address to physical address
	else if(ctl == REQUEST_KV2P)
	{
		ack_pa = kv2p(va,&status);
		if(pa == FAIL)
        {
			dbginfo("No physical address\n");
		}
		ack_signal = ACK_KV2P;
		return;
	}
	/// get kernel function's addr(kernel virtual address)
	else if(ctl == REQUEST_KFUNC_VA)
	{
		ack_va = kFunc2v(kFuncName);
		ack_signal = ACK_KFUNC_VA;
		return;
	}
	/// �����ȡ�ں˺�����ʼ��ַ����
	else if(ctl == REQUEST_READ_KFUNC)
	{
		kernel_va = kFunc2v(kFuncName);
		memVal = *((long *)kernel_va);
		ack_signal = ACK_READ_KFUNC;
	}
	/// �����д�ں˺�����ʼ��ַ����
	else if(ctl == REQUEST_WRITE_KFUNC)
	{
		//����kprobe���ڵ�һ�ε���do_timer()ʱ��ע�����
		int ret;
		count = 0;
		if(strlen(kFuncName) > 0)
		{
			faultInterval = 1;	//���Ͻ�����һ��ʱ������
			kp_kFunc.addr = 0;
			kp_kFunc.symbol_name = kFuncName;
			kp_kFunc.pre_handler = handler_pre_kFunc;
			ret = register_kprobe(&kp_kFunc);
			if(ret < 0)
			{
				dbginfo("Fained to register kprobe\n");
				ack_signal = ACK_WRITE_KFUNC;
				return;
			}

			// �ȴ�����ע�����
			dbginfo("start count\n");
			temp = 0;

			while(1)
			{
				if(count == -1)
				{
					unregister_kprobe(&kp_kFunc);
					dbginfo("recovery\n");
					break;
				}
				if(temp == -1)
				{
					break;
				}
				temp++;
				//dbginfo("count:%d\n",count);
			}
		}
		ack_signal = ACK_WRITE_KFUNC;
		dbginfo("Success to inject MTTR fault\n");
		return;
	}

}

struct dentry* file_entry(struct file *pfile)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)

    return pfile->f_path.dentry;

#else

    return pfile->f_dentry;

#endif

}

/*
*  get a task's memory map information
*/
int getTaskInfo(struct task_struct *pTask, char *pData, int length)
{
	struct mm_struct        *pMM;                   // struct mm_struct is defined in `include/linux/mm_types.h`
	struct vm_area_struct   *pVMA;
	struct vm_area_struct   *p;
	struct dentry *         pPath    = NULL;
	char                    *info    = pData;

    char                    file[MAX_LINE];
	char                    *end, *start;

	long                    phy_addr;
	unsigned long           start_va, end_va;
	int                     status;

	if(pTask == NULL) { return FAIL; }
	if((pMM = pTask->mm) == NULL)
    {
        dprint("the task %d:%s a kernel thread[task->mm == NULL]...\n", pTask->pid, pTask->comm);
        return FAIL;
    }

	memset(pData, '\0', length);


	//  ǰ19���ֶ��ǹ��ڽ����ڴ���Ϣ��������Ϣ
	safe_sprintf(pData, length, info+strlen(info), "%lx%c", pMM->total_vm, DELIMITER);   undbginfo("%lx%c", pMM->total_vm, DELIMITER);
	safe_sprintf(pData, length, info+strlen(info), "%lx%c", pMM->locked_vm, DELIMITER);  undbginfo("%lx%c", pMM->locked_vm, DELIMITER);
	safe_sprintf(pData, length, info+strlen(info), "%lx%c", pMM->shared_vm, DELIMITER);  undbginfo("%lx%c", pMM->shared_vm, DELIMITER);
	safe_sprintf(pData, length, info+strlen(info), "%lx%c", pMM->exec_vm, DELIMITER);    undbginfo("%lx%c", pMM->exec_vm, DELIMITER);

	safe_sprintf(pData, length, info+strlen(info), "%lx%c", pMM->stack_vm, DELIMITER);   undbginfo("%lx%c", pMM->stack_vm, DELIMITER);


/// modify by gatieme for system porting NeoKylin-linux-3.14/16
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 7, 0)
    //  error: ��struct mm_struct�� has no member named ��reserved_vm��
    //
    //  ��linux 3.7.0��ʼ�ں˲���֧��RESERVED_VM
    //  struct mm_struct Ҳû����reserved_mm�ֶ�
    //  struct vm_area_struct�ṹ����flag��־ʹ��ֵ VM_RESERVED -=> (VM_DONTEXPAND | VM_DONTDUMP)
    //
	safe_sprintf(pData, length, info+strlen(info), "%lx%c", pMM->reserved_vm, DELIMITER);undbginfo("%lx%c", pMM->reserved_vm, DELIMITER);
#endif

    safe_sprintf(pData, length, info+strlen(info), "%lx%c", pMM->def_flags, DELIMITER);  undbginfo("%lx%c", pMM->def_flags, DELIMITER);

	safe_sprintf(pData, length, info+strlen(info), "%lx%c", pMM->nr_ptes, DELIMITER);    undbginfo("%lx%c", pMM->nr_ptes, DELIMITER);

	safe_sprintf(pData, length, info+strlen(info), "%lx%c", pMM->start_code, DELIMITER); undbginfo("%lx%c", pMM->start_code, DELIMITER);
	safe_sprintf(pData, length, info+strlen(info), "%lx%c", pMM->end_code, DELIMITER);   undbginfo("%lx%c", pMM->end_code, DELIMITER);
	safe_sprintf(pData, length, info+strlen(info), "%lx%c", pMM->start_data, DELIMITER); undbginfo("%lx%c", pMM->start_data, DELIMITER);
	safe_sprintf(pData, length, info+strlen(info), "%lx%c", pMM->end_data, DELIMITER);   undbginfo("%lx%c", pMM->end_data, DELIMITER);

	safe_sprintf(pData, length, info+strlen(info), "%lx%c", pMM->start_brk, DELIMITER);  undbginfo("%lx%c", pMM->start_brk, DELIMITER);
	safe_sprintf(pData, length, info+strlen(info), "%lx%c", pMM->brk, DELIMITER);        undbginfo("%lx%c", pMM->brk, DELIMITER);
	safe_sprintf(pData, length, info+strlen(info), "%lx%c", pMM->start_stack, DELIMITER);undbginfo("%lx%c", pMM->start_stack, DELIMITER);

	safe_sprintf(pData, length, info+strlen(info), "%lx%c", pMM->arg_start, DELIMITER);  undbginfo("%lx%c", pMM->arg_start, DELIMITER);
	safe_sprintf(pData, length, info+strlen(info), "%lx%c", pMM->arg_end, DELIMITER);    undbginfo("%lx%c", pMM->arg_end, DELIMITER);
	safe_sprintf(pData, length, info+strlen(info), "%lx%c", pMM->env_start, DELIMITER);  undbginfo("%lx%c", pMM->env_start, DELIMITER);
	safe_sprintf(pData, length, info+strlen(info), "%lx%c", pMM->env_end, DELIMITER);    undbginfo("%lx%c", pMM->env_end, DELIMITER);


	pVMA = pMM->mmap;
	if(pVMA == NULL)
    {
        return OK;
    }
	for(p = pVMA; p != NULL; p = p->vm_next)
	{
		//  ��ʼ��ַ
		safe_sprintf(pData, length, info + strlen(info), "%lx %lx ", p->vm_start, p->vm_end);

        //  ����
		(p->vm_flags & VM_READ)   ? safe_sprintf(pData, length, info+strlen(info), "r") : safe_sprintf(pData, length, info+strlen(info), "-");

		(p->vm_flags & VM_WRITE)  ? safe_sprintf(pData, length, info+strlen(info), "w") : safe_sprintf(pData, length, info+strlen(info), "-");

		(p->vm_flags & VM_EXEC)   ? safe_sprintf(pData, length, info+strlen(info), "x") : safe_sprintf(pData, length, info+strlen(info), "-");

        (p->vm_flags & VM_SHARED) ? safe_sprintf(pData, length, info+strlen(info), "s") : safe_sprintf(pData, length, info+strlen(info), "p");


		//  ��Ӧ�ļ���
		if(p->vm_file != NULL)
		{
            //  i find in linux-kernel-3.16
            //  http://lxr.free-electrons.com/source/include/linux/fs.h?v=3.16#L827
            //  struct path             f_path;
            //  #define f_dentry        f_path.dentry
            struct dentry *den = file_entry(p->vm_file);

            //if(p->vm_file->f_path.dentry != NULL)
            if(den != NULL)
            {
				safe_sprintf(pData, length, info+strlen(info), " ");
				memset(file,'\0',sizeof(file));
				//for(pPath = p->vm_file->f_path.dentry;
				for(pPath = den;
                    pPath != NULL;
                    pPath = pPath->d_parent)
				{

                    if(strcmp(pPath->d_name.name, "/") != 0)
					{
						strcpy(file + strlen(file), pPath->d_name.name);
						strcpy(file + strlen(file), "/");
						continue;
					}
					break;
				}
				do
				{
					end = file + strlen(file) - 1;
					for(start = end - 1; *start != '/' && start > file; start--);
					if(*start == '/')	{start++;}
					*end = '\0';

					safe_sprintf(pData, length, info+strlen(info), "/%s", start);
					*start = '\0';
				} while(start > file);
			}
		}
		safe_sprintf(pData, length, info+strlen(info), "%c", DELIMITER);

		//��Ӧ�����ַҳ
		start_va = p->vm_start;
		end_va = p->vm_end;
		while(end_va > start_va)
		{
			safe_sprintf(pData, length, info+strlen(info), "%lx-%lx\t", start_va, start_va + PAGE_SIZE);
			phy_addr = v2p(pMM, start_va, &status);
			if(phy_addr != FAIL)
			{
				safe_sprintf(pData, length, info+strlen(info), "va:0x%lx <--> pa:0x%lx", start_va, phy_addr);
			}
			start_va += PAGE_SIZE;
			safe_sprintf(pData, length, info+strlen(info), "%c", DELIMITER);
		}

		safe_sprintf(pData, length, info+strlen(info), "%c", DELIMITER);
	}

    dbginfo("get task info success...\n");
	return OK;
}

/*
*
*/
static int handler_pre_kFunc(struct kprobe *p, struct pt_regs *regs)
{
	unsigned long va;
	va = (unsigned long)p->addr;
	if(va <= 0) { return OK; }

	//��һ�δ���
	if(count == 0)
	{
		//��ȡǰ64�ֽڣ�ע�����
		orig_code = *((long *)va);
		//_inject_fault(va,memVal);
		*((long *)va) = memVal;	//����
		return OK;
	}
	count ++;
	//�������ʱ��
	if(count == faultInterval + 1)
	{
		//�ָ�code
		*((long *)va) = orig_code;
		count = -1;
	}
	return OK;
}

/*
*	find_task_by_pid maybe not supported
*	O(n) is fine :)
*/
struct task_struct * findTaskByPid(pid_t pid)
{
	struct task_struct *task = NULL;

    //  Traversing the process in the system to find the PID
    //  add by gatieme @2016-03-20
    for_each_process(task)
	{
		if(task->pid == pid)
        {
            dbginfo("find task by pid = %d\n", pid);
			return task;
	    }
    }
	return NULL;
}

/*
* convert a process's linear address to physical address
*/
long v2p(struct mm_struct *pMM,unsigned long va,int *pStatus)
{
	pte_t *pte = NULL;
	unsigned long phyaddress = FAIL;

	pte = getPte(pMM, va);
	if(pte != NULL)
	{
		phyaddress = (pte_val(*pte) & PAGE_MASK) | (va & ~PAGE_MASK);
	}
	return phyaddress;
}

/*
*  convert kernel virtual address to physical address
*
*/
long kv2p(unsigned long va,int *pStatus)
{
    if(va < 0)
    {
		return FAIL;
    }

    if(__pa(va) >= 0)
    {
		return __pa(va);
    }
    return FAIL;
}

/*
*  get kernel function's addr(kernel virtual address)
*	 the kernel function should be looked up in the System.map
*/
static struct kprobe kp;
long kFunc2v(char *funcName)
{
	int ret;
	unsigned long va;

	kp.addr = 0;
	kp.symbol_name = funcName;
	ret = register_kprobe(&kp);
	if(ret < 0)
	{
		dbginfo("Fained to register kprobe\n");
		return FAIL;
	}
	va = (unsigned long)kp.addr;
	unregister_kprobe(&kp);
	if(va == 0)
		return FAIL;
	return va;
}



/*
*
*/
struct vm_area_struct * getVMA(struct mm_struct *pMM,unsigned long va)
{
	struct vm_area_struct *p;
	if(pMM == NULL) return NULL;
	p = pMM->mmap;
	if(p == NULL) return NULL;

	for(; p != NULL; p = p->vm_next)
	{
		if( va >= p->vm_start && va < p->vm_end )
		{
			return p;
		}
	}
	return NULL;
}

/*
*
*/
pte_t * getPte(struct mm_struct *pMM, unsigned long va)
{
	pgd_t *pgd = NULL;
	pmd_t *pmd = NULL;
	pud_t *pud = NULL;
	pte_t *pte = NULL;

	///get the pdg entry pointer
	pgd =  pgd_offset(pMM, va);
	if(pgd_none(*pgd)) { return NULL; }

	pud = pud_offset(pgd,va);
	if(pud_none(*pud)) { return NULL; }

	pmd = pmd_offset(pud,va);
	if(pmd_none(*pmd)) { return NULL; }

	pte = pte_offset_kernel(pmd,va);
	if(pte_none(*pte)) { return NULL; }
	if(!pte_present(*pte)) { return NULL; }
	return pte;
}

/*
*
*/
int setVMAFlags(struct mm_struct *pMM,unsigned long va,int *pStatus,int flags)
{
	struct vm_area_struct *p;
	p = getVMA(pMM,va);
	if(p == NULL) return FAIL;

	if(flags > 0)
	{
		p->vm_flags |= VM_WRITE;
		p->vm_flags |= VM_SHARED;
	}
	if(flags == 0)
	{
		p->vm_flags &= ~VM_WRITE;
		p->vm_flags &= ~VM_SHARED;
	}
	else { return FAIL; }
	return OK;
}

/*
*
*/
int setPageFlags(struct mm_struct *pMM,unsigned long va,int *pStatus,int flags)
{
	pte_t *pte = NULL;
	pte_t ret;
	pte = getPte(pMM, va);
	if( pte == NULL ) { return FAIL; }
	if(flags > 0)
	{
		ret = pte_mkwrite(*pte);
	}
	else if(flags == 0)
	{
		ret = pte_wrprotect(*pte);
	}
	else { return FAIL;	}
	return OK;
}

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

/*
*
*/
int proc_read_virtualAddr(  char * page,
                            char **start,
                            off_t off,
                            int count,
                            int * eof,
                            void * data)
{
	int iLen;

	iLen = sprintf(page, "%lx", ack_va);


    return iLen;
}

/*
*
*/
int proc_write_virtualAddr( struct file *file,
                            const char *buffer,
                            unsigned long count,
                            void * data)
{
	int iRet;
	char sVa[MAX_LINE];

	if(count <= 0)
    {
        return FAIL;
    }

    memset(sVa, '\0', sizeof(sVa));

    iRet = copy_from_user(sVa, buffer, count);
	if(iRet)
    {
        return FAIL;
    }

    iRet = sscanf(sVa,"%lx",&va);
	if(iRet != 1)
    {
        return FAIL;
    }

	dbginfo("Rcv virtual addr:0x%lx\n",va);

    return count;
}

/*
*
*/
int proc_write_ctl( struct file *file,
                    const char *buffer,
                    unsigned long count,
                    void * data)
{
	int iRet;
	char sCtl[MAX_LINE];

	if(count <= 0)
    {
        return FAIL;
    }

    memset(sCtl, '\0', sizeof(sCtl));

    iRet = copy_from_user(sCtl, buffer, count);
	if(iRet)
    {
        return FAIL;
    }

    iRet = sscanf(sCtl,"%d",&ctl);
	if(iRet != 1)
    {
        return FAIL;
    }

    do_request();

    return count;
}

/*
*
*/
int proc_read_signal(   char * page,
                        char **start,
                        off_t off,
                        int count,
                        int * eof,
                        void * data)
{
	int iLen;
    dbginfo("%d\n", ack_signal);
    iLen = sprintf(page, "%d", ack_signal);

    return iLen;
}

/*
*
*/
int proc_write_signal(  struct file *file,
                        const char *buffer,
                        unsigned long count,
                        void * data)
{
	int iRet;
	char sSignal[MAX_LINE];

	if(count <= 0)
    {
        return FAIL;
    }

    memset(sSignal, '\0', sizeof(sSignal));
	iRet = copy_from_user(sSignal, buffer, count);
	if(iRet)
    {
        return FAIL;
    }

    iRet = sscanf(sSignal,"%d",&signal);
	if(iRet != 1)
    {
        return FAIL;
    }

    dbginfo("Rcv signal:%d\n",signal);

    return count;
}

/*
*
*/
int proc_read_pa(char * page,char **start, off_t off, int count, int * eof,void * data)
{
	int iLen;
	iLen = sprintf(page, "%lx", ack_pa);
	return iLen;
}

/*
*
*/
int proc_write_pa(struct file *file,const char *buffer,unsigned long count,void * data)
{
	int iRet;
	char sPa[MAX_LINE];

	if(count <= 0) { return FAIL; }
	memset(sPa, '\0', sizeof(sPa));
	iRet = copy_from_user(sPa, buffer, count);
	if(iRet) { return FAIL; }
	iRet = sscanf(sPa,"%lx",&pa);
	if(iRet != 1) { return FAIL; }
	dbginfo("Rcv pa:0x%lx\n",pa);
	return count;
}

/*
*
*/
int proc_write_kFuncName(struct file *file,const char *buffer,unsigned long count,void * data)
{
	int iRet;
	if(count <= 0) { return FAIL; }
	memset(kFuncName, '\0', sizeof(kFuncName));
	iRet = copy_from_user(kFuncName, buffer, count);
	if(iRet) { return FAIL; }
	//remove '\n'
	if(kFuncName[strlen(kFuncName) - 1] == '\n')
	{
		kFuncName[strlen(kFuncName) - 1] = '\0';
	}
	dbginfo("Rcv kernel func name:%s\n",kFuncName);
	return count;
}

/*
*
*/
int proc_read_taskInfo(char * page,char **start, off_t off, int count, int * eof,void * data)
{
	int iLen;
	iLen = sprintf(page, "%s", taskInfo);
	return iLen;
}

/*
*
*/
int proc_write_memVal(struct file *file,const char *buffer,unsigned long count,void * data)
{
	int iRet;
	char sMemVal[MAX_LINE];

	if(count <= 0)
    {
        return FAIL;
    }

    memset(sMemVal, '\0', sizeof(sMemVal));
	iRet = copy_from_user(sMemVal, buffer, count);
	if(iRet)
    {
        return FAIL;
    }

    iRet = sscanf(sMemVal,"%lx",&memVal);
	if(iRet != 1)
    {
        return FAIL;
    }

    dbginfo("Rcv memVal:0x%lx\n",memVal);
	return count;
}

/*
*
*/
int proc_read_memVal(   char * page,
                        char **start,
                        off_t off,
                        int count,
                        int * eof,
                        void * data)
{
	int iLen;
	iLen = sprintf(page, "%lx", memVal);
	return iLen;
}

/*
*  init memory fault injection module
*  ��ʼ���ڴ�ע�����ģ��
*
*  ʹ��proc_mkdir()����һ��dir = /proc/memoryEngine
*  ������create_proc_read_entry()��������һ��processinfo�ļ���
*  ���Ǵ�ģ�������ȡ����Ϣ����д�뵽��Щ�ļ��С�
*
*/
static int __init initME(void)
{
	/*
     *  create a direntory named "memoryEngine" in /proc for the moudles
     *  as the interface between the kernel and the user program.
     *
     */
    dir = proc_mkdir("memoryEngine", NULL);
	if(dir == NULL)
	{
		dbginfo("Can't create /proc/memoryEngine/\n");
		return FAIL;
	}
    dbginfo("Create /proc/memoryEngine success...\n");

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
    /// modify by gatieme for system porting NeoKylin-linux-3.14/16
    /// error: dereferencing pointer to incomplete type
	dir->owner = THIS_MODULE;
#endif


    /// modify by gatieme for system porting NeoKylin-linux-3.14/16 @ 2016--03-28 20:08
    /*
     *  ==
     *  write in STACK_OVER_FLOW http://stackoverflow.com/questions/26808325/implicit-declaration-of-function-create-proc-entry
     *  ==
     *
     *  proc filesystem has been refactored in 3.10,
     *  the function `create_proc_entry` has been removed,
     *  you should use the full featured `proc_create function` family.
     *
     *  Note that the signatures are different, you can find them in LXR
     *  3.10 version: http://lxr.free-electrons.com/source/include/linux/proc_fs.h?v=3.10
     *  3.9 version: http://lxr.free-electrons.com/source/include/linux/proc_fs.h?v=3.9
     *
     *  You can find greater explanation of using full featured /proc functions in the book Linux Device Drivers 4,
     *  or, if you want shorter solution, check this link (https://github.com/jesstess/ldd4/blob/master/scull/main.c)
     *  where you can see how the struct file_operations has been used. You do not have to setup to all fields of the struct.
     *
     *  but the function  remove_proc_remove, you can do nothing for it, becase there are tow function for it
     *  static inline void proc_remove(struct proc_dir_entry *de) {}
     *  #define remove_proc_entry(name, parent) do {} while (0)
     */

    /// create a file named "pid" in direntory
#ifdef CREATE_PROC_ENTRY

    proc_pid = create_proc_entry("pid", PERMISSION, dir);

    if(proc_pid == NULL)
	{
		dbginfo("Can't create /proc/memoryEngine/pid\n");

        goto create_pid_failed;
	}
	proc_pid->write_proc = proc_write_pid;  /// write only
	//proc_pid->owner = THIS_MODULE;

#elif defined PROC_CREATE
    static const struct file_operations pid_fops =
    {
        .owner = THIS_MODULE,
       // .read = proc_write_read,
        .write = proc_write_pid,          /*  write only  */
    };

    proc_pid = proc_create("pid", PERMISSION, dir, &pid_fops);

    if(proc_pid == NULL)
	{
		dbginfo("Can't create /proc/memoryEngine/pid\n");

        goto create_pid_failed;
	}

#endif
   dbginfo("create /proc/memoryEngine/pid success...\n");

    /// create a file named "virtualAddr" in direntory
#ifdef CREATE_PROC_ENTRY

	proc_va = create_proc_entry("virtualAddr", PERMISSION, dir);
	if(proc_va == NULL)
	{
		dbginfo("Can't create /proc/memoryEngine/virtualAddr\n");

        goto create_va_failed;
	}
	proc_va->read_proc = proc_read_virtualAddr;         // can read
	proc_va->write_proc = proc_write_virtualAddr;       // can write
	//proc_va->owner = THIS_MODULE;

#elif defined PROC_CREATE
    static const struct file_operations va_fops =
    {
        .owner = THIS_MODULE,
	    .read  = proc_read_virtualAddr,                 // can read
	    .write = proc_write_virtualAddr,                // can write
    };

    proc_va = proc_create("virtualAddr", PERMISSION, dir, &va_fops);

    if(proc_va == NULL)
	{
		dbginfo("Can't create /proc/memoryEngine/virtualAddr\n");

        goto create_va_failed;
    }
#endif
    dbginfo("create /proc/memoryEngine/virtualAddr success...\n");


   ///  create a file named "ctl" in direntory
#ifdef CREATE_PROC_ENTRY

    proc_ctl = create_proc_entry("ctl", PERMISSION, dir);
	if(proc_ctl == NULL)
	{
		dbginfo("Can't create /proc/memoryEngine/ctl\n");

        goto create_ctl_failed;

        return FAIL;
	}

    proc_ctl->write_proc = proc_write_ctl;              // write only
	//proc_ctl->owner = THIS_MODULE;

#elif defined PROC_CREATE

    static const struct file_operations ctl_fops =
    {
        .owner = THIS_MODULE,
	    //.read  = proc_read_ctl,                       // can read
	    .write = proc_write_ctl,                        // write only
    };

    proc_ctl = proc_create("ctl", PERMISSION, dir, &ctl_fops);

    if(proc_ctl == NULL)
	{
		dbginfo("Can't create /proc/memoryEngine/ctl\n");

	    goto create_ctl_failed;
    }
#endif
    dbginfo("create /proc/memoryEngine/ctl success...\n");

    ///  create a file named "signal" in direntory
#ifdef CREATE_PROC_ENTRY

    proc_signal = create_proc_entry("signal", PERMISSION, dir);

    if(proc_signal == NULL)
	{
		dbginfo("Can't create /proc/memoryEngine/signal\n");

        goto create_signal_failed;
	}
	proc_signal->read_proc = proc_read_signal;          //  can read
	proc_signal->write_proc = proc_write_signal;        //  can write
	//proc_signal->owner = THIS_MODULE;

#elif defined PROC_CREATE

    static const struct file_operations signal_fops =
    {
        .owner = THIS_MODULE,
	    .read  = proc_read_signal,                       // can read
	    .write = proc_write_signal,                      // can write
    };

    proc_signal = proc_create("signal", PERMISSION, dir, &signal_fops);

    if(proc_signal == NULL)
	{
		dbginfo("Can't create /proc/memoryEngine/signal\n");

        goto create_signal_failed;
	}
#endif
    dbginfo("Create /proc/memoryEngine/signal success\n");


    ///  create a file named "physicalAddr" in direntory
#ifdef CREATE_PROC_ENTRY

	proc_pa = create_proc_entry("physicalAddr", PERMISSION, dir);
	if(proc_pa == NULL)
	{
		dbginfo("Can't create /proc/memoryEngine/physicalAddr\n");

        goto create_pa_failed;
	}

	proc_pa->read_proc = proc_read_pa;                  //  can read
	proc_pa->write_proc = proc_write_pa;                //  can write

#elif defined PROC_CREATE

    static const struct file_operations pa_fops =
    {
        .owner = THIS_MODULE,
	    .read  = proc_read_pa,                         // read only
	    .write = proc_write_pa,                        // write only
    };

    proc_pa = proc_create("physicalAddr", PERMISSION, dir, &pa_fops);

    if(proc_pa == NULL)
	{
		dbginfo("Can't create /proc/memoryEngine/physicalAddr\n");

        goto create_pa_failed;
    }

#endif
    dbginfo("Create /proc/memoryEngine/physicalAddr success\n");


    ///  create a file named "kFuncName" in direntory
#ifdef CREATE_PROC_ENTRY

    proc_kFuncName = create_proc_entry("kFuncName", PERMISSION, dir);

    if(proc_kFuncName == NULL)
	{
		dbginfo("Can't create /proc/memoryEngine/kFuncName\n");

        goto create_kFuncName_failed;

    }
	proc_kFuncName->write_proc = proc_write_kFuncName;  // write only

#elif defined PROC_CREATE

    static const struct file_operations kFuncName_fops =
    {
        .owner = THIS_MODULE,
	    //.read  = proc_read_kFuncName,                         // read only
	    .write = proc_write_kFuncName,                        // write only
    };

    proc_kFuncName = proc_create("kFuncName", PERMISSION, dir, &kFuncName_fops);

    if(proc_kFuncName == NULL)
	{
		dbginfo("Can't create /proc/memoryEngine/kFuncName\n");

        goto create_kFuncName_failed;
    }
#endif
	dbginfo("Create /proc/memoryEngine/kFuncName success\n");

    ///  create a file named "taskInfo" in direntory
#ifdef CREATE_PROC_ENTRY

    proc_taskInfo = create_proc_entry("taskInfo", PERMISSION, dir);

    if(proc_taskInfo == NULL)
	{
		dbginfo("Can't create /proc/memoryEngine/taskInfo\n");

        goto create_taskInfo_failed;
	}
	proc_taskInfo->read_proc = proc_read_taskInfo;      // read only

#elif defined PROC_CREATE

    static const struct file_operations taskInfo_fops =
    {
        .owner = THIS_MODULE,
	    .read  = proc_read_taskInfo,                    // read only
	    //.write = proc_write_taskInfo,                        // write only
    };

    proc_taskInfo = proc_create("taskInfo", PERMISSION, dir, &taskInfo_fops);

    if(proc_taskInfo == NULL)
	{
		dbginfo("Can't create /proc/memoryEngine/taskInfo\n");

        goto create_taskInfo_failed;
	}
#endif
    dbginfo("Create /proc/memoryEngine/taskInfo success\n");

    ///  create a file named "memVal" in direntory
#ifdef CREATE_PROC_ENTRY

    proc_val = create_proc_entry("memVal", PERMISSION, dir);

    if(proc_val == NULL)
	{
		dbginfo("Can't create /proc/memoryEngine/memVal\n");

        goto create_val_failed;
	}

    proc_val->write_proc = proc_write_memVal;           // can write
	proc_val->read_proc = proc_read_memVal;             // can read


#elif defined PROC_CREATE

    static const struct file_operations val_fops =
    {
        .owner = THIS_MODULE,
	    .read  = proc_read_memVal,                      // can read
	    .write = proc_write_memVal,                     // can write
    };

    proc_val = proc_create("memVal", PERMISSION, dir, &val_fops);

    if(proc_val == NULL)
	{
		dbginfo("Can't create /proc/memoryEngine/memVal\n");

        goto create_val_failed;
    }
#endif
    dbginfo("Create /proc/memoryEngine/memVal success\n");

    ret = register_jprobe(&jprobe1);
	if (ret < 0)
	{
		printk("register_jprobe jprobe1 failed, returned %d\n", ret);
		return ret;
	}
	printk("Planted jprobe at force_sig_info: %p\n", jprobe1.kp.addr);

	dbginfo("Memory engine module init\n");
	return OK;


//  remove the proc files
create_val_failed    :
    remove_proc_entry("kFuncName", dir);

create_kFuncName_failed :
    remove_proc_entry("taskInfo", dir);

create_taskInfo_failed  :
    remove_proc_entry("physicalAddr", dir);

create_pa_failed        :
    remove_proc_entry("signal", dir);

create_signal_failed       :
    remove_proc_entry("ctl", dir);

create_ctl_failed        :
    remove_proc_entry("virtualAddr", dir);

create_va_failed       :
    remove_proc_entry("pid", dir);

create_pid_failed       :
    remove_proc_entry("memoryEngine", NULL);

    return FAIL;

}

/// modify by gatieme for system porting NeoKylin-linux-3.14/16 @ 2016--03-28 20:08
// invalid storage class for function ��jforce_sig_info��
static int jforce_sig_info(int sig, struct siginfo *info, struct task_struct *t)
{
    printk("MemSysFI: kernel is sending signal %d to process pid: %d, comm: %s\n", sig, t->pid, t->comm);
/*
    if (f_inject == 'N')
    {
        jprobe_return();
        return 0;
    }

    down_interruptible(&sem);
    if ( addone(addone(inj_info.rear))==inj_info.front )
    {
		*/
        /*error:������*/
       /*
	   sprintf(inj_info.inj_log[inj_info.rear].msg,"caution : buf is full, messages have been dropped\n");
    }
    else
    {
        inj_info.rear = addone(inj_info.rear);
        sprintf(inj_info.inj_log[inj_info.rear].msg,"warning : kernel is sending signal %d to process pid: %d, comm: %s\n",sig,current->pid,current->comm);
        //inj_info->inj_log[inj_info->rear ] = x ;
    }
    up(&sem);
	*/
    /*
    	if(message!=NULL)
    	{
    		for(i=0;i<256;i++)
    			message[i]='\0';

    		sprintf(message,"warning : kernel is sending signal %d to process pid: %d, comm: %s\n\0",sig,current->pid,current->comm);
    	}
    	else
    		return -1;


    	struct bufferList *buflist;
    	buflist = (struct bufferList *)kmalloc(sizeof(struct bufferList),GFP_KERNEL);
    	buflist->pNext = NULL;
    	buflist->buffer = message;

    	down_interruptible(&sem);
    	if(Head==NULL)
    	{
    		Head = buflist;
    		Tail = Head;
    	}
    	else
    	{
    	  Tail->pNext = buflist;
    	  Tail = buflist;
    	}
    	flag = 1;
    	up(&sem);
    	*/

//	wake_up_interruptible(&wq2);

    jprobe_return();
    return 0;
}

/*
*  uninit memory fault injection module
*/
static void __exit exitME(void)
{
	remove_proc_entry("pid", dir);
	remove_proc_entry("virtualAddr", dir);
	remove_proc_entry("ctl", dir);
	remove_proc_entry("signal", dir);
	remove_proc_entry("physicalAddr", dir);
	remove_proc_entry("kFuncName", dir);
	remove_proc_entry("taskInfo", dir);
	remove_proc_entry("memVal", dir);
	remove_proc_entry("memoryEngine", NULL);

    unregister_jprobe(&jprobe1);
	printk("jprobe at %p unregistered.\n",	jprobe1.kp.addr);
	dbginfo("Memory engine module exit\n");
}

module_init(initME);
module_exit(exitME);
MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Gatieme @ HIT CS HDMC team");
MODULE_DESCRIPTION("Memory Engine Module.");

