

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
// #include <bl-flash.h>
// #include "../include/kern/bl-flash.h"

#ifndef DEBUG
#define DEBUG 1
#endif

#define SECTOR_NUMBER      7
#define TARGET_ADDRESS  0x08060000
#define BOATLOADER_SIZE (0x10000U)
#define OS_START_ADDRESS (0X08010000U) //duos 
#define BOATLOADER_SIZE (0x10000U)
#define MMIO8(address)  (*(volatile uint8_t *)(address))
#define MMIO32(address)  (*(volatile uint32_t *)(address))

#define VERSION_ADDR        ((volatile uint32_t *) 0x2000FFFC)
#define FLASH_BASE          ((0x40000000UL + 0x00020000UL) + 0x3C00UL) 
// #define FLASH_BASE          (0x08000000)
#define FLASH_SR            ((volatile uint32_t *)(FLASH_BASE + 0x0CUL))
#define FLASH_KEYR			    ((volatile uint32_t *)( FLASH_BASE + 0x04UL))
#define FLASH_KEYR_KEY1     0x45670123
#define FLASH_KEYR_KEY2     0xCDEF89AB
#define FLASH_CR            ((volatile uint32_t *)(FLASH_BASE + 0x10))
#define FLASH_CR_SNB_MASK		0x1f
#define FLASH_CR_PROGRAM_SHIFT		8
#define FLASH_CR_SNB_SHIFT		3

void flash_lock(void);

uint8_t flash_read_byte(uint32_t* address)
{
  return MMIO8(address);  // Read a byte from the specified address
}

uint32_t flash_read_register(uint32_t* address) {
  return MMIO32(address);
}


void flash_unlock(void)
{
  kprintf("flash unlock process started\n");
  if ((*FLASH_CR & FLASH_CR_LOCK) != 0) {  // Check if flash is locked
    /* Authorize the FPEC access. */
    // int32_t data = flash_read_register(FLASH_KEYR);
    // kprintf("value of FLASH_KEYR %d\n", data);

    *FLASH_KEYR = FLASH_KEYR_KEY1;
    // data = flash_read_register(FLASH_KEYR);
    // kprintf("value of FLASH_KEYR %d\n", data);
    
    *FLASH_KEYR = FLASH_KEYR_KEY2;
    // data = flash_read_register(FLASH_KEYR);
    // kprintf("value of FLASH_KEYR %d\n", data);
  }

  if ((*FLASH_CR & FLASH_CR_LOCK) != 0) {  // Check if flash is locked
    kprintf("Sad. Did not get unlocked.\n");
    return;
  }

  kprintf("flash should be unlocked now.\n");
}

void flash_lock(void)
{
	*FLASH_CR |= FLASH_CR_LOCK;
}

static inline void flash_set_program_size(uint32_t psize)
{
	*FLASH_CR &= ~(FLASH_CR_PG_Msk << FLASH_CR_PROGRAM_SHIFT);
	*FLASH_CR |= psize << FLASH_CR_PROGRAM_SHIFT;
}


void flash_wait_for_last_operation(void)
{
  // while ((MMIO8(FLASH_SR) & FLASH_SR_BSY) != 0);
	// while ((MMIO32(FLASH_SR) & FLASH_SR_BSY) == FLASH_SR_BSY);
  kprintf("checking if flash is busy.\n");
  while(*FLASH_SR & FLASH_SR_BSY == FLASH_SR_BSY) {
  }
  kprintf("flash is now free to work\n");
}

void flash_program_byte(uint32_t address, uint8_t data)
{
  kprintf("flashing a byte to address %d.\n", address);

	flash_wait_for_last_operation();
	flash_set_program_size(0);

	*FLASH_CR |= FLASH_CR_PG;

	// MMIO8(address) = data;
  (* (volatile uint32_t*) address) = data;
  // *address = data

	flash_wait_for_last_operation();

	*FLASH_CR &= ~FLASH_CR_PG;		/* Disable the PG bit. */

  kprintf("hopefully a byte is flashed.\n");
}

void flash_program(uint32_t address, const uint8_t *data, uint32_t len)
{
	/* TODO: Use dword and word size program operations where possible for
	 * turbo speed.
	 */
	uint32_t i;
	for (i = 0; i < len; i++) {
		flash_program_byte(address+i, data[i]);
	}
}

void flash_erase_sector(uint8_t sector, uint32_t program_size)
{
	flash_wait_for_last_operation();
	flash_set_program_size(program_size);

	/* Sector numbering is not contiguous internally! */
	if (sector >= 12) {
		sector += 4;
	}

	*FLASH_CR &= ~(FLASH_CR_SNB_MASK << FLASH_CR_SNB_SHIFT);
	*FLASH_CR |= (sector & FLASH_CR_SNB_MASK) << FLASH_CR_SNB_SHIFT;
	*FLASH_CR |= FLASH_CR_SER;
	*FLASH_CR |= FLASH_CR_STRT;

	flash_wait_for_last_operation();
	*FLASH_CR &= ~FLASH_CR_SER;
	*FLASH_CR &= ~(FLASH_CR_SNB_MASK << FLASH_CR_SNB_SHIFT);

}




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
  // ms_delay(100);    
  flash_lock();                                                            
  flash_unlock();
  flash_erase_sector(SECTOR_NUMBER, 0x02);

  // uint8_t data = 101;
  // flash_program_byte(TARGET_ADDRESS, data);

  uint8_t arr[] = {10, 12};
  flash_program_byte(TARGET_ADDRESS, arr[0]);
  flash_program_byte(TARGET_ADDRESS + 4, arr[1]);
  // flash_program(TARGET_ADDRESS, arr, 2);

  flash_lock();

  uint8_t read_value = *(volatile uint32_t*) TARGET_ADDRESS;
  kprintf("Value: %d\n", read_value);

  read_value = *(volatile uint32_t*) (TARGET_ADDRESS + 4);
  kprintf("Second Value: %d\n", read_value);


  // ms_delay(100);
  kprintf("Before calling the flash erase function \n");
  // flash_erase_sector(4,0x02);
  // flash_erase_sector(5,0x02);
  // flash_erase_sector(6,0x02);
  // flash_erase_sector(TARGET_ADDRESS, 0x02);

  // uint8_t data[1];
  // for(uint32_t i = 0; i < 1; i++) {
  //   data[i] = (uint8_t)(i + 1);  // Fill with values from 1 to 255
  // }
  // flash_program(TARGET_ADDRESS, data, 1);
  flash_lock();
  // for (uint32_t i = 0; i < 1; i++) {
  //   uint8_t read_value = *(volatile uint32_t*) (TARGET_ADDRESS + i);
  //   kprintf("value is %d\n",read_value);
  //   if (read_value != data[i]) {
  //     kprintf("Data verification failed at address %08X\n", TARGET_ADDRESS + i);
  //     return;
  //   }
  // }

  kprintf("Data written and verified successfully!\n");
  kprintf("Hello from Bootloader\n");
  ms_delay(5000);
  *VERSION_ADDR = 1;
  // __sys_disable();
  
  // jump_to_os();
  while(1) {

  }
}