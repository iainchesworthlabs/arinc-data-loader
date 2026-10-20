#!/usr/bin/env python3
"""Re-saves PNG screenshots with an adaptive 256-colour palette (no dithering).

The windows are flat colours with anti-aliased text, so a palette of 256 colours keeps them
sharp while making the files about a third of the size. Needs Pillow (`pip install pillow`).

    python tools/docs/optimise_png.py docs/images/*.png
"""
import io
import sys
from pathlib import Path

from PIL import Image


def optimise(path: Path) -> tuple[int, int]:
    before = path.stat().st_size
    image = Image.open(path).convert("RGB")
    palette = image.quantize(colors=256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    buffer = io.BytesIO()
    palette.save(buffer, "PNG", optimize=True)
    data = buffer.getvalue()
    if len(data) < before:
        path.write_bytes(data)
        return before, len(data)
    return before, before


def main(arguments: list[str]) -> int:
    total_before = total_after = 0
    for name in arguments:
        before, after = optimise(Path(name))
        total_before += before
        total_after += after
    print(f"{len(arguments)} pictures, {total_before // 1024} KB -> {total_after // 1024} KB")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
