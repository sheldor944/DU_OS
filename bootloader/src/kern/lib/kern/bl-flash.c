// #include "../../include/kern/bl-flash.h"
// #include "../../../../../bootloader/src/kern/arch/stm32f446re/include/sys_bus_matrix.h"
// // #include <sys_bus_matrix.h>

// #define MAIN_APP_SECTOR_START (2)
// #define MAIN_APP_SECTOR_END   (7)

// #define FLASH_KEYR			FLASH__BASE + 0x04
// #define FLASH_KEYR_KEY1     0x45670123
// #define FLASH_KEYR_KEY2     0xCDEF89AB

// void flash_unlock(void)
// {
// 	/* Authorize the FPEC access. */
// 	FLASH_KEYR = FLASH_KEYR_KEY1;
// 	FLASH_KEYR = FLASH_KEYR_KEY2;
// }


// void bl_flash_erase_main_application(void) {
//   flash_unlock();
// //   for (uint8_t sector = MAIN_APP_SECTOR_START; sector <= MAIN_APP_SECTOR_END; sector++) {
// //     flash_erase_sector(sector, FLASH_CR_PROGRAM_X32);
// //   }
//   flash_lock();
// }

// void bl_flash_write(const uint32_t address, const uint8_t* data, const uint32_t length) {
//   flash_unlock();
//   flash_program(address, data, length);
//   flash_lock();
// }