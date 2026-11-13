#!/usr/bin/env python3
"""Regenerate the derived seed/KAT fonts in kat/data (WOFF1 wrapper of a committed TrueType font, and a
minimal Type 1 font). kat.pcf and its .gz/.bz2 come from `bdftopcf kat.bdf` (xfonts-utils) plus gzip/bzip2.
usage: make_seeds.py <outdir> <tests/data dir>"""
import pathlib, struct, sys, zlib

out = pathlib.Path(sys.argv[1])
src = pathlib.Path(sys.argv[2])
out.mkdir(parents=True, exist_ok=True)


def woff1(sfnt):
    flavor, n = struct.unpack(">4sH", sfnt[:6])
    tables = []
    for i in range(n):
        tag, csum, off, length = struct.unpack(">4sIII", sfnt[12 + 16 * i:28 + 16 * i])
        data = sfnt[off:off + length]
        comp = zlib.compress(data, 9)
        if len(comp) >= length:
            comp = data
        tables.append((tag, csum, length, comp))
    hdr_len, dir_len = 44, 20 * n
    off = hdr_len + dir_len
    entries, blobs = b"", b""
    for tag, csum, length, comp in tables:
        entries += struct.pack(">4sIIII", tag, off + len(blobs), len(comp), length, csum)
        blobs += comp + b"\0" * (-len(comp) % 4)
    total = hdr_len + dir_len + len(blobs)
    header = struct.pack(">4s4sIHHIHHIIIII", b"wOFF", flavor, total, n, 0, len(sfnt), 1, 0, 0, 0, 0, 0, 0)
    return header + entries + blobs


def encrypt(data, r, prefix=b"xxxx"):
    out = bytearray()
    for b in prefix + data:
        c = b ^ (r >> 8)
        r = ((c + r) * 52845 + 22719) & 0xFFFF
        out.append(c)
    return bytes(out)


def num(v):
    if -107 <= v <= 107:
        return bytes([v + 139])
    if 108 <= v <= 1131:
        v -= 108
        return bytes([247 + (v >> 8), v & 255])
    if -1131 <= v <= -108:
        v = -v - 108
        return bytes([251 + (v >> 8), v & 255])
    return b"\xff" + struct.pack(">i", v)


def charstring(*items):
    ops = {"hsbw": 13, "rmoveto": 21, "rlineto": 5, "closepath": 9, "endchar": 14}
    return b"".join(bytes([ops[i]]) if isinstance(i, str) else num(i) for i in items)


def type1():
    notdef = charstring(0, 500, "hsbw", "endchar")
    glyph_a = charstring(50, 600, "hsbw", 100, 0, "rmoveto", 400, 0, "rlineto", 0, 500, "rlineto",
                         -400, 0, "rlineto", "closepath", "endchar")
    head = (b"%!PS-AdobeFont-1.0: KatType1 1.0\n11 dict begin\n"
            b"/FontInfo 3 dict dup begin /FullName (Kat Type1) def /FamilyName (Kat) def /Weight (Medium) def end readonly def\n"
            b"/FontName /KatType1 def\n/PaintType 0 def\n/FontType 1 def\n/FontMatrix [0.001 0 0 0.001 0 0] readonly def\n"
            b"/Encoding 256 array 0 1 255 {1 index exch /.notdef put} for dup 65 /A put readonly def\n"
            b"/FontBBox {0 0 600 500} readonly def\ncurrentdict end\ncurrentfile eexec\n")
    cs = lambda b: encrypt(b, 4330)
    notdef, glyph_a = cs(notdef), cs(glyph_a)
    priv = (b"dup /Private 8 dict dup begin\n/RD {string currentfile exch readstring pop} executeonly def\n"
            b"/ND {noaccess def} executeonly def\n/NP {noaccess put} executeonly def\n"
            b"/BlueValues [] def\n/MinFeature {16 16} def\n/password 5839 def\n/lenIV 4 def\n"
            b"/Subrs 0 array\nND\n2 index /CharStrings 2 dict dup begin\n"
            + b"/.notdef %d RD " % len(notdef) + notdef + b" ND\n"
            + b"/A %d RD " % len(glyph_a) + glyph_a + b" ND\n"
            b"end\nend\nreadonly put\nnoaccess put\ndup /FontName get exch definefont pop\nmark currentfile closefile\n")
    tail = (b"0" * 64 + b"\n") * 8 + b"cleartomark\n"
    return head + encrypt(priv, 55665) + b"\n" + tail


(out / "varc-static-gvar.woff").write_bytes(woff1((src / "varc-static-gvar.ttf").read_bytes()))
(out / "kat.pfa").write_bytes(type1())
