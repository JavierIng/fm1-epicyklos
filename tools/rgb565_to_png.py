# SPDX-License-Identifier: GPL-3.0-only
# Screen dumps (RGB565, big-endian as the FM-1 panel takes them) to PNG at 2x (volcados de pantalla a PNG).
import sys, struct
from PIL import Image
for p in sys.argv[1:]:
    d = open(p, 'rb').read()
    px = struct.unpack('>57600H', d)
    im = Image.new('RGB', (240, 240))
    im.putdata([(((v >> 11) & 31) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31) for v in px])
    im.resize((480, 480), Image.NEAREST).save(p.replace('.rgb565', '.png'))
