#!/bin/bash
set -euo pipefail

project_root="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="${CCP_BUILD_DIR:-$project_root/build-macos}"
vst3="$build_dir/ChordCodePro_artefacts/Release/VST3/Chord Code.vst3"
au="$build_dir/ChordCodePro_artefacts/Release/AU/Chord Code.component"

validate_bundle() {
  local bundle="$1"
  local executable
  local architectures
  local minimum_system

  test -d "$bundle"
  executable="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "$bundle/Contents/Info.plist")"
  executable="$bundle/Contents/MacOS/$executable"
  test -x "$executable"

  architectures="$(lipo -archs "$executable")"
  echo "$(basename "$bundle"): $architectures"
  [[ " $architectures " == *" arm64 "* ]]
  [[ " $architectures " == *" x86_64 "* ]]

  minimum_system="$(/usr/libexec/PlistBuddy -c 'Print :LSMinimumSystemVersion' "$bundle/Contents/Info.plist")"
  echo "$(basename "$bundle"): minimum macOS $minimum_system"
  [[ "$minimum_system" == "10.15" ]]

  xcrun vtool -show-build -arch x86_64 "$executable" | grep -E 'minos[[:space:]]+10\.15'

  codesign --verify --deep --strict --verbose=2 "$bundle"
}

validate_bundle "$vst3"
validate_bundle "$au"

if [[ "${CCP_REQUIRE_SIGNED:-0}" == "1" ]]; then
  codesign -dv --verbose=4 "$vst3" 2>&1 | grep -F "Authority=Developer ID Application"
  codesign -dv --verbose=4 "$au" 2>&1 | grep -F "Authority=Developer ID Application"
  pkg_path="$(find "$project_root/dist-macos" -maxdepth 1 -name '*.pkg' -print -quit)"
  pkgutil --check-signature "$pkg_path" | grep -F "Developer ID Installer"
  xcrun stapler validate "$pkg_path"
fi

au_destination="$HOME/Library/Audio/Plug-Ins/Components/Chord Code.component"
mkdir -p "$(dirname "$au_destination")"
ditto "$au" "$au_destination"
killall -9 AudioComponentRegistrar 2>/dev/null || true
sleep 3

if ! auval -v aumu CcPr AuMk; then
  if [[ "${CCP_REQUIRE_AUVAL:-0}" == "1" ]]; then
    exit 1
  fi
  echo "AU validation failed, but CCP_REQUIRE_AUVAL is disabled." >&2
fi

echo "macOS bundles validated successfully."
