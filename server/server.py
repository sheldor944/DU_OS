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
OS_FILENAME = 'duos'

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
            data_bytes[i] = self.data[i]
        
        crc_bytes = self.crc.to_bytes(4, byteorder='big')
        
        packet_bytes = command_bytes + length_bytes + bytes(data_bytes) + crc_bytes
        return packet_bytes

current_state = State.INITIALIZATION

'''
    Packet Protocol Command Types Meaning:
    cmd=0: DEBUG
    cmd=1: INITIALIZATION
    cmd=2: SEND, the client will provide some ack
'''

def bytearray_to_packet(byte_array: bytearray):
    if len(byte_array) != PACKET_MAX_LENGTH:
        raise ValueError(f"Invalid packet size, expected {PACKET_MAX_LENGTH} bytes, got {len(byte_array)} bytes")

    # Extract command, length, data, and CRC from the byte_array
    command = byte_array[0]  # First byte is the command
    length = byte_array[1]   # Second byte is the length
    data_integers = byte_array[2:2+PACKET_DATA_MAX_LENGTH]  # Data starts at index 2 and has a length of 'length'
    data = [bytes([b]) for b in data_integers]
    crc = byte_array[2+PACKET_DATA_MAX_LENGTH:]  # CRC is the last 4 bytes after the data
    
    # Initialize the Packet object
    packet = Packet(command=command, length=length, data=data, crc=crc)
    
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
    print("DEBUGGING START:")
    length = packet.length

    byte_array = bytearray()
    for i in range(PACKET_DATA_MAX_LENGTH):
        byte = packet.data[i]
        if i >= length:
            continue
        byte_array.extend(byte)
    data = bytes(byte_array)
    print(data.decode(ENCODER))

    print("DEBUGGING END")


def handle_initialization(ser: serial.Serial, packet: Packet):
    print("INITIALIZING PROTOCOL.")
    size = file_size(OS_FILENAME)
    print(f"File size: {size}")

    total_number_of_packets = math.ceil(size / PACKET_DATA_MAX_LENGTH)
    
    bytes_32_bit = total_number_of_packets.to_bytes(4, byteorder='big')
    bytes_96_bit = bytes(12)
    combined_bytes = bytes_32_bit + bytes_96_bit

    crc = calculate_crc(packet)
    packet = Packet(command=1, length=32, data=combined_bytes, crc=crc)
    
    packet_bytes = packet.to_bytes()
    ser.write(packet_bytes)

# def handle_send(ser: serial.Serial):
#     pass

def receive_packet_bytes(ser: serial.Serial):
    byte_array = bytearray()
    cnt = 0
    
    while ser.in_waiting < PACKET_MAX_LENGTH:
        continue
    print("Serial size: ", ser.in_waiting)
    byte = ser.read(PACKET_MAX_LENGTH)
    byte_array.extend(byte)
    print("byte_array size: ", len(byte_array))
    return byte_array

def operate(ser: serial.Serial, packet: Packet):
    cmd = packet.command
    if cmd == 0:
        handle_debug(packet)
    if cmd == 1:
        handle_initialization(ser, packet)
    # implement remaining

def test_write(ser: serial.Serial):
    data = b'\x41'
    ser.write(data)
    ser.flush()      # Wait until all data is sent
    time.sleep(1)


try:
    with serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=10) as ser:
        # time.sleep(5)
        print(f"Listening for data on {SERIAL_PORT} at {BAUD_RATE} baud.")
        # test_write(ser)
        while True:
            print("initiating packet receive\n")
            byte_array = receive_packet_bytes(ser)
            print("got a new command")
            packet = bytearray_to_packet(byte_array=byte_array)
            operate(ser, packet)



except serial.SerialException as e:
    print(f"Error opening serial port: {e}")
except KeyboardInterrupt:
    print("Exiting program.")
