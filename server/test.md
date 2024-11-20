### Detailed Documentation for Server and Client Files

#### Overview
The files provided comprise a system for managing firmware updates and interactions between a server and a client microcontroller (STM32). The system uses serial communication to exchange data packets that include commands for operations such as debugging, initialization, data sending, and version checking. Here's how to use and understand these files:

---

### 1. How to Use the System

#### Server Setup
- Ensure Python3 is installed on your system.
- Connect the microcontroller to your computer using the appropriate serial connection, such as a USB to UART converter. We are using UART2, which is connected to the USB through the ST-LINK debugger.
- Before running the server, set the current OS version in the server from the `version.txt` file. If an updated version of the OS is available, place the binary file of the updated OS in the `os_files` directory. Additionally, update the `version_names.json` file so that it can locate the corresponding file name from the version major and version minor.
- Run the Python script (`server.py`). The script will automatically handle incoming packets from the microcontroller and respond based on the command type and data contained within these packets.

#### Client (Microcontroller) Setup
- The bootloader code should be compiled and flashed onto an STM32 microcontroller.
- The microcontroller should be configured to match the communication parameters specified in the Python script (e.g., baud rate, serial port).
- The microcontroller will initiate communications based on its firmware logic, responding to commands from the server or sending requests to the server.

---

### 2. Packet Protocol Details

#### Packet Structure
- **Command Field (1 byte)**: Determines the type of action to be performed.
- **Length Field (1 byte)**: Indicates the length of the data segment in the packet.
- **Data Field (up to 128 bytes)**: Contains the data to be processed or transmitted.
- **CRC Field (4 bytes)**: Contains a CRC checksum for error checking.

#### Packet Max Length
- The total length of a packet is fixed at 134 bytes.

---
### 3. Commands and Their Functions

- **Command 0 (DEBUG)**: Used for debugging purposes, allowing the server to receive and display debug messages from the client.
- **Command 1 (INITIALIZATION)**: Initializes communication and prepares the system for data transfer. Upon receiving this command, the server will send the number of chunks it will transmit for updating the OS. The server sends the number of chunks as a 32-bit integer, so the corresponding length will always be 32 bytes.
- **Command 2 (SEND)**: Manages the sending of data packets from the server to the client, typically used for firmware updates or command execution. This command sends the corresponding chunk as received from the client as an acknowledgment.
- **Command 3 (VERSION CHECK)**: Used to verify or check the firmware version between the client and server to ensure compatibility or identify the need for updates. This is the first command the client performs. In this command, the length field is 64 bits. The first 4 bytes in the data section represent the current version major, and the following 4 bytes represent the current version minor. Based on version matching, this command will either call 'test_init' or 'jump_to_os'.
- **Command 4 (OS CONSOLE)**: Transforms the server into an operating system console, allowing direct command input and responses from the connected microcontroller. After completing the OS update or version check, the bootloader will jump to the OS. In this mode, there is no protocol for console printing, so this command will disable the packet protocol on the server. Consequently, the `kprintf` function will serve as the console for the OS. 
NOTE: In the bootloader section, we use `UART_READ` with the packet protocol.
- **Command 5 (RETRANSMIT)**: Requests the retransmission of a packet, typically used when a CRC check fails.


### 4. Code Execution Flow

**Server Side**
- The server initializes by setting up serial communication and reading necessary version files.
- It enters a loop where it continuously listens for packets.
- Upon receiving a packet, the server converts it to a packet from (initially it is just a bytes array). Then, checks its integrity using a CRC check.
- Depending on the command in the packet, the server executes different handlers (e.g., send data, initialize communication).
- It can handle errors, retransmit requests, and debug information output.