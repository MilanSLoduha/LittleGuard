#!/usr/bin/env python3
"""
Dekomprimuje camera_index.h -> index.html
"""
import gzip

# Prečítaj komprimovaný súbor
with open('src/camera_index.h', 'r') as f:
    lines = f.readlines()

# Nájdi bajty v hex formáte
hex_line = [l for l in lines if 'index_html_gz[] = {' in l][0]
hex_data = hex_line.split('{')[1].split('}')[0]

# Konvertuj hex -> bytes
bytes_list = [int(x, 16) for x in hex_data.split(',') if x.strip()]
compressed = bytes(bytes_list)

# Dekomprimuj GZIP
decompressed = gzip.decompress(compressed)

# Ulož ako index.html
with open('index.html', 'wb') as f:
    f.write(decompressed)

print("✅ Dekomprimované do index.html")
print(f"   Veľkosť: {len(decompressed)} bytov")
