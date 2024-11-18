import serial
from enum import Enum
import os
import math
from typing import List
import time

# Replace '/dev/ttyUSB0' with the correct device for your STM32 connection
SERIAL_PORT = '/dev/ttyACM0'
BAUD_RATE = 115200  # Set to match the UART2 configuration on STM32
PACKET_DATA_MAX_LENGTH = 128
CMD_FIELD_LENGTH = 1
LENGTH_FIELD_LENGTH = 1
CRC_FIELD_LENGTH = 4
PACKET_MAX_LENGTH = \
    CMD_FIELD_LENGTH + \
    LENGTH_FIELD_LENGTH + \
    PACKET_DATA_MAX_LENGTH + \
    CRC_FIELD_LENGTH
ENCODER = 'utf-8'
OS_FILENAME = 'output.bin'

last_sent = -1

class State(Enum):
    COMMAND_RECEIVE = 0
    DEBUG = 1
    INITIALIZATION = 2
    SEND = 3

class Packet:
    def __init__(self, command: int, length: int, data: List[bytes], crc: int):
        self.command = command
        self.length = length
        self.data = data
        self.crc = crc # 4 bytes

    def __repr__(self):
        return f"Packet(command={self.command}, length={self.length}, data={self.data}, crc={self.crc})"
    
    def to_bytes(self) -> bytes:
        command_bytes = self.command.to_bytes(1, byteorder='big')
        length_bytes = self.length.to_bytes(1, byteorder='big')
        
        data_bytes = bytearray(PACKET_DATA_MAX_LENGTH)
        for i in range(min(len(self.data), PACKET_DATA_MAX_LENGTH)):
            data_bytes[i] = self.data[i][0]
        
        crc_bytes = self.crc.to_bytes(4, byteorder='big')
        
        packet_bytes = command_bytes + length_bytes + bytes(data_bytes) + crc_bytes
        return packet_bytes

current_state = State.INITIALIZATION
file_chunks = []

def read_file_in_chunks(filename, chunk_size=PACKET_DATA_MAX_LENGTH):
    global file_chunks
    file_chunks = []  # Reset the global variable in case it's already populated
    with open(filename, 'rb') as file:
        while True:
            chunk = file.read(chunk_size)
            if len(chunk) == 0:  # End of file
                break
            elif len(chunk) < chunk_size:  # Last chunk with padding of 1s in all bits
                chunk += b'\xFF' * (chunk_size - len(chunk))
            file_chunks.append(chunk)
    print("Number of chunks: ", len(file_chunks))
    print("Type of file_chunks: ", type(file_chunks))
    print("Type of the items of file_chunks: ", type(file_chunks[0]))

def print_chunks():
    for i, chunk in enumerate(file_chunks):
        if i == 2:
            break
        print(f"Chunk {i}: ", " ".join(f"{byte:02x}" for byte in chunk))

'''
    Packet Protocol Command Types Meaning:
    cmd=0: DEBUG
    cmd=1: INITIALIZATION
    cmd=2: SEND, the client will provide some ack
'''

def to_list_of_integers(packet: Packet):
    packet_list = [(packet.command << 8) + packet.length]
    # print(f"to list of integers : packet.data : {type(packet.data)} " )
    for i in range(0, len(packet.data), 4):
        byte_chunk = packet.data[i:i+4]
        # print(f"type of byte chunk {type(byte_chunk)}")
        int_value = int.from_bytes(b''.join(byte_chunk), byteorder='big')
        packet_list.append(int_value)
    return packet_list

