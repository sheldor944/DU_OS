
 #include <stdarg.h>
#include <kstdio.h>
#include <sys_usart.h>
#include <UsartRingBuffer.h>
#include <kstring.h>
#include <float.h>
#include <system_config.h>

/**
* first argument define the type of string to kprintf and kscanf, 
* %c for charater
* %s for string, 
* %d for integer
* %x hexadecimal
* %o octal number
* %f for floating point number
*/
// Simplified version of printf
void kprintf(char *format,...)
{
//write your code here
	char *tr;
	uint32_t i;
	uint8_t *str;
	va_list list;
	double dval;
	//uint32_t *intval;
	va_start(list,format);
	for(tr = format;*tr != '\0';tr++)
	{
		while(*tr != '%' && *tr!='\0')
		{
			Uart_write(*tr,__CONSOLE);
			tr++;
		}
		if(*tr == '\0') break;
		tr++;
		switch (*tr)
		{
		case 'c': i = va_arg(list,int);
			Uart_write(i,__CONSOLE);
			break;
		case 'd': i = va_arg(list,int);
			if(i<0)
			{
				Uart_write('-',__CONSOLE);
				i=-i;				
			}
			Uart_sendstring((char*)convert(i,10),__CONSOLE);
			break;
		case 'o': i = va_arg(list,int);
			if(i<0)
			{
				Uart_write('-',__CONSOLE);
				i=-i;				
			}
			Uart_sendstring((char*)convert(i,8),__CONSOLE);
			break;
		case 'x': i = va_arg(list,int);
			/*if(i<0)
			{
				Uart_write('-',__CONSOLE);
				i=-i;				
			}*/
			Uart_sendstring((char*)convertu32(i,16),__CONSOLE);
			break;
		case 'u':	
		case 's': str = va_arg(list,uint8_t*);
			Uart_sendstring((char*)str,__CONSOLE);
			break;
		case 'f': 
			dval = va_arg(list,double);
			Uart_sendstring((char*)float2str(dval),__CONSOLE);
			break;	
		default:
			break;
		}
	}
	va_end(list);
}

void putstr(const uint8_t *str,size_t size)
{
	for(uint32_t i=0;i<size;i++)
	{
		Uart_write(str[i],__CONSOLE);
	}
}

// Simplified version of scanf
void kscanf(char *format,...)
{
//write your code here
	va_list list;
	char *ptr;
	uint8_t buff[50];
	uint8_t *str;
	int len;
	ptr=format;
	va_start(list,format);
	while (*ptr)
	{
		if(*ptr == '%') //looking for format of an input
		{
			ptr++;
			switch (*ptr)
			{
			case 'c': //charater
				*(uint8_t*)va_arg(list,uint8_t*)=Uart_read(__CONSOLE);
				break;
			case 'd': //integer number 
				//uart_USART_READ_STR(USART2,buff,50); 
				*(uint32_t*)va_arg(list,uint32_t*)=__str_to_num(buff,10);	
				break;
			case 's': //string without spaces
				//_USART_READ_STR(USART2,buff,50); 
				str = va_arg(list,uint8_t*);
				len = __strlen(buff);
				for(int u = 0; u<=len; u++)	// copy from buff to user defined char pointer (i.e string)
					str[u] = buff[u];	
				break;
			case 'x': //hexadecimal number
				//_USART_READ_STR(USART2,buff,50); 
				*(int*)va_arg(list,uint32_t*)=__str_to_num(buff,16);	
				break;	
			case 'o': //octal number
				//_USART_READ_STR(USART2,buff,50); 
				*(uint32_t*)va_arg(list,uint32_t*)=__str_to_num(buff,8);	
				break;	
			case 'f': //floating point number
				//_USART_READ_STR(USART2,buff,50);
				//*(uint32_t*)va_arg(list,double*) = __str_to_num(buff,10);
				*(float*)va_arg(list,float*) = str2float(buff);	// Works for float but not for double !!!
				break;	
			default: //rest not recognized
				break;
			}
		}
		ptr++;
	}
	va_end(list);
}
