"""Convert the orange image resources under data/images to grayscale (idempotent).
Requires Pillow.  Run again after merging upstream if new colored resources appear.

ICO: palette / BGRA bytes edited in place (the layout is kept), PNG frames re-encoded.
PNG/TIFF: alpha kept.  SVG: hex colors replaced by their luminance.
"""
import io, os, re, struct
from PIL import Image

root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'data', 'images')


def lum(r, g, b):
    return int(round(0.299 * r + 0.587 * g + 0.114 * b))


def gray_image(im):
    # Luminance with the alpha kept (an already gray pixel does not change).
    return im.convert('RGBA').convert('LA').convert('RGBA')


def gray_png_bytes(data):
    out = io.BytesIO()
    gray_image(Image.open(io.BytesIO(data))).save(out, 'PNG')
    return out.getvalue()


def gray_dib(data):
    data = bytearray(data)
    hdr, w, h, planes, bpp, comp, imgsize, _, _, clr_used, _ = struct.unpack_from('<IiiHHIIiiII', data, 0)
    assert comp == 0, comp
    if bpp <= 8:
        n = clr_used or (1 << bpp)
        for i in range(n):
            o = hdr + 4 * i
            b, g, r = data[o], data[o + 1], data[o + 2]
            data[o] = data[o + 1] = data[o + 2] = lum(r, g, b)
    elif bpp == 32:
        for o in range(hdr, hdr + abs(h) // 2 * w * 4, 4):
            b, g, r = data[o], data[o + 1], data[o + 2]
            data[o] = data[o + 1] = data[o + 2] = lum(r, g, b)
    else:
        raise ValueError(bpp)
    return bytes(data)


def gray_ico(path):
    data = open(path, 'rb').read()
    n = struct.unpack_from('<H', data, 4)[0]
    entries = [list(struct.unpack_from('<BBBBHHII', data, 6 + 16 * i)) for i in range(n)]
    blobs = []
    for e in entries:
        raw = data[e[7]:e[7] + e[6]]
        blobs.append(gray_png_bytes(raw) if raw[:4] == b'\x89PNG' else gray_dib(raw))
    out = bytearray(data[:6 + 16 * n])
    off = len(out)
    for i, (e, b) in enumerate(zip(entries, blobs)):
        e[6], e[7] = len(b), off
        struct.pack_into('<BBBBHHII', out, 6 + 16 * i, *e)
        out += b
        off += len(b)
    open(path, 'wb').write(out)


def gray_raster(path):
    im = Image.open(path)
    g = gray_image(im)
    if g.tobytes() == im.convert('RGBA').tobytes():
        return False
    g.save(path)
    return True


def gray_svg(path):
    s = open(path, encoding='utf-8', newline='').read()

    def sub(m):
        h = m.group(1)
        if len(h) == 3:
            h = ''.join(c * 2 for c in h)
        r, g, b = (int(h[i:i + 2], 16) for i in (0, 2, 4))
        if max(r, g, b) - min(r, g, b) == 0:
            return m.group(0)
        v = lum(r, g, b)
        return '#%02X%02X%02X' % (v, v, v)

    s2 = re.sub(r'#([0-9a-fA-F]{6}|[0-9a-fA-F]{3})\b', sub, s)
    if s2 != s:
        open(path, 'w', encoding='utf-8', newline='').write(s2)
        return True
    return False


for d, _, fs in os.walk(root):
    for f in sorted(fs):
        p = os.path.join(d, f)
        ext = f.lower().rsplit('.', 1)[-1]
        if ext == 'ico':
            gray_ico(p)
        elif ext in ('png', 'tiff'):
            if not gray_raster(p):
                continue
        elif ext == 'svg':
            gray_svg(p)
        else:
            continue
        print('converted', os.path.relpath(p, root))
