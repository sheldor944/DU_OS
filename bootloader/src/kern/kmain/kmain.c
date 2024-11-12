

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

// headers for implementing packet protocol
#include <UsartRingBuffer.h>
#include <system_config.h>
// #include <kstring.h>

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

// needed for packet protocl
#define PACKET_DATA_MAX_LENGTH 128UL
#define CMD_FIELD_LENGTH 1
#define LENGTH_FIELD_LENGTH 1
#define CRC_FIELD_LENGTH 4
#define PACKET_MAX_LENGTH (CMD_FIELD_LENGTH + LENGTH_FIELD_LENGTH + PACKET_DATA_MAX_LENGTH + CRC_FIELD_LENGTH)

struct Packet {
  uint8_t command;
  uint8_t length;
  uint8_t* data;
  uint8_t* crc;
};

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

void flash_program_4_bytes(uint32_t address, uint32_t data)
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

static void test_flash(void) {
  kprintf("Initiating flash test...\n");

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
  kprintf("Data written and verified successfully!\n");
}

static void send_packet(struct Packet* packet) {
  Uart_write(packet->command, __CONSOLE);
  Uart_write(packet->length, __CONSOLE);
  for(uint8_t i = 0; i < PACKET_DATA_MAX_LENGTH; i++) {
    Uart_write(packet->data[i], __CONSOLE);
  }
  // Uart_write(packet->crc, __CONSOLE);
  for(int i = 0; i < 4; i++) {
    Uart_write(packet->crc[i], __CONSOLE);
  }
}

static void debug(const uint8_t* statement) {
  uint8_t length = __strlen(statement);
  uint8_t command = 0;

  uint8_t crc[4];
  for(uint8_t i = 0; i < 4; i++) crc[i] = 0;

  uint8_t data[PACKET_DATA_MAX_LENGTH];
  for(uint8_t i = 0; i < PACKET_DATA_MAX_LENGTH; i++) {
    if(i < length) {
      data[i] = statement[i];
      continue;
    }
    data[i] = 0;
  }

  struct Packet packet = {
    .command = command,
    .length = length,
    .data = data,
    .crc = crc
  };

  send_packet(&packet);
}

static uint32_t bits32_from_4_bytes(uint8_t * crc_array){
  uint32_t sum = 0; 
  uint32_t power = 1 << 8; 
  for(uint8_t i = 0; i < 4; i++){
    sum *= power; 
    sum += crc_array[i];
  }
  return sum; 
}

static void init(){
  uint8_t data[PACKET_DATA_MAX_LENGTH] ;
  for(uint8_t i = 0 ; i < PACKET_DATA_MAX_LENGTH ; i++){
    data[i] = 0;
  }
  uint32_t crc = 0; 
  struct Packet packet = {
    .command = 1,
    .length = 1,
    .data = data,
    .crc = crc 
  }; 

  send_packet(&packet);
}

static void packet_from_bytes(uint8_t* data_bytes, struct Packet* packet) {
  uint8_t data[PACKET_DATA_MAX_LENGTH];
  uint8_t crc_array[4] = {data_bytes[PACKET_MAX_LENGTH - 4],
                          data_bytes[PACKET_MAX_LENGTH - 3],
                          data_bytes[PACKET_MAX_LENGTH - 2],
                          data_bytes[PACKET_MAX_LENGTH - 1]
                          };

  uint32_t crc = bits32_from_4_bytes(crc_array);

  for(uint8_t i = 0 ; i < PACKET_DATA_MAX_LENGTH ; i ++){
    data[i] = data_bytes[i + 2];
  }

  packet->command = data_bytes[0];
  packet->length = data_bytes[1];
  packet->data = data; 
  packet->crc = crc;
}

static void receive_packet(void) {
  uint8_t data[PACKET_MAX_LENGTH];
  for(uint8_t i = 0; i < PACKET_MAX_LENGTH; i++) {
    while(data[i] = Uart_read(__CONSOLE)){

    }
    // data[i] = Uart_read(__CONSOLE);
  }
  struct Packet packet ;
  packet_from_bytes(data, &packet); 
  uint8_t temp[4] = {
    packet.data[0],
    packet.data[1],
    packet.data[2],
    packet.data[3]
  };
  debug(temp);
}

static int8_t test_read(void) {
  int8_t read_value;
  while(1) {
    read_value = Uart_read(__CONSOLE);
    // debug(read_value);
    if(read_value != -1) {
      break;
    }
    ms_delay(100);
  }
  return read_value;
}

static void test_ring_buffer(void) {
  char ch[] = {65, '\0'};
  // debug(ch);
  uint8_t data[5];
  for(int8_t i = 0; i < 4; i++) {
    data[i] = test_read();
  }
  data[4] = '\0';
  debug(data);

  flash_lock();                                                            
  flash_unlock();
  flash_erase_sector(SECTOR_NUMBER, 0x02);

  // uint8_t data = 101;
  // flash_program_byte(TARGET_ADDRESS, data);

  uint8_t arr[] = {10, 12};
  flash_program_byte(TARGET_ADDRESS, data[0] + 1);
  flash_program_byte(TARGET_ADDRESS + 4, data[1] + 2);
  flash_program_byte(TARGET_ADDRESS + 8, data[2]);
  flash_program_byte(TARGET_ADDRESS + 12, data[3]);

  // flash_program(TARGET_ADDRESS, arr, 2);

  flash_lock();

  // uint8_t read_value = *(volatile uint32_t*) TARGET_ADDRESS;
  // kprintf("Value: %d\n", read_value);

  uint8_t values[5] = {
    *(volatile uint32_t*) TARGET_ADDRESS,
    *(volatile uint32_t*) (TARGET_ADDRESS + 4),
    *(volatile uint32_t*) (TARGET_ADDRESS + 8),
    *(volatile uint32_t*) (TARGET_ADDRESS + 12),
    '\0'
  };
  debug(values);
}

static void test_flash_write_4_bytes(void) {
  flash_lock();                                                            
  flash_unlock();
  flash_erase_sector(SECTOR_NUMBER, 0x02);

  // uint8_t arr[] = {(1 << 0 | 1 << 8 | 1 << 16 | 1 << 24)};
  // flash_program_byte(TARGET_ADDRESS, data[0] + 1);
  // flash_program_byte(TARGET_ADDRESS + 4, data[1] + 2);
  // flash_program_byte(TARGET_ADDRESS + 8, data[2]);
  // flash_program_byte(TARGET_ADDRESS + 12, data[3]);
  uint32_t num = (65 << 0 | 70 << 8 | 74 << 16 | 78 << 24);
  flash_program_4_bytes(TARGET_ADDRESS, num);

  // flash_program(TARGET_ADDRESS, arr, 2);

  flash_lock();

  // uint8_t read_value = *(volatile uint32_t*) TARGET_ADDRESS;
  // kprintf("Value: %d\n", read_value);

  uint8_t values[5] = {
    *(volatile uint32_t*) TARGET_ADDRESS,
    *(volatile uint32_t*) (TARGET_ADDRESS + 1),
    *(volatile uint32_t*) (TARGET_ADDRESS + 2),
    *(volatile uint32_t*) (TARGET_ADDRESS + 3),
    '\0'
  };
  debug(values);
}

void kmain(void)
{
  __sys_init();
  // test_flash();
  test_flash_write_4_bytes();

  test_ring_buffer();

  // kprintf("Hello from Bootloader\n");
  ms_delay(5000);
  *VERSION_ADDR = 1;
  // __sys_disable();
  
  // jump_to_os();
  while(1) {

  }
}