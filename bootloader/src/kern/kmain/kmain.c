

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
#include <sys_bus_matrix.h>
// #include <kstring.h>

#ifndef DEBUG
#define DEBUG 1
#endif

#define SECTOR_NUMBER              4
#define OS_VERSION_STORAGE_SECTOR  7
#define OS_VERSION_ADDRESS         0x08060000
#define TARGET_ADDRESS             0x08010000
#define BOATLOADER_SIZE            (0x10000U)
#define OS_START_ADDRESS           (0X08010000U) //duos 
#define BOATLOADER_SIZE            (0x10000U)
#define MMIO8(address)             (*(volatile uint8_t *)(address))
#define MMIO32(address)            (*(volatile uint32_t *)(address))

#define VERSION_ADDR               ((volatile uint32_t *) 0x2000FFFC)
#define FLASH_BASE                 ((0x40000000UL + 0x00020000UL) + 0x3C00UL) 
// #define FLASH_BASE          (0x08000000)
#define FLASH_SR                   ((volatile uint32_t *)(FLASH_BASE + 0x0CUL))
#define FLASH_KEYR			           ((volatile uint32_t *)( FLASH_BASE + 0x04UL))
#define FLASH_KEYR_KEY1            0x45670123
#define FLASH_KEYR_KEY2            0xCDEF89AB
#define FLASH_CR                   ((volatile uint32_t *)(FLASH_BASE + 0x10))
#define FLASH_CR_SNB_MASK		       0x1f
#define FLASH_CR_PROGRAM_SHIFT	   8
#define FLASH_CR_SNB_SHIFT		     3

// needed for packet protocl
#define PACKET_DATA_MAX_LENGTH 128UL
#define CMD_FIELD_LENGTH 1
#define LENGTH_FIELD_LENGTH 1
#define CRC_FIELD_LENGTH 4
#define PACKET_MAX_LENGTH (CMD_FIELD_LENGTH + LENGTH_FIELD_LENGTH + PACKET_DATA_MAX_LENGTH + CRC_FIELD_LENGTH)
#define BYTE_SIZE

static volatile uint32_t total_number_of_packets;
static uint32_t bits32_from_4_bytes(uint8_t *bytes); 

struct Packet {
  uint8_t command;
  uint8_t length;
  uint8_t* data;
  uint8_t* crc;
};

void CRC_Init(void) {
  RCC->AHB1ENR |= RCC_AHB1ENR_CRCEN;
  CRC->CR |= 1;
}

uint32_t CRC_Calculate(uint32_t *data, uint32_t length) {
  if(!(RCC->AHB1ENR & RCC_AHB1ENR_CRCEN)) {
    CRC_Init();
  }

  for(uint32_t i = 0; i < length; i++) {
    CRC->DR = data[i];
  }

  return CRC->DR;
}

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

static uint32_t calculate_packet_CRC(struct Packet* packet)
{
  CRC_Init();
  uint32_t length = ( CMD_FIELD_LENGTH + LENGTH_FIELD_LENGTH + PACKET_DATA_MAX_LENGTH)  ; 
  if(length % 4 == 0){
    length = length / 4 ;
  }
  else{
    length = length/4 ;
    length++;
  }
  uint32_t data[PACKET_MAX_LENGTH];

  uint8_t bytes[4] = {
    0,
    0,
    packet->command,
    packet->length
  };
  
  data[0] = bits32_from_4_bytes(bytes);
  
  for(uint32_t i = 0, j = 1 ; i < PACKET_DATA_MAX_LENGTH ; i+=4, j++){
    bytes[0] = packet->data[i];
    bytes[1] = packet->data[i + 1];
    bytes[2] = packet->data[i + 2];
    bytes[3] = packet->data[i + 3];

    data[j] = bits32_from_4_bytes(bytes);
  }

  uint32_t crc_value = CRC_Calculate(data, length);

  return crc_value;
}

static uint32_t populate_CRC(struct Packet* packet)
{
  CRC_Init();
  uint32_t length = ( CMD_FIELD_LENGTH + LENGTH_FIELD_LENGTH + PACKET_DATA_MAX_LENGTH)  ; 
  if(length % 4 == 0){
    length = length / 4 ;
  }
  else{
    length = length/4 ;
    length++;
  }
  uint32_t data[PACKET_MAX_LENGTH];

  uint8_t bytes[4] = {
    0,
    0,
    packet->command,
    packet->length
  };
  
  data[0] = bits32_from_4_bytes(bytes);
  
  for(uint32_t i = 0, j = 1 ; i < PACKET_DATA_MAX_LENGTH ; i+=4, j++){
    bytes[0] = packet->data[i];
    bytes[1] = packet->data[i + 1];
    bytes[2] = packet->data[i + 2];
    bytes[3] = packet->data[i + 3];

    data[j] = bits32_from_4_bytes(bytes);
  }

  uint32_t crc_value = CRC_Calculate(data, length);
  uint32_t saved_crc_value = crc_value;
  uint32_t mask = (1<<8)-1 ; 
  for(int i = 3 ; i >=0 ; i--){
    packet->crc[i] = (uint8_t) (crc_value & mask); 
    crc_value >>= 8;
  }

  return packet->crc[2];
}