def bytearray_to_packet(byte_array: bytearray):
    if len(byte_array) != PACKET_MAX_LENGTH:
        raise ValueError(f"Invalid packet size, expected {PACKET_MAX_LENGTH} bytes, got {len(byte_array)} bytes")

    # Extract command, length, data, and CRC from the byte_array
    command = byte_array[0]  # First byte is the command
    length = byte_array[1]   # Second byte is the length
    data_integers = byte_array[2:2+PACKET_DATA_MAX_LENGTH]  # Data starts at index 2 and has a length of 'length'
    data = [bytes([b]) for b in data_integers]
    crc_byte_array = byte_array[2+PACKET_DATA_MAX_LENGTH:]  # CRC is the last 4 bytes after the data
    # print(type(crc_byte_array))

    # crc_value = int.from_bytes(b''.join(crc_byte_array), byteorder='big')
    crc_value = int.from_bytes(crc_byte_array, byteorder='big')


    # UPDATED recently. Change it if it does not work
    # print("bytearray_to_packet function: ", crc_value)
    # print("\n\nreceived byte array: ", byte_array, "\n\n")
    
    # Initialize the Packet object
    packet = Packet(command=command, length=length, data=data, crc=crc_value)
    
    return packet

def calculate_crc(packet: Packet):
    return 0

def crc_validate(packet):
    return True

def bytes_to_int(byte):
    return int.from_bytes(byte, byteorder='big')

def int_to_byte(integer: int):
    return integer.to_bytes(1, byteorder='big')

def file_size(file_path):
    return os.path.getsize(file_path)

# def handle_command_receive(ser: serial.Serial):
#     if ser.in_waiting > 0:
#         cmd_byte = ser.read(1)
#         cmd = bytes_to_int(cmd_byte)
        
#         if cmd == 0:
#             current_state = State.DEBUG
#             handle_debug(ser)
#     pass

def handle_debug(packet: Packet):
    print("\n\n----\nDEBUGGING START:\n")
    length = packet.length

    # print("Debug packet:\n", packet.to_bytes, "\n\n")

    byte_array = bytearray()
    for i in range(PACKET_DATA_MAX_LENGTH):
        byte = packet.data[i]
        if i >= length:
            continue
        byte_array.extend(byte)
    data = bytes(byte_array)
    print("[",  data.decode(ENCODER), "]")

    print("\nDEBUGGING END\n----\n\n")


def handle_initialization(ser: serial.Serial, packet: Packet):
    print("INITIALIZING PROTOCOL.")
    size = file_size(OS_FILENAME)
    print(f"File size: {size}")

    total_number_of_packets = math.ceil(size / PACKET_DATA_MAX_LENGTH)
    print("Total number of packets: ", total_number_of_packets)
    
    bytes_4 = total_number_of_packets.to_bytes(4, byteorder='big')
    bytes_124 = bytes(124)
    combined_bytes = bytes_4 + bytes_124
    data_list = [bytes([b]) for b in combined_bytes]


    sendPacket = Packet(command=1, length=32, data=data_list, crc=12)
    # crc = 0
    packet_data_integer_list = to_list_of_integers(sendPacket)
    # print("handle_init: packet_data_integer_list size: ", len(packet_data_integer_list))
    crc = crc32_stm32_32bit(packet_data_integer_list)
    sendPacket.crc = crc
    print("CRC from server init: ", crc)
    
    packet_bytes = sendPacket.to_bytes()
    # print("\n\nsending packet bytes...: ", packet_bytes, "\n\n")
    ser.write(packet_bytes)

def handle_send(ser: serial.Serial, packet: Packet):
    chunk_index = int.from_bytes(b''.join(packet.data)[:4], byteorder="big")
    print("chunk index: ", chunk_index)
    global last_sent
    last_sent = chunk_index
    data_list = [bytes([b]) for b in file_chunks[chunk_index]]
    packet = Packet(
        command=2,
        length=PACKET_DATA_MAX_LENGTH,
        data=data_list,
        crc=0
    )
    crc_value = crc32_stm32_32bit(to_list_of_integers(packet))
    packet.crc = crc_value
    packet_bytes = packet.to_bytes()
    
    # print("The packet being sent is: ", packet)
    print("Bytes are: ", packet_bytes)
    ser.write(packet_bytes)

