#!/usr/bin/env python3
#
# SPDX-FileCopyrightText: 2023, Inseo Oh (dhdlstjtr@gmail.com)
#
# SPDX-License-Identifier: BSD-2-Clause
import re
import sys

CODEPOINT_MIN = ord(' ')
CODEPOINT_MAX = ord('~')
GLYPH_COUNT = CODEPOINT_MAX - CODEPOINT_MIN + 1

if len(sys.argv) < 3:
        sys.stderr.write('Usage: fontgen.py <hex font file> <output file>\n')
        sys.exit(1)

out_decl_lines = []
for n in range(0, 65536):
        out_decl_lines.append('{ .is_wide = false, .data = NULL },')

glyphs = []
for n in range(0, GLYPH_COUNT):
        glyphs.append([0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0])

with open(sys.argv[1]) as in_file:
        for l in in_file:
                matches =re.match('([A-Z0-9]{4}):([A-Z0-9]{2,})', l)
                codepoint = int(matches[1], 16)
                if codepoint > CODEPOINT_MAX:
                        break
                if codepoint < CODEPOINT_MIN:
                        continue
                raw_data = matches[2]
                data_len = 0
                out_data_line = f'static const uint8_t GLYPH_{codepoint:X}[] = {{'
                for n in range(0, len(raw_data), 2):
                        b = int(raw_data[n:n + 2], 16)
                        out_data_line += f'0x{b:02X}, '
                        data_len += 1
                        if data_len <= len(glyphs[0]):
                                glyphs[codepoint - CODEPOINT_MIN][n // 2] = b
                out_data_line += '};'
                is_wide = data_len == 32
                if is_wide:
                        sys.stderr.write('WARNING: Wide characters are NOT supported\n')
                out_decl_lines[codepoint] = f"{{ .is_wide = {'true' if is_wide else 'false'}, .data = GLYPH_{codepoint:X} }},"

with open(sys.argv[2], 'wb') as out_file:
        bcnt = 0
        for glyph in glyphs:
                assert len(glyph) == 16
                for b in glyph:
                        out_file.write(bytes([b]))

