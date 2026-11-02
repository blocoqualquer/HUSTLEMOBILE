import sys, struct, numpy as np
from PIL import Image
f = open(sys.argv[1], 'rb').read()
assert f[:4] == b'YTX1'
n, pw, _ = struct.unpack_from('<HHH', f, 4)
pal = np.frombuffer(f, np.uint8, 1024, 10).reshape(256, 4)
o = 10 + 1024
for i in range(n):
    h, = struct.unpack_from('<H', f, o); o += 2
    raw = np.frombuffer(f, np.uint8, pw * h, o); o += pw * h
    a = raw.reshape(h // 8, pw // 16, 8, 16).transpose(0, 2, 1, 3).reshape(h, pw)
    img = pal[a]
    bg = np.full(img.shape, 90, np.uint8); bg[..., 3] = 255
    al = img[..., 3:4] / 255.0
    out = (img[..., :3] * al + bg[..., :3] * (1 - al)).astype(np.uint8)
    Image.fromarray(out).save(f'{sys.argv[2]}_{i}.png')
    print('page', i, pw, h)
assert o == len(f), (o, len(f))
