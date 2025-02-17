import os
import struct
import sys

# Define package format
PACKAGE_HEADER = b"ESP_UPDATE"  # 10-byte magic header
HEADER_FORMAT = "<IIII"  # Four little-endian integers: firmware size, spiffs size, firmware offset, spiffs offset
HEADER_SIZE = struct.calcsize(HEADER_FORMAT)

def create_package(firmware_path, spiffs_folder, output_file):
    """ Creates a single .pkg update file containing firmware and spiffs files """
    
    # Read firmware
    with open(firmware_path, "rb") as f:
        firmware_data = f.read()

    # Collect SPIFFS files
    spiffs_files = []
    for root, _, files in os.walk(spiffs_folder):
        for file in files:
            file_path = os.path.join(root, file)
            with open(file_path, "rb") as f:
                file_data = f.read()
            relative_path = os.path.relpath(file_path, spiffs_folder)
            
            # Encode file name
            file_name_encoded = relative_path.encode("utf-8")

            # Print metadata before writing
            print(f"Adding file: {relative_path}")
            print(f"   - File Name Length: {len(file_name_encoded)} bytes")
            print(f"   - File Size: {len(file_data)} bytes")

            spiffs_files.append((file_name_encoded, file_data))

    # Serialize SPIFFS file structure
    spiffs_data = b""
    for file_name, file_content in spiffs_files:
        spiffs_data += struct.pack("<H", len(file_name))  # File name length (2 bytes, little-endian)
        spiffs_data += struct.pack("<I", len(file_content))  # File size (4 bytes, little-endian)
        spiffs_data += file_name  # File name bytes
        spiffs_data += file_content  # File data

    # Create package header
    firmware_offset = len(PACKAGE_HEADER) + HEADER_SIZE  # Firmware starts after the header
    spiffs_offset = firmware_offset + len(firmware_data)  # SPIFFS starts after firmware

    package_header = struct.pack("<IIII", len(firmware_data), len(spiffs_data), firmware_offset, spiffs_offset)

    # Write the final package
    with open(output_file, "wb") as f:
        f.write(PACKAGE_HEADER)
        f.write(package_header)
        f.write(firmware_data)
        f.write(spiffs_data)

    print(f"\n Package '{output_file}' created successfully!")
    print(f"   - Firmware Size: {len(firmware_data)} bytes")
    print(f"   - SPIFFS Size: {len(spiffs_data)} bytes")
    print(f"   - Firmware Offset: {firmware_offset}")
    print(f"   - SPIFFS Offset: {spiffs_offset}")

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("Usage: python create_update_package.py <firmware.bin> <spiffs_folder> <output.pkg>")
        sys.exit(1)

    firmware_bin = sys.argv[1]
    spiffs_folder = sys.argv[2]
    output_package = sys.argv[3]

    create_package(firmware_bin, spiffs_folder, output_package)