static void send_packet(struct Packet* packet) {
  populate_CRC(packet);

  Uart_write(packet->command, __CONSOLE);
  Uart_write(packet->length, __CONSOLE);
  for(uint8_t i = 0; i < PACKET_DATA_MAX_LENGTH; i++) {
    Uart_write(packet->data[i], __CONSOLE);
  }
  for(int i = 0; i < 4; i++) {
    Uart_write(packet->crc[i], __CONSOLE);
  }
}

static void debug(const uint8_t* statement) {
  uint8_t length = __strlen(statement);
  uint8_t command = 0;

  uint8_t crc_array[10];
  

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
    .crc = crc_array
  };

  send_packet(&packet);
}

static uint32_t bits32_from_4_different_bytes(uint32_t a, uint32_t b, uint32_t c, uint32_t d){
  return (a << 24) + (b << 16) + (c << 8) + d;
}

static uint32_t bits32_from_4_bytes(uint8_t *bytes){
  uint32_t sum = 0; 
  uint32_t power = 1 << 8; 
  for(uint8_t i = 0; i < 4; i++){
    sum *= power; 
    sum += bytes[i];
  }
  return sum; 
}

static void init(void){
  uint8_t data[PACKET_DATA_MAX_LENGTH] ;
  for(uint8_t i = 0 ; i < PACKET_DATA_MAX_LENGTH ; i++){
    data[i] = 0;
  }
  uint8_t crc[4];
  for(uint8_t i = 0; i < 4; i++) {
    crc[i] = 0;
  } 

  struct Packet packet = {
    .command = 1,
    .length = 0,
    .data = data,
    .crc = crc 
  }; 

  send_packet(&packet);
}

// WARNING:
// packet->data needs to be allocated a size of PACKET_DATA_MAX_LENGTH
// before it is called.
static void packet_from_bytes(uint8_t* data_bytes, struct Packet* packet) {
  packet->command = data_bytes[0];
  packet->length = data_bytes[1];
  // debug(convertu32(packet->length, 10));

  
  for(uint8_t i = 0 ; i < PACKET_DATA_MAX_LENGTH ; i ++) {
    // i + 2 because cmd takes 1 byte, and length takes 1 byte
    packet->data[i] = data_bytes[i + 2];
  }

  for(uint8_t i = 0; i < 4; i++) {
    packet->crc[i] = data_bytes[PACKET_MAX_LENGTH - (4 - i)];
  }

  // no idea why
  uint32_t crc = bits32_from_4_bytes(packet->crc);
}

// WARNING: packet object needs to have statically allocated crc and data fields
static void   receive_packet(struct Packet* packet) {
  uint8_t data[PACKET_MAX_LENGTH];
  for(uint8_t i = 0; i < PACKET_MAX_LENGTH; i++) {
    data[i] = UART_READ(__CONSOLE);
  }
  Uart_flush(__CONSOLE);
  packet_from_bytes(data, packet);

  // debug(convertu32(bits32_from_4_different_bytes(
  //   packet->crc[0],
  //   packet->crc[1],
  //   packet->crc[2],
  //   packet->crc[3]
  // ), 10));
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

  // uint8_t values[5] = {
  //   *(volatile uint32_t*) TARGET_ADDRESS,
  //   *(volatile uint32_t*) (TARGET_ADDRESS + 1),
  //   *(volatile uint32_t*) (TARGET_ADDRESS + 2),
  //   *(volatile uint32_t*) (TARGET_ADDRESS + 3),
  //   '\0'
  // };

  uint8_t values[5];
  uint32_t value = *(volatile uint32_t*) TARGET_ADDRESS;
  uint32_t mask = (1 << 8) - 1;

  values[0] = value & mask;
  value >>= 8;
  values[1] = value & mask;
  value >>= 8;
  values[2] = value & mask;
  value >>= 8;
  values[3] = value & mask;
  value >>= 8;
  values[4] = '\0';
  
  debug(values);
}

static void write_init(void) {
  
  flash_lock();                                                            
  flash_unlock();
  flash_erase_sector(SECTOR_NUMBER, 0x02);
}

static void write_end(void) {
  flash_lock();
}

