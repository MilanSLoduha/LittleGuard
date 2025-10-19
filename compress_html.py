#!/usr/bin/env python3
"""
Komprimuje index.html -> camera_index.h
"""
import gzip

# Načítaj HTML súbor
with open('index.html', 'rb') as f:
    html_data = f.read()

# Komprimuj do GZIP
compressed = gzip.compress(html_data, compresslevel=9)

# Konvertuj do C array formátu
hex_values = ','.join(f'0x{b:x}' for b in compressed)

# Vytvor .h súbor
header_content = f"""const uint8_t index_html_gz[] = {{{hex_values}}};
#define index_html_gz_len {len(compressed)}
"""

# Ulož do camera_index.h
with open('src/camera_index.h', 'w') as f:
    f.write(header_content)

print("✅ Komprimované do src/camera_index.h")
print(f"   Originál: {len(html_data)} bytov")
print(f"   GZIP:     {len(compressed)} bytov")
print(f"   Kompresia: {100 - (len(compressed) / len(html_data) * 100):.1f}%")
