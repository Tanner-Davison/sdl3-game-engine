import struct, sys

def png_dims(path):
    with open(path, 'rb') as f:
        f.read(8)  # PNG signature
        f.read(4)  # chunk length
        f.read(4)  # chunk type (IHDR)
        w = struct.unpack('>I', f.read(4))[0]
        h = struct.unpack('>I', f.read(4))[0]
        print(f"{path}: {w}x{h}")

for p in sys.argv[1:]:
    png_dims(p)