static void write(struct Packet* packet, uint32_t chunk_index) {
  uint32_t current_target_address = TARGET_ADDRESS + chunk_index * PACKET_DATA_MAX_LENGTH;

  // uint8_t data = 101;
  // flash_program_byte(TARGET_ADDRESS, data);

  // uint8_t arr[] = {10, 12};
  // flash_program_byte(TARGET_ADDRESS, arr[0]);
  // flash_program_byte(TARGET_ADDRESS + 4, arr[1]);
  // flash_program(TARGET_ADDRESS, arr, 2);
  for(int i = 0; i < PACKET_DATA_MAX_LENGTH; i += 4) {
    uint32_t data_to_write = 0, power = (1 << 8);
    for(int j = 3; j >= 0; j--) {
      data_to_write = (data_to_write * power) + (uint32_t) packet->data[i + j];
    }
    // debug("Chunk Index");
    // debug(convertu32(i, 10));
    // debug("data to write:");
    // debug(convertu32(data_to_write, 10));
    flash_program_4_bytes(current_target_address + i, data_to_write);
  }
}

static void make_server_os_console() {
  volatile uint8_t data[PACKET_DATA_MAX_LENGTH];
  for(uint8_t i = 0; i < PACKET_DATA_MAX_LENGTH; i++) {
    data[i] = 0;
  }
  volatile uint8_t crc[4];
  struct Packet packet = {
    .command = 4,
    .length = 0,
    .data = data,
    .crc = crc
  };
  send_packet(&packet);
}

static void test_init() {
  write_init();
  uint8_t crc_passed = 0;
  volatile uint8_t data[PACKET_DATA_MAX_LENGTH];
  volatile uint8_t crc[4];
  struct Packet packet = {
    .data = data,
    .crc = crc
  };
  while(!crc_passed) {
    init();
    receive_packet(&packet);
    uint32_t provided_crc = bits32_from_4_bytes(packet.crc);

    uint32_t calculated_packet_crc = calculate_packet_CRC(&packet);
    if(calculated_packet_crc == provided_crc) {
      crc_passed = 1;
    }
  }

  volatile uint8_t size_info_array[4] = {
    packet.data[0],
    packet.data[1],
    packet.data[2],
    packet.data[3]
  };
  total_number_of_packets = bits32_from_4_bytes(size_info_array);
  // debug(convertu32(total_number_of_packets, 10));

  volatile uint8_t cmd_data[PACKET_DATA_MAX_LENGTH];
  volatile uint8_t cmd_crc[4];
  for(uint32_t i = 0; i < total_number_of_packets; i++) {
    crc_passed = 0;
    while(!crc_passed) {
      for(uint32_t j = 0; j < PACKET_DATA_MAX_LENGTH; j++) {
        cmd_data[j] = 0;
      }
      uint32_t ack = i;
      uint32_t mask = (1 << 8) - 1;
      for(int32_t j = 3; j >= 0; j--) {
        cmd_data[j] = ack & mask;
        ack >>= 8;
        cmd_crc[j] = 0;
      }
      // *(uint32_t *) cmd_crc = (uint32_t) 0;
      struct Packet command_packet = {
        .command = 2,
        .length = 4,
        .data = cmd_data,
        .crc = cmd_crc
      };
      send_packet(&command_packet);
      receive_packet(&packet);

      uint32_t provided_crc = bits32_from_4_bytes(packet.crc);

      uint32_t calculated_packet_crc = calculate_packet_CRC(&packet);
      if(calculated_packet_crc == provided_crc) {
        crc_passed = 1;
      }
    }
    // uint32_t first_4_byte = 0, power = (1 << 8) ; 
    // for(int j = 3 ; j >= 0 ; j--){
    //   uint32_t current_number = packet.data[j];
    //   debug("current number ");
    //   debug(convertu32(current_number, 10));
    //   first_4_byte *= power;
    //   first_4_byte += (uint32_t) packet.data[j];
    // }
    // debug("first 4 bytes ");
    // debug(convertu32(first_4_byte, 10));

    write(&packet, i);
    // debug(convertu32(packet.length, 10));
  }

  write_end();
}

static void coms_testing(void) {
  init();
  volatile uint8_t data[PACKET_DATA_MAX_LENGTH];
  volatile uint8_t crc[4];
  struct Packet packet = {
    .data = data,
    .crc = crc
  };
  receive_packet(&packet);
  send_packet(&packet);
  // uint32_t crc_value = calculate_packet_CRC(&packet);
  // debug(convertu32(crc_value, 10));

  // debug(convertu32(packet.crc[0], 10));
  // debug(convertu32(packet.data[0] + 40, 10));

  // init();
  // receive_packet(&packet);
  // debug(convertu32(packet.data[0], 10));

  
  // debug(convertu32(packet.data[0] + 40, 10));

  // init();
  // receive_packet(&packet);
  // debug(convertu32(packet.data[0], 10));
}

