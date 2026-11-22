"""Source-only checks that can run without the SH toolchain."""
from pathlib import Path
import re
root = Path(__file__).resolve().parents[1]
modules = {
    'base': {'ls', 'cat', 'echo', 'cp', 'mv', 'rm', 'touch', 'mkdir', 'rmdir', 'date', 'true'},
    'editors': {'vi', 'nano'},
    'text': {'wc', 'head', 'grep', 'basename', 'dirname', 'uniq', 'sort', 'tail', 'more', 'hexdump'},
    'utils': {'rev', 'tac', 'nl', 'cut', 'tr', 'strings', 'comm', 'paste', 'fold', 'expand', 'seq', 'cmp', 'cksum', 'stat', 'du', 'find', 'printf', 'false'},
}
for name, expected in modules.items():
    source = (root/'src'/f'{name}.c').read_text()
    match = re.search(r'\.names\s*=\s*\{([^}]+)\}', source)
    assert match, name
    names = re.findall(r'"([a-z0-9_-]+)"', match.group(1))
    assert set(names) == expected and len(names) == len(expected), (name,names)
    assert len(name+'.edl') <= 12
assert sum(len(cmd)+1 for names in modules.values() for cmd in names) <= 256, 'D01 packed command-name pool overflow'
assert sum(len(names) for names in modules.values()) <= 48, 'D01 route limit exceeded'
assert len('toybox.d01') <= 12
print('PASS: module manifest source names and 8.3 filenames')
