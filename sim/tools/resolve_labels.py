#!/usr/bin/env python3
"""Replace labels in an isaSim assembly file with concrete instruction addresses.

The project handout asks that submitted benchmarks use concrete addresses instead of
labels. Write benchmarks with labels, then:

    python3 resolve_labels.py labeled/foo.asm > foo.asm

Label definitions become comments (`# LOOP: (0x000A)`) and every operand that names a
label becomes a hex address. Numeric labels (`0x3A:`) set the origin like the assembler.
"""
import re
import sys

LABEL_DEF = re.compile(r'^\s*([A-Za-z_][A-Za-z0-9_]*|0[xX][0-9A-Fa-f]+)\s*:')


def split_comment(line):
    m = re.search(r'[#;]', line)
    return (line[:m.start()], line[m.start():]) if m else (line, '')


def main(path):
    src = open(path).read().splitlines()
    labels, addr = {}, 0
    parsed = []  # (defs, code, comment, address)
    for line in src:
        code, comment = split_comment(line)
        defs = []
        while True:
            m = LABEL_DEF.match(code)
            if not m:
                break
            name = m.group(1)
            if re.fullmatch(r'0[xX][0-9A-Fa-f]+', name):
                addr = int(name, 16)
            else:
                labels[name] = addr
            defs.append(name)
            code = code[m.end():]
        has_instr = bool(code.strip())
        parsed.append((defs, code, comment, addr))
        if has_instr:
            addr += 1

    out = []
    for defs, code, comment, a in parsed:
        for d in defs:
            if not re.fullmatch(r'0[xX][0-9A-Fa-f]+', d):
                out.append(f"# {d}: (0x{labels[d]:04X})")
        if code.strip():
            parts = code.strip().split(None, 1)
            ops = parts[1] if len(parts) > 1 else ''
            for name in sorted(labels, key=len, reverse=True):
                ops = re.sub(rf'(?<![\w]){re.escape(name)}(?![\w])', f"0x{labels[name]:04X}", ops)
            text = f"{parts[0]} {ops}".rstrip()
            out.append(f"{text:<28}{comment}".rstrip())
        elif comment and not defs:
            out.append(comment)
        elif comment:
            out.append(comment)
        elif not defs:
            out.append('')
    print("\n".join(out))


if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
