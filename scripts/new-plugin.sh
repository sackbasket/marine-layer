#!/usr/bin/env bash
# Turn a fresh copy of the template into a new plug-in.
#   scripts/new-plugin.sh <Name> <4-char code> ["tagline"] [VST3 category]
# Example:
#   scripts/new-plugin.sh Unveil Unv1 "perceptual unmasking EQ" EQ
set -euo pipefail
cd "$(dirname "$0")/.."

NAME="${1:?usage: new-plugin.sh <Name> <Code> [tagline] [category]}"
CODE="${2:?4-character plug-in code, e.g. Abc1}"
TAGLINE="${3:-}"
CATEGORY="${4:-Fx}"

[[ "$NAME" =~ ^[A-Za-z][A-Za-z0-9]*$ ]] || { echo "Name must be letters/digits, no spaces"; exit 1; }
[[ ${#CODE} -eq 4 ]] || { echo "Code must be exactly 4 characters"; exit 1; }
[[ "$CODE" =~ ^[A-Z] ]] || { echo "Code must start with an uppercase letter"; exit 1; }

LOWER=$(echo "$NAME" | tr '[:upper:]' '[:lower:]')
python3 - "$NAME" "$CODE" "$TAGLINE" "$CATEGORY" "$LOWER" <<'PY'
import re, sys
name, code, tagline, category, lower = sys.argv[1:]
p = 'plugin.cmake'
s = open(p).read()
def setv(key, val):
    global s
    s = re.sub(r'set\(%s\s+"[^"]*"' % key, 'set(%s "%s"' % (key, val), s)
setv('PLUGIN_NAME', name)
setv('PLUGIN_DISPLAY', name)
setv('PLUGIN_TAGLINE', tagline)
setv('PLUGIN_CODE', code)
setv('PLUGIN_VERSION', '0.1.0')
setv('PLUGIN_CATEGORY', category)
setv('PLUGIN_BUNDLE_ID', 'com.' + lower + '.fx')
open(p, 'w').write(s)
PY

sed -i.bak "s/^# TemplateFX$/# $NAME/" README.md && rm -f README.md.bak
echo "Configured $NAME ($CODE). Now replace Source/dsp/Engine.*, Source/Params.h and Source/ui/Display.*"
