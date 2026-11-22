"""Validate 8.3 layout, SH ELF applet manifests, native D01 size and packaging."""
from pathlib import Path
import struct
import sys

build = Path(sys.argv[1])
app_id = sys.argv[2] if len(sys.argv) > 2 else "TOYBX"
module = sys.argv[3] if len(sys.argv) > 3 else "toybox"
expected = {
    'base': {'ls','cat','echo','cp','mv','rm','touch','mkdir','rmdir','date','true'},
    'editors': {'vi','nano'},
    'text': {'wc','head','grep','basename','dirname','uniq','sort','tail','more','hexdump'},
    'utils': {'rev','tac','nl','cut','tr','strings','comm','paste','fold','expand','seq','cmp','cksum','stat','du','find','printf','false'}
}
boot = build / f'{module}.d01'
data = boot.read_bytes()
assert data[:16] == b'CASIODICPLUGIN10', 'invalid D01 header'
assert 128 < len(data) <= 131072, f'D01 exceeds 128 KiB ({len(data)} bytes)'
assert len(boot.name) <= 12
print(f'PASS: {boot.name}: {len(data)} / 131072 bytes ({131072-len(data)} free)')

def read_manifest(filename):
    b = filename.read_bytes()
    assert b[:7] == b'\x7fELF\x01\x02\x01', filename
    assert struct.unpack_from('>H',b,18)[0] == 42
    assert struct.unpack_from('>H',b,16)[0] == 2
    shoff = struct.unpack_from('>I',b,32)[0]
    shentsize, count, strings = struct.unpack_from('>HHH',b,46)
    assert shentsize == 40 and 0 < count <= 512 and strings < count
    assert shoff + count*40 <= len(b)
    sections = [b[shoff+i*40:shoff+(i+1)*40] for i in range(count)]
    shnames = sections[strings]
    nameoff, namesize = struct.unpack_from('>II',shnames,16)
    names = b[nameoff:nameoff+namesize]
    assert len(names) == namesize
    matches = []
    for sh in sections:
        off = struct.unpack_from('>I',sh)[0]
        name = names[off:].split(b'\0',1)[0] if off < len(names) else b''
        if name != b'.edl.cmd': continue
        kind, flags, offset, size = (struct.unpack_from('>I',sh,4)[0],
            struct.unpack_from('>I',sh,8)[0],
            struct.unpack_from('>I',sh,16)[0],
            struct.unpack_from('>I',sh,20)[0])
        assert kind == 1 and not flags&2 and size == 12+24*16
        assert offset + size <= len(b)
        blob = b[offset:offset+size]
        magic, version, count = struct.unpack_from('>III',blob)
        assert magic == 0x434D4431 and version == 1 and 0 < count <= 24
        commands = [blob[12+i*16:12+(i+1)*16].split(b'\0')[0].decode('ascii')
                    for i in range(count)]
        assert len(commands) == len(set(commands))
        matches.append(commands)
    assert len(matches) == 1, (filename, len(matches))
    return set(matches[0])

for stem,commands in expected.items():
    path = build / f'{stem}.edl'
    assert len(path.name) <= 12
    actual = read_manifest(path)
    assert actual == commands, (stem, actual, commands)
    print(f'PASS: {stem}.edl: {len(commands)} applets')

for locale in ('ja','cn'):
    folder = build / 'packages' / locale / app_id
    if folder.exists():
        for basename in [boot.name]+[f'{name}.edl' for name in expected]:
            assert (folder/basename).read_bytes() == (build/basename).read_bytes()
        assert (folder/'fileinfo.cji').is_file()
        assert list(folder.glob('*.htm'))
        assert (folder/'COPYING.TXT').read_bytes() == (Path(__file__).resolve().parents[1]/'COPYING.BUSYBOX').read_bytes()
        print(f'PASS: {locale} installation package')
print('PASS: D01 size, ELF manifests, and 8.3 layout')
