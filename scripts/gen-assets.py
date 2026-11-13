#!/usr/bin/env python3
import argparse
import os
import struct
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

SOURCE_FILLS = ["4F5B58", "E67E80", "7FBBB3", "DBBC7F"]


def parse_hex(value):
    value = value.strip().lstrip("#")
    if len(value) != 6:
        raise SystemExit(f"invalid color: {value}")
    return tuple(int(value[i : i + 2], 16) for i in (0, 2, 4))


def parse_fills(value):
    if not value:
        return []
    return [parse_hex(part) for part in value.split(",") if part.strip()]


def recolor(image, source_fills, target_fills):
    if not source_fills or not target_fills:
        return image
    if len(source_fills) != len(target_fills):
        raise SystemExit("source and target fill counts differ")

    fills = np.array(source_fills, dtype=np.float64)
    targets = np.array(target_fills, dtype=np.float64)
    rgba = np.asarray(image.convert("RGBA"), dtype=np.uint8)
    rgb = rgba[..., :3]
    alpha = rgba[..., 3]
    flat = rgb.reshape(-1, 3).astype(np.uint32)
    keys = (flat[:, 0] << 16) | (flat[:, 1] << 8) | flat[:, 2]
    unique, inverse = np.unique(keys, return_inverse=True)
    unique_rgb = np.stack(
        [(unique >> 16) & 255, (unique >> 8) & 255, unique & 255], axis=1
    ).astype(np.float64)

    out_colors = np.empty((len(unique), 3), dtype=np.uint8)
    for index, color in enumerate(unique_rgb):
        exact = np.where(np.all(fills == color, axis=1))[0]
        if len(exact):
            out_colors[index] = np.round(targets[exact[0]]).astype(np.uint8)
            continue

        best = None
        for first in range(len(fills)):
            for second in range(first + 1, len(fills)):
                start = fills[first]
                direction = fills[second] - start
                amount = np.clip(
                    ((color - start) @ direction) / (direction @ direction), 0.0, 1.0
                )
                distance = float(np.sum((color - (start + amount * direction)) ** 2))
                if best is None or distance < best[0]:
                    best = (distance, first, second, amount)

        _, first, second, amount = best
        out_colors[index] = np.round(
            (1.0 - amount) * targets[first] + amount * targets[second]
        ).astype(np.uint8)

    output = out_colors[inverse].reshape(rgb.shape)
    output = np.dstack([output, alpha])
    return Image.fromarray(output)


def render_text(text, font_path, font_size, color):
    font = ImageFont.truetype(font_path, font_size)
    left, top, right, bottom = font.getbbox(text)
    width = max(1, right - left)
    height = max(1, bottom - top)
    image = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.text((-left, -top), text, font=font, fill=(*color, 255))
    return image


def read_tips(path):
    if not path:
        return []
    tips = []
    with open(path, encoding="utf-8") as handle:
        for line in handle:
            tip = line.strip()
            if tip and not tip.startswith("#"):
                tips.append(tip)
    return tips


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--logo", required=True)
    parser.add_argument("--out-dir", required=True)
    parser.add_argument("--background", default="#242424")
    parser.add_argument("--foreground", default="#d5c4a1")
    parser.add_argument("--source-fills", default=",".join(SOURCE_FILLS))
    parser.add_argument("--target-fills", default=",".join(SOURCE_FILLS))
    parser.add_argument("--font", required=True)
    parser.add_argument("--font-size", type=int, default=32)
    parser.add_argument("--text", default="Welcome to Ojuny")
    parser.add_argument("--border-percent", type=float, default=22.0)
    parser.add_argument("--text-gap", type=int, default=32)
    parser.add_argument("--tips-file", default="")
    parser.add_argument("--tip-foreground", default="#a89984")
    parser.add_argument("--tip-font-size", type=int, default=18)
    parser.add_argument("--tip-gap", type=int, default=10)
    args = parser.parse_args()

    if args.font_size <= 0:
        raise SystemExit("font size must be positive")
    if args.text_gap < 0:
        raise SystemExit("text gap must not be negative")
    if args.tip_font_size <= 0:
        raise SystemExit("tip font size must be positive")
    if args.tip_gap < 0:
        raise SystemExit("tip gap must not be negative")

    background = parse_hex(args.background)
    foreground = parse_hex(args.foreground)
    tip_foreground = parse_hex(args.tip_foreground)

    with Image.open(args.logo) as source:
        logo = source.convert("RGBA")
    logo = recolor(
        logo,
        parse_fills(args.source_fills),
        parse_fills(args.target_fills),
    )

    content = logo.width
    border = round(content * args.border_percent / 100.0)
    canvas = Image.new(
        "RGBA", (logo.width + 2 * border, logo.height + 2 * border), (0, 0, 0, 0)
    )
    canvas.paste(logo, (border, border), logo)
    logo = canvas

    text = render_text(args.text, args.font, args.font_size, foreground)
    tips = [
        render_text(tip, args.font, args.tip_font_size, tip_foreground)
        for tip in read_tips(args.tips_file)
    ]

    os.makedirs(args.out_dir, exist_ok=True)
    bg_rgb = (background[0] << 16) | (background[1] << 8) | background[2]
    meta = struct.pack(
        "<8I",
        1,
        bg_rgb,
        logo.width,
        logo.height,
        content,
        text.width,
        text.height,
        args.text_gap,
    )
    with open(os.path.join(args.out_dir, "meta"), "wb") as handle:
        handle.write(meta)
    with open(os.path.join(args.out_dir, "logo.rgba"), "wb") as handle:
        handle.write(logo.tobytes())
    with open(os.path.join(args.out_dir, "text.rgba"), "wb") as handle:
        handle.write(text.tobytes())

    with open(os.path.join(args.out_dir, "tips.meta"), "wb") as handle:
        handle.write(struct.pack("<3I", 1, len(tips), args.tip_gap))
        for tip in tips:
            handle.write(struct.pack("<2I", tip.width, tip.height))
    for index, tip in enumerate(tips):
        with open(os.path.join(args.out_dir, f"tip-{index}.rgba"), "wb") as handle:
            handle.write(tip.tobytes())

    print(
        f"logo {logo.width}x{logo.height} content {content} "
        f"text {text.width}x{text.height} tips {len(tips)} bg #{bg_rgb:06x}",
        file=sys.stderr,
    )


if __name__ == "__main__":
    main()
