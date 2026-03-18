#!/usr/bin/env python3
"""Generate platform icon files from a 1024x1024 master PNG.

Outputs:
  - <name>.ico  (Windows: 16, 24, 32, 48, 256)
  - <name>.icns (macOS: via iconutil, macOS-only)

Usage:
  python3 tools/gen_icons.py assets/icons/realm.png --output assets/icons/
"""

import argparse
import os
import platform
import shutil
import subprocess
import sys
import tempfile

try:
    from PIL import Image
except ImportError:
    print("ERROR: Pillow is required. Install with: pip install Pillow", file=sys.stderr)
    sys.exit(1)

ICO_SIZES = [16, 24, 32, 48, 256]

ICNS_SIZES = {
    "icon_16x16.png": 16,
    "icon_16x16@2x.png": 32,
    "icon_32x32.png": 32,
    "icon_32x32@2x.png": 64,
    "icon_128x128.png": 128,
    "icon_128x128@2x.png": 256,
    "icon_256x256.png": 256,
    "icon_256x256@2x.png": 512,
    "icon_512x512.png": 512,
    "icon_512x512@2x.png": 1024,
}


def generate_ico(src: Image.Image, output_path: str) -> None:
    sizes = [(s, s) for s in ICO_SIZES]
    src.save(output_path, format="ICO", sizes=sizes)
    print(f"  .ico -> {output_path}")


def generate_icns(src: Image.Image, output_path: str) -> None:
    if platform.system() != "Darwin":
        print("  .icns skipped (iconutil only available on macOS)")
        return

    with tempfile.TemporaryDirectory() as tmpdir:
        iconset = os.path.join(tmpdir, "icon.iconset")
        os.makedirs(iconset)

        for name, size in ICNS_SIZES.items():
            resized = src.resize((size, size), Image.LANCZOS)
            resized.save(os.path.join(iconset, name), format="PNG")

        result = subprocess.run(
            ["iconutil", "-c", "icns", iconset, "-o", output_path],
            capture_output=True, text=True,
        )
        if result.returncode != 0:
            print(f"  .icns FAILED: {result.stderr.strip()}", file=sys.stderr)
            return

    print(f"  .icns -> {output_path}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate platform icons from master PNG")
    parser.add_argument("input", help="Path to 1024x1024 master PNG")
    parser.add_argument("--output", "-o", default=".", help="Output directory")
    parser.add_argument("--name", "-n", default=None, help="Base name for output files (default: input filename)")
    args = parser.parse_args()

    src = Image.open(args.input).convert("RGBA")

    if src.width != src.height:
        print(f"WARNING: Input is not square ({src.width}x{src.height})", file=sys.stderr)
    if src.width < 1024 or src.height < 1024:
        print(f"WARNING: Input is smaller than 1024x1024 ({src.width}x{src.height})", file=sys.stderr)

    os.makedirs(args.output, exist_ok=True)

    name = args.name or os.path.splitext(os.path.basename(args.input))[0]
    ico_path = os.path.join(args.output, f"{name}.ico")
    icns_path = os.path.join(args.output, f"{name}.icns")

    print(f"Generating icons from {args.input} ({src.width}x{src.height}):")
    generate_ico(src, ico_path)
    generate_icns(src, icns_path)
    print("Done.")


if __name__ == "__main__":
    main()