static void jump_to_os(void){
  uint32_t *reset_vector_entry = (uint32_t *)(OS_START_ADDRESS + 4U);
  uint32_t *reset_vector= (uint32_t *)(*reset_vector_entry);

  typedef void(*void_fn) (void);
  void_fn jump_to_os_reset = (void_fn) reset_vector;

  // uint8_t statement[5] = {
  //   *(volatile uint8_t *) OS_START_ADDRESS,
  //   *(volatile uint8_t *) OS_START_ADDRESS + 1,
  //   *(volatile uint8_t *) OS_START_ADDRESS + 2,
  //   *(volatile uint8_t *) OS_START_ADDRESS + 3,
  //   '\0'
  // };
  // debug(statement);
  uint32_t first_value = *(volatile uint32_t *) (OS_START_ADDRESS);
  // debug(convertu32(first_value, 10));
  ms_delay(500);
  SCB->VTOR = BOATLOADER_SIZE;
  jump_to_os_reset();
}

static void static_data_test_crc(void) {
  uint32_t data[PACKET_MAX_LENGTH];
}

static void test_crc(void) {
  // uint32_t data[] = {
  //   1,
  //   2,
  //   3,
  //   4
  // };
  // uint32_t length = 4;

  // uint32_t crc = CRC_Calculate(data, length);
  // debug(convertu32(crc, 10));

  uint8_t data[PACKET_DATA_MAX_LENGTH];
  for(uint8_t i = 0; i < PACKET_DATA_MAX_LENGTH; i++) {
    if(i == 0) data[i] = 'a';
    else data[i] = 0;
  }

  uint8_t crc_array[10];

  struct Packet command_packet = {
      .command = 0,
      .length = 1,
      .data = data,
      .crc = crc_array
    };
    send_packet(&command_packet);
    // debug(convertu32(command_packet.crc[0], 10));
    // debug("a"); 
}

static void set_OS_version(uint32_t version_major, uint32_t version_minor){

  flash_lock();                                                            
  flash_unlock();
  flash_erase_sector(OS_VERSION_STORAGE_SECTOR, 0x02);

  flash_program_4_bytes(OS_VERSION_ADDRESS, version_major);
  flash_program_4_bytes(OS_VERSION_ADDRESS + 4, version_minor);
  flash_lock();
  

}

static uint32_t get_OS_version_major(){
  uint32_t version_major = *(volatile uint32_t*) OS_VERSION_ADDRESS;
  return version_major; 
}
static uint32_t get_OS_version_minor(){
  uint32_t version_minor = *(volatile uint32_t*) (OS_VERSION_ADDRESS + 4);
  return version_minor; 
}




static void check_for_update(void){
  uint8_t data[PACKET_DATA_MAX_LENGTH];
  for(uint8_t i = 0; i < PACKET_DATA_MAX_LENGTH; i++) {
    data[i] = 0;
  }

  uint8_t crc_array[10];
  struct Packet command_packet = {
      .command = 3,
      .length = 0,
      .data = data,
      .crc = crc_array
    };
  struct Packet received_packet = {
    .command = 0,
    .length = 0,
    .data = data,
    .crc = crc_array
   };
  send_packet(&command_packet);
  receive_packet(&received_packet);
  uint32_t version_major = bits32_from_4_different_bytes(
    received_packet.data[0],
    received_packet.data[1],
    received_packet.data[2],
    received_packet.data[3]
  );

  uint32_t version_minor = bits32_from_4_different_bytes(
    received_packet.data[4],
    received_packet.data[5],
    received_packet.data[6],
    received_packet.data[7]
  );
  uint32_t current_version_major = get_OS_version_major();
  uint32_t current_version_minor = get_OS_version_minor();
  if(current_version_major == version_major && current_version_minor == version_minor){
    debug("Already updated");

    
    
  }
  else{
    debug("Need update");
    test_init();

    set_OS_version(version_major, version_minor);


  }

}

static void test_version(void){
  // set_OS_version(3,56);
  // uint32_t version_major = get_OS_version_major(); 
  // uint32_t version_minor = get_OS_version_minor() ; 
  
  // debug(convertu32(version_major, 10));
  // debug(convertu32(version_minor, 10));
  

  // debug(convertu32(current_version, 10));
  check_for_update();
}

void kmain(void)
{
  __sys_init();
  
  // test_crc();
  // test_flash();
  // test_flash_write_4_bytes();
  // test_init();
  // ms_delay(500);
  
  // init();
  // coms_testing();
  // coms_testing();
  // test_ring_buffer();
  test_version();
  make_server_os_console();
  // kprintf("Hello from Bootloader\n");
  // ms_delay(5000);
  *VERSION_ADDR = 1;
  // __sys_disable();
  // Uart_flush(__CONSOLE);
  // debug("hi");
  // Uart_flush(__CONSOLE);
  ms_delay(500);
  // debug("jump to os");

  
  
  jump_to_os();
  while(1) {

  }
}