

#include <syscall.h>
#include <syscall_def.h>
#include <errno.h>
#include <errmsg.h>
void syscall(uint16_t callno)
{
/* The SVC_Handler calls this function to evaluate and execute the actual function */
/* Take care of return value or code */
	switch(callno)
	{
		/* Write your code to call actual function (kunistd.h/c or times.h/c and handle the return value(s) */
		case SYS_read: 
			break;
		case SYS_write:
			break;
		case SYS_reboot:
			break;	
		case SYS__exit:
			break;
		case SYS_getpid:
			break;
		case SYS___time:
			break;
		case SYS_yield:
			break;				
		/* return error code see error.h and errmsg.h ENOSYS sys_errlist[ENOSYS]*/	
		default: ;
	}
/* Handle SVC return here */
}

