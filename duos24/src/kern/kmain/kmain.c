

#include <sys_init.h>
#include <cm4.h>
#include <kmain.h>
#include <stdint.h>
#include <sys_usart.h>
#include <kstdio.h>
#include <sys_rtc.h>
#include <kstring.h>

#define BOATLOADER_SIZE (0x10000U)


#ifndef DEBUG
#define DEBUG 1
#endif




//FLASH(RX): ORIGIN = 0X08010000, LENGTH =448K

static void vector_setup(void)
{
    // Set the Vector Table base address
    // SCB->VTOR = BOATLOADER_SIZE;
}

//	*(.bootloader_section)
void kmain(void)
{
    vector_setup();
    __sys_init();
    kprintf("Hello from OS\n");
    
    while (1)
    {
      
    }
}
