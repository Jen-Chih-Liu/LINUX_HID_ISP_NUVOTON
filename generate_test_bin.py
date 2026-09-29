import sys
import os

def generate_bin(output_path, size_kb, base_bin=None, fill_byte=0xFF):
    target_bytes = size_kb * 1024
    data = bytearray()

    if base_bin and os.path.exists(base_bin):
        with open(base_bin, 'rb') as f:
            base_data = f.read()
            data.extend(base_data[:target_bytes])

    if len(data) < target_bytes:
        data.extend(bytes([fill_byte]) * (target_bytes - len(data)))

    with open(output_path, 'wb') as f:
        f.write(data)

    print(f"Generated '{output_path}' ({len(data)} bytes, {size_kb} KB)")

if __name__ == '__main__':
    size_kb = int(sys.argv[1]) if len(sys.argv) > 1 else 256
    out_file = sys.argv[2] if len(sys.argv) > 2 else f"test_{size_kb}k.bin"
    base_file = "LINUX_HID_ISP_NUVOTON/linux_usb_isp_nuvoton/Debug/test.bin"
    if not os.path.exists(base_file):
        base_file = None

    generate_bin(out_file, size_kb, base_bin=base_file)
