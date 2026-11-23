"""
Ubah video Bad Apple!! jadi data frame untuk aplikasi badapple di OSjur.

Pemakaian: python3 tools/badapple-pack.py <video.mp4> <folder output>
Butuh ffmpeg di PATH. Hasil: file ba00, ba01, ... (masing-masing 280 frame).

Format: tiap frame 80x24 piksel hitam/putih, 1 bit per piksel (MSB duluan),
jadi 240 byte per frame. Satu file 280 frame = 67200 byte, masih di bawah
batas ukuran file EXT2 kita ((12 + 128) x 512 = 71680 byte).
"""
import os
import subprocess
import sys

WIDTH, HEIGHT = 80, 24
FPS = 20                # harus sama dengan FPS di src/user-badapple.c
FRAMES_PER_CHUNK = 280  # harus sama dengan FRAMES_PER_CHUNK di src/user-badapple.c
THRESHOLD = 128

if len(sys.argv) != 3:
    print("pemakaian: python3 tools/badapple-pack.py <video> <folder output>")
    sys.exit(1)

video, out_dir = sys.argv[1], sys.argv[2]
os.makedirs(out_dir, exist_ok=True)

# ffmpeg yang mengurus resize dan fps, keluarannya piksel grayscale mentah 1 byte per piksel
raw = subprocess.run(
    ["ffmpeg", "-loglevel", "error", "-i", video,
     "-vf", f"fps={FPS},scale={WIDTH}:{HEIGHT}",
     "-f", "rawvideo", "-pix_fmt", "gray", "-"],
    check=True, stdout=subprocess.PIPE).stdout

frame_size = WIDTH * HEIGHT
frames = []
for start in range(0, len(raw) - frame_size + 1, frame_size):
    pixels = raw[start:start + frame_size]
    packed = bytearray(frame_size // 8)
    for i, value in enumerate(pixels):
        if value >= THRESHOLD:
            packed[i // 8] |= 0x80 >> (i % 8)
    frames.append(bytes(packed))

# Frame terakhir dipenuhi frame hitam supaya semua file berukuran sama
while len(frames) % FRAMES_PER_CHUNK != 0:
    frames.append(bytes(frame_size // 8))

chunk_count = len(frames) // FRAMES_PER_CHUNK
if chunk_count > 100:
    print("video terlalu panjang, nama file cuma sampai ba99")
    sys.exit(1)

for c in range(chunk_count):
    data = b"".join(frames[c * FRAMES_PER_CHUNK:(c + 1) * FRAMES_PER_CHUNK])
    with open(os.path.join(out_dir, f"ba{c:02d}"), "wb") as f:
        f.write(data)

print(f"{len(frames)} frame ({FPS} fps), {chunk_count} file, total {len(frames) * frame_size // 8} byte")
