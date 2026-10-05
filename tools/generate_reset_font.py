#!/usr/bin/env python3
"""Generate the fixed UI glyph inventory and a licensed Noto Sans SC subset."""
from pathlib import Path
import json
import re
import subprocess
ROOT = Path(__file__).resolve().parent.parent
sources = [ROOT / 'main' / name for name in ('main.c','reset_ui.c','reset_model.c','reset_network.c')]
# C string literals only; include punctuation used in formatted status strings.
symbols=set(chr(i) for i in range(32,127))
for path in sources:
    for literal in re.findall(r'"(?:\\.|[^"\\])*"', path.read_text()):
        symbols.update(c for c in literal if ord(c)>=128)
symbols=''.join(sorted(symbols))
font=ROOT/'assets/fonts/NotoSansSC.ttf'
from fontTools.ttLib import TTFont
from fontTools import subset
from fontTools.varLib.instancer import instantiateVariableFont
f=instantiateVariableFont(TTFont(font), {"wght":500})
missing=set(map(ord,symbols))-set(f.getBestCmap())
if missing: raise SystemExit(f'Missing source glyphs: {sorted(missing)}')
options=subset.Options(); options.flavor='woff2'
s=subset.Subsetter(options=options); s.populate(text=symbols); s.subset(f)
f.flavor=None; f.save(ROOT/'assets/fonts/reset-ui-medium.ttf')
f.flavor='woff2'; f.save(ROOT/'preview/passport.woff2')
(ROOT/'assets/fonts/reset_glyphs.json').write_text(json.dumps({'converter':'lv_font_conv@1.5.3','size':14,'bpp':4,'codepoints':sorted(map(ord,symbols))},indent=2)+'\n')
subprocess.run([str(ROOT/'node_modules/.bin/lv_font_conv'),'--font','assets/fonts/reset-ui-medium.ttf','--symbols',symbols,'--size','14','--bpp','4','--format','lvgl','--no-compress','--lv-font-name','reset_font_14','--lv-include','lvgl.h','--output','assets/fonts/reset_font_14.c'],cwd=ROOT,check=True)
generated=ROOT/'assets/fonts/reset_font_14.c'
generated.write_text(generated.read_text().rstrip()+'\n')
print(f'Font: {len(symbols)} glyphs checked and generated; U+9F98 intentionally absent.')
