from pathlib import Path

filename = Path("output.bin")

if filename.exists():
    reply = input(f"Filename {filename} already exists, replace it? [Y/n] ").strip().lower()
    if reply == "n":
        raise SystemExit

with filename.open("wb+") as f:
    for j in range(16):
        for i in range(256):
            f.write(bytes([i]) * 16)
