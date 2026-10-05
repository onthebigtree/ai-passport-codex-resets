#!/usr/bin/env python3
from pathlib import Path
import json,re
root=Path(__file__).resolve().parent.parent
inventory=set(json.loads((root/'assets/fonts/reset_glyphs.json').read_text())['codepoints'])
required=set(range(32,127))
for name in ('main.c','reset_ui.c','reset_model.c','reset_network.c'):
    for literal in re.findall(r'"(?:\\.|[^"\\])*"',(root/'main'/name).read_text()):
        required.update(ord(c) for c in literal if ord(c)>=128)
assert required<=inventory, f'Font inventory missing: {required-inventory}'
s=(root/'assets/fonts/reset_font_14.c').read_text()
# Converter glyph comments independently attest generated glyphs (not just input inventory).
generated={int(x,16) for x in re.findall(r'U\+([0-9A-Fa-f]+)',s)}
assert required<=generated, f'Generated font missing: {required-generated}'
assert 0x9f98 not in generated, 'Negative coverage probe unexpectedly exists'
print(f'Font coverage: PASS ({len(required)} required glyphs, negative U+9F98)')
