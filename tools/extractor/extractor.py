#!/usr/bin/env python3
import json
import sys
import os

# Константы ZX Spectrum
ATTR_BASE = 0x1800
SCREEN_WIDTH_CHARS = 32
SCREEN_HEIGHT_CHARS = 24

screen_addrs = [
    0x0000, 0x0100, 0x0200, 0x0300, 0x0400, 0x0500, 0x0600, 0x0700,
    0x0020, 0x0120, 0x0220, 0x0320, 0x0420, 0x0520, 0x0620, 0x0720,
    0x0040, 0x0140, 0x0240, 0x0340, 0x0440, 0x0540, 0x0640, 0x0740,
    0x0060, 0x0160, 0x0260, 0x0360, 0x0460, 0x0560, 0x0660, 0x0760,
    0x0080, 0x0180, 0x0280, 0x0380, 0x0480, 0x0580, 0x0680, 0x0780,
    0x00a0, 0x01a0, 0x02a0, 0x03a0, 0x04a0, 0x05a0, 0x06a0, 0x07a0,
    0x00c0, 0x01c0, 0x02c0, 0x03c0, 0x04c0, 0x05c0, 0x06c0, 0x07c0,
    0x00e0, 0x01e0, 0x02e0, 0x03e0, 0x04e0, 0x05e0, 0x06e0, 0x07e0,
    0x0800, 0x0900, 0x0a00, 0x0b00, 0x0c00, 0x0d00, 0x0e00, 0x0f00,
    0x0820, 0x0920, 0x0a20, 0x0b20, 0x0c20, 0x0d20, 0x0e20, 0x0f20,
    0x0840, 0x0940, 0x0a40, 0x0b40, 0x0c40, 0x0d40, 0x0e40, 0x0f40,
    0x0860, 0x0960, 0x0a60, 0x0b60, 0x0c60, 0x0d60, 0x0e60, 0x0f60,
    0x0880, 0x0980, 0x0a80, 0x0b80, 0x0c80, 0x0d80, 0x0e80, 0x0f80,
    0x08a0, 0x09a0, 0x0aa0, 0x0ba0, 0x0ca0, 0x0da0, 0x0ea0, 0x0fa0,
    0x08c0, 0x09c0, 0x0ac0, 0x0bc0, 0x0cc0, 0x0dc0, 0x0ec0, 0x0fc0,
    0x08e0, 0x09e0, 0x0ae0, 0x0be0, 0x0ce0, 0x0de0, 0x0ee0, 0x0fe0,
    0x1000, 0x1100, 0x1200, 0x1300, 0x1400, 0x1500, 0x1600, 0x1700,
    0x1020, 0x1120, 0x1220, 0x1320, 0x1420, 0x1520, 0x1620, 0x1720,
    0x1040, 0x1140, 0x1240, 0x1340, 0x1440, 0x1540, 0x1640, 0x1740,
    0x1060, 0x1160, 0x1260, 0x1360, 0x1460, 0x1560, 0x1660, 0x1760,
    0x1080, 0x1180, 0x1280, 0x1380, 0x1480, 0x1580, 0x1680, 0x1780,
    0x10a0, 0x11a0, 0x12a0, 0x13a0, 0x14a0, 0x15a0, 0x16a0, 0x17a0,
    0x10c0, 0x11c0, 0x12c0, 0x13c0, 0x14c0, 0x15c0, 0x16c0, 0x17c0,
    0x10e0, 0x11e0, 0x12e0, 0x13e0, 0x14e0, 0x15e0, 0x16e0, 0x17e0
]

def pixel_addr(x, y):
    if not (0 <= x < SCREEN_WIDTH_CHARS and 0 <= y < SCREEN_HEIGHT_CHARS * 8):
        raise ValueError(f"Invalid char coords: ({x}, {y})")
    return screen_addrs[y] + x

def attr_addr(x_char, y_char):
    return ATTR_BASE + y_char * SCREEN_WIDTH_CHARS + x_char

def extract_sprite_pixels(mem, x_char, y_char, w_chars, h_chars):
    """Извлекает пиксельные данные спрайта как байты."""
    data = bytearray()
    for dy in range(y_char * 8, (y_char + h_chars) * 8):
        for dx in range(x_char, x_char + w_chars):
            addr = pixel_addr(dx, dy)
            # Каждое знакоместо — 8 байт (по одному на строку)
            data.append(mem[addr])
    return bytes(data)

