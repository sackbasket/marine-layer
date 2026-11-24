#!/usr/bin/env bash
# Build a one-click macOS installer for a built VST3.
#   scripts/make-pkg.sh <path/to/Name.vst3> <Name> <bundle.id> <version> <output.pkg>
set -euo pipefail
VST="$1"; NAME="$2"; BUNDLE="$3"; VERSION="$4"; OUT="$5"
WORK=$(mktemp -d)

mkdir -p "$WORK/root/Library/Audio/Plug-Ins/VST3" "$WORK/resources"
cp -R "$VST" "$WORK/root/Library/Audio/Plug-Ins/VST3/"

pkgbuild --root "$WORK/root" --identifier "$BUNDLE.vst3" --version "$VERSION" \
         --install-location / "$WORK/component.pkg"

cat > "$WORK/resources/welcome.html" <<HTML
<html><body style="font-family:-apple-system,Helvetica;font-size:13px">
<h2>$NAME $VERSION</h2>
<p>This installs <b>$NAME.vst3</b> into <code>/Library/Audio/Plug-Ins/VST3</code>,
where Ableton Live and other hosts find it automatically.</p>
</body></html>
HTML
cat > "$WORK/resources/conclusion.html" <<HTML
<html><body style="font-family:-apple-system,Helvetica;font-size:13px">
<h2>Installed</h2>
<p>In Ableton Live open <b>Settings &rarr; Plug-Ins</b>, make sure <b>Use VST3 Plug-In System Folders</b>
is on and click <b>Rescan</b>. $NAME appears under Plug-Ins &rarr; VST3.</p>
</body></html>
HTML
LICENSE_TAG=""
if [ -f COPYING ]; then cp COPYING "$WORK/resources/license.txt"; LICENSE_TAG='<license file="license.txt"/>'; fi

cat > "$WORK/distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>$NAME</title>
    <welcome file="welcome.html" mime-type="text/html"/>
    $LICENSE_TAG
    <conclusion file="conclusion.html" mime-type="text/html"/>
    <options customize="never" require-scripts="false" hostArchitectures="arm64,x86_64"/>
    <domains enable_localSystem="true"/>
    <choices-outline><line choice="default"/></choices-outline>
    <choice id="default" title="$NAME VST3"><pkg-ref id="$BUNDLE.vst3"/></choice>
    <pkg-ref id="$BUNDLE.vst3">component.pkg</pkg-ref>
</installer-gui-script>
XML

productbuild --distribution "$WORK/distribution.xml" --resources "$WORK/resources" \
             --package-path "$WORK" "$OUT"
rm -rf "$WORK"
ls -la "$OUT"
