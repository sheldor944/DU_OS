

#include <sys_init.h>
#include <cm4.h>
#include <kmain.h>
#include <stdint.h>
#include <sys_usart.h>
#include <kstdio.h>
#include <sys_rtc.h>
#include <kstring.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef DEBUG
#define DEBUG 1
#endif

#define BOATLOADER_SIZE (0x10000U)
#define OS_START_ADDRESS (0X08010000U) //duos 
#define BOATLOADER_SIZE (0x10000U)


#define VERSION_ADDR ((volatile uint32_t *) 0x2000FFFC)

static void jump_to_os(void){
  uint32_t *reset_vector_entry = (uint32_t *)(OS_START_ADDRESS + 4U);
  uint32_t *reset_vector= (uint32_t *)(*reset_vector_entry);

  typedef void(*void_fn) (void);
  void_fn jump_to_os_reset = (void_fn) reset_vector;
  SCB->VTOR = BOATLOADER_SIZE;

  jump_to_os_reset();
}

void kmain(void)
{
  __sys_init();
  kprintf("Hello from Bootloader\n");
  ms_delay(5000);
  *VERSION_ADDR = 1;
  // __sys_disable();
  
  jump_to_os();
  while(1) {

  }
}