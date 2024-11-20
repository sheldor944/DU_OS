BOOTLOADER_SIZE = 0x10000 # 64 KB
BOOTLOADER_FILE = "target/duos"

with open(BOOTLOADER_FILE, "rb") as f:
    raw_file = f.read()

bytes_to_pad = BOOTLOADER_SIZE - len(raw_file)
print(f"Bootloader size {len(raw_file)}")

if bytes_to_pad < 0:
   print("Bootloader is too large")
else:
    padding = bytes([0xFF] * bytes_to_pad)
    with open(BOOTLOADER_FILE, "wb") as f:
        f.write(raw_file + padding)




