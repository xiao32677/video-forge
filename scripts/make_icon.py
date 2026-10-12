#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
VideoForge 图标生成脚本
------------------------------------------------------------
用 Pillow 画出应用图标（圆角渐变底 + 播放三角 + 转换箭头环），
并导出 assets/videoforge.ico（多尺寸）与 assets/logo.png。

设计意图：一眼看出"视频 + 转换"，且在小尺寸（16/32）下依然清晰。
重新生成： python scripts/make_icon.py
"""

from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent
ASSETS = ROOT / "assets"
ASSETS.mkdir(parents=True, exist_ok=True)

SIZE = 1024
RADIUS = int(SIZE * 0.22)

TOP = (0x5B, 0x9B, 0xFF)
BOTTOM = (0x28, 0x53, 0xC8)


def vertical_gradient(size: int) -> Image.Image:
    image = Image.new("RGB", (size, size))
    draw = ImageDraw.Draw(image)
    for y in range(size):
        ratio = y / max(1, size - 1)
        color = tuple(int(TOP[i] + (BOTTOM[i] - TOP[i]) * ratio) for i in range(3))
        draw.line([(0, y), (size, y)], fill=color)
    return image


def rounded_mask(size: int, radius: int) -> Image.Image:
    mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, size - 1, size - 1], radius=radius, fill=255)
    return mask


def draw_arrow_ring(draw: ImageDraw.ImageDraw, size: int) -> None:
    """画一个带箭头的圆弧，表达"转换/循环"。"""
    margin = int(size * 0.17)
    width = int(size * 0.075)
    box = [margin, margin, size - margin, size - margin]
    # 缺口在右上，箭头收口
    draw.arc(box, start=-58, end=250, fill=(255, 255, 255, 235), width=width)

    # 箭头三角
    import math

    cx = cy = size / 2
    radius = (size - 2 * margin) / 2
    angle = math.radians(-58)
    ax = cx + radius * math.cos(angle)
    ay = cy + radius * math.sin(angle)
    head = size * 0.085
    draw.polygon(
        [
            (ax + head * 0.9, ay - head * 0.25),
            (ax - head * 0.55, ay - head * 0.95),
            (ax - head * 0.15, ay + head * 0.75),
        ],
        fill=(255, 255, 255, 245),
    )


def draw_play(draw: ImageDraw.ImageDraw, size: int) -> None:
    """中间实心播放三角。"""
    left = size * 0.375
    right = size * 0.685
    top = size * 0.325
    bottom = size * 0.675
    draw.polygon(
        [(left, top), (right, (top + bottom) / 2), (left, bottom)],
        fill=(255, 255, 255, 255),
    )


def build_base() -> Image.Image:
    background = vertical_gradient(SIZE).convert("RGBA")
    background.putalpha(rounded_mask(SIZE, RADIUS))

    overlay = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)
    draw_arrow_ring(draw, SIZE)
    draw_play(draw, SIZE)
    return Image.alpha_composite(background, overlay)


def main() -> None:
    base = build_base()
    base.save(ASSETS / "logo.png")

    sizes = [16, 24, 32, 48, 64, 128, 256]
    frames = [base.resize((s, s), Image.LANCZOS).convert("RGBA") for s in sizes]
    frames[-1].save(ASSETS / "videoforge.ico", format="ICO",
                    sizes=[(s, s) for s in sizes], append_images=frames[:-1])

    print(f"已生成: {ASSETS / 'videoforge.ico'}")
    print(f"已生成: {ASSETS / 'logo.png'}")


if __name__ == "__main__":
    main()
