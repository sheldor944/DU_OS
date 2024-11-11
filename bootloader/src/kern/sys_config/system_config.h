

#ifndef __SYSTEM_CONFIG_H
#define __SYSTEM_CONFIG_H
#ifdef __cplusplus
extern "C" {
#endif
#include <sys_usart.h>
extern UART_HandleTypeDef huart2;
/**
 * Define console for log data and default display
*/
#define  __CONSOLE &huart2





#ifdef __cplusplus
}
#endif
#endif