def receive_packet_bytes(ser: serial.Serial):
    byte_array = bytearray()
    cnt = 0
    
    while ser.in_waiting < PACKET_MAX_LENGTH:
        continue
    # print("Serial size: ", ser.in_waiting)
    byte = ser.read(PACKET_MAX_LENGTH)
    byte_array.extend(byte)
    # print("\n\n\nreceived byte_array:\n", byte_array, "\n\n\n")
    return byte_array

def operate(ser: serial.Serial, packet: Packet):
    cmd = packet.command
    print("Command: ", cmd)
    if cmd == 0:
        handle_debug(packet)
    if cmd == 1:
        handle_initialization(ser, packet)
    if cmd == 2:
        handle_send(ser, packet)
    # implement remaining

def test_write(ser: serial.Serial):
    data = b'\x42'
    for i in range(4):
        ser.write(data)
        ser.flush()
    # ser.flush()      # Wait until all data is sent
    # time.sleep(1)

def crc32_stm32_32bit(data: list[int]) -> int:
    """
    Calculate CRC-32 using the same polynomial as the STM32F446RE (Ethernet CRC-32).
    Polynomial: 0x04C11DB7
    Initial value: 0xFFFFFFFF
    XOR out: 0xFFFFFFFF

    Parameters:
    data (list[int]): List of 32-bit integers to calculate the CRC on.

    Returns:
    int: Calculated CRC-32 checksum.
    """
    # Initialize CRC parameters
    crc = 0xFFFFFFFF
    polynomial = 0x04C11DB7

    for word in data:
        crc ^= word  # XOR with the 32-bit word directly

        # Process each bit in the 32-bit word
        for _ in range(32):
            if crc & 0x80000000:
                crc = (crc << 1) ^ polynomial
            else:
                crc <<= 1

            # Keep CRC within 32 bits
            crc &= 0xFFFFFFFF

    return crc

def test_crc(packet: Packet):
    packet_list = [(packet.command << 8) + packet.length]
    # print(f"in test crc : type of packet.data {type(packet.data)}")
    for i in range(0, len(packet.data), 4):
        byte_chunk = packet.data[i:i+4]
        # print(f"in test crc : type of byte chunk {type(byte_chunk)}")

        int_value = int.from_bytes(b''.join(byte_chunk), byteorder='big')
        packet_list.append(int_value)

    print(packet_list)
    print("test_crc: Packet CRC Provided: ", packet.crc)
    calculated_crc = crc32_stm32_32bit(packet_list)
    print("test_crc: Calculated CRC in server: ", calculated_crc)
    
    return packet_list

def os_print(ser: serial.Serial):
    if ser.in_waiting() > 1:
        data = ser.read(ser.in_waiting())
        print(data.decode(ENCODER))

# Example usage
data = [0x1, 2, 3, 4]  # Example 32-bit data items
crc_value = crc32_stm32_32bit(data)
print(f"CRC32: {crc_value:#010x}")
print(f"CRC32: {crc_value}")
print(f"CRC32 without xor : {crc_value^0xFFFFFFFF}")


read_file_in_chunks(OS_FILENAME)
print_chunks()
# print("First file chunk: ",  file_chunks[0])

try:
    with serial.Serial(SERIAL_PORT, BAUD_RATE) as ser:
        ser.reset_input_buffer()
        ser.reset_output_buffer()

        # time.sleep(5)
        print(f"Listening for data on {SERIAL_PORT} at {BAUD_RATE} baud.")
        # test_write(ser)
        while True:
            # if last_sent >= len(file_chunks) - 1:
            #     os_print(ser)
            # else:
            #     byte_array = receive_packet_bytes(ser)
            #     packet = bytearray_to_packet(byte_array=byte_array)
            #     # print(f" received packet : {packet}")
            #     # test_crc(packet)
            #     operate(ser, packet)
            
            byte_array = receive_packet_bytes(ser)
            packet = bytearray_to_packet(byte_array=byte_array)
            # print(f" received packet : {packet}")
            # test_crc(packet)
            operate(ser, packet)



except serial.SerialException as e:
    print(f"Error opening serial port: {e}")
except KeyboardInterrupt:
    print("Exiting program.")