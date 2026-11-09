#!/usr/bin/env python3
"""Align all markdown tables in a file to fixed column widths."""

import re
import sys


def parse_row(line):
    cells = line.strip().split('|')
    return [c.strip() for c in cells[1:-1]]


def is_separator(row):
    return all(re.fullmatch(r':?-+:?', c) for c in row if c)


def align_table(lines):
    rows = [parse_row(l) for l in lines]
    ncols = max(len(r) for r in rows)

    # Pad all rows to same column count
    for r in rows:
        while len(r) < ncols:
            r.append('')

    sep_idx = next((i for i, r in enumerate(rows) if is_separator(r)), None)

    # Column widths based on non-separator rows only
    widths = [
        max(len(r[c]) for i, r in enumerate(rows) if i != sep_idx)
        for c in range(ncols)
    ]

    result = []
    for i, row in enumerate(rows):
        if i == sep_idx:
            result.append('|' + '|'.join('-' * (w + 2) for w in widths) + '|')
        else:
            result.append('|' + '|'.join(f' {row[c].ljust(widths[c])} ' for c in range(ncols)) + '|')
    return result


def align_file(path):
    with open(path) as f:
        lines = f.readlines()

    output = []
    i = 0
    while i < len(lines):
        line = lines[i].rstrip('\n')
        if re.match(r'\s*\|.*\|', line):
            block = []
            while i < len(lines) and re.match(r'\s*\|.*\|', lines[i].rstrip('\n')):
                block.append(lines[i].rstrip('\n'))
                i += 1
            output.extend(align_table(block))
        else:
            output.append(line)
            i += 1

    with open(path, 'w') as f:
        f.write('\n'.join(output) + '\n')
    print(f"Aligned: {path}")


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: mdalign.py <file.md> [file2.md ...]")
        sys.exit(1)
    for path in sys.argv[1:]:
        align_file(path)