def extract_sprite_attrs(mem, x_char, y_char, w_chars, h_chars):
    """Извлекает атрибуты (цвета) как байты, по одному на знакоместо."""
    data = bytearray()
    for dy in range(y_char, y_char + h_chars):
        for dx in range(x_char, x_char + w_chars):
            addr = attr_addr(dx, dy)
            data.append(mem[addr])
    return bytes(data)

def c_array_literal(data, name, const=True):
    prefix = "const " if const else ""
    lines = []
    lines.append(f"{prefix}unsigned char {name}[] = {{")
    # Разбиваем по 16 байт в строку
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        hex_str = ", ".join(f"0x{b:02X}" for b in chunk)
        lines.append(f"    {hex_str},")
    # Убираем последнюю запятую
    if lines[-1].endswith(","):
        lines[-1] = lines[-1][:-1]
    lines.append("};")
    return "\n".join(lines)

def generate_header(sprites_info, output_h, include_attributes):
    with open(output_h, "w") as f:
        f.write("#ifndef SPRITES_H\n")
        f.write("#define SPRITES_H\n\n")
        f.write("#include <stddef.h>\n\n")

        f.write("typedef struct {\n")
        f.write("    const unsigned char *p_sprite;\n")
        f.write("    const unsigned char *p_attributes;\n")
        f.write("    unsigned char width;\n")
        f.write("    unsigned char height;\n")
        f.write("} t_sprite;\n\n")

        all_decls = []
        for info in sprites_info:
            name = info["name"]
            pixels = info["pixels"]
            attrs = info["attrs"]
            w = info["width"]
            h = info["height"]

            # Генерируем массивы
            pixel_preamble = f"/*{{w:{w*8},h:{h*8},bpp:1,brev:1}}*/"
            pixel_lines = c_array_literal(pixels, f"{name}_sprite_data").split('\n')
            pixel_lines[0] = pixel_lines[0][:-1] + pixel_preamble  # заменяем "{" на "{/*...*/"
            pixel_lines[0] += "{"
            f.write('\n'.join(pixel_lines) + "\n\n")

            if include_attributes:
                f.write(c_array_literal(attrs, f"{name}_attr_data") + "\n\n")

            # Объявление структуры
            f.write(f"static const t_sprite {name} = {{\n")
            f.write(f"    .p_sprite = {name}_sprite_data,\n")
            if include_attributes:
                f.write(f"    .p_attributes = {name}_attr_data,\n")
            else:
                f.write("    .p_attributes = NULL,\n")
            f.write(f"    .width = {w},\n")
            f.write(f"    .height = {h}\n")
            f.write("};\n\n")

            all_decls.append(name)

        # Опционально: массив всех спрайтов
        if all_decls:
            f.write("static const t_sprite * const all_sprites[] = {\n")
            for name in all_decls:
                f.write(f"    &{name},\n")
            f.write("};\n\n")
            f.write(f"static const size_t num_sprites = {len(all_decls)};\n\n")

        f.write("#endif // SPRITES_H\n")

def main():
    if len(sys.argv) != 3:
        print("Usage: python3 zx_sprite_extractor.py <manifest.json> <output.h>")
        sys.exit(1)

    manifest_path = sys.argv[1]
    output_h = sys.argv[2]

    with open(manifest_path, "r") as mf:
        manifest = json.load(mf)

    mem_file = manifest["memory_dump"]
    if not os.path.exists(mem_file):
        print(f"Error: memory dump file '{mem_file}' not found.")
        sys.exit(1)

    # Новый флаг: включать ли атрибуты (по умолчанию — да)
    include_attributes = manifest.get("include_attributes", True)

    with open(mem_file, "rb") as f:
        mem = bytearray(f.read())

    sprites_info = []

    for spr in manifest["sprites"]:
        name = spr["name"]
        x = spr["x_char"]
        y = spr["y_char"]
        w = spr["width_chars"]
        h = spr["height_chars"]

        if x + w > SCREEN_WIDTH_CHARS or y + h > SCREEN_HEIGHT_CHARS:
            print(f"Warning: sprite '{name}' exceeds screen bounds.")

        pixels = extract_sprite_pixels(mem, x, y, w, h)
        attrs = extract_sprite_attrs(mem, x, y, w, h)

        sprites_info.append({
            "name": name,
            "pixels": pixels,
            "attrs": attrs,
            "width": w,
            "height": h
        })

    generate_header(sprites_info, output_h, include_attributes)
    attr_state = "with" if include_attributes else "without"
    print(f"✅ Generated {output_h} {attr_state} attributes ({len(sprites_info)} sprites).")

if __name__ == "__main__":
    main()
    