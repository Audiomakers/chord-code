#!/bin/bash
set -euo pipefail

project_root="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="${CCP_BUILD_DIR:-$project_root/build-macos}"
stage_dir="$build_dir/package-root"
dist_dir="${CCP_DIST_DIR:-$project_root/dist-macos}"
cmake_version="$(sed -nE 's/^project\(ChordCodePro VERSION ([0-9]+\.[0-9]+\.[0-9]+).*/\1/p' "$project_root/CMakeLists.txt")"
version="${CCP_VERSION:-$cmake_version}"

if [[ -z "$version" ]]; then
  echo "Unable to determine project version from CMakeLists.txt." >&2
  exit 1
fi

if [[ -n "${CCP_VERSION:-}" && "$CCP_VERSION" != "$cmake_version" ]]; then
  echo "CCP_VERSION ($CCP_VERSION) does not match CMake version ($cmake_version)." >&2
  exit 1
fi

cmake -S "$project_root" -B "$build_dir" -G Xcode \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15 \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DCCP_FETCH_JUCE=ON

cmake --build "$build_dir" --config Release \
  --target ChordCodePro_VST3 ChordCodePro_AU

rm -rf "$stage_dir"
mkdir -p "$stage_dir/Library/Audio/Plug-Ins/VST3"
mkdir -p "$stage_dir/Library/Audio/Plug-Ins/Components"
mkdir -p "$dist_dir"

ditto "$build_dir/ChordCodePro_artefacts/Release/VST3/Chord Code.vst3" \
      "$stage_dir/Library/Audio/Plug-Ins/VST3/Chord Code.vst3"
ditto "$build_dir/ChordCodePro_artefacts/Release/AU/Chord Code.component" \
      "$stage_dir/Library/Audio/Plug-Ins/Components/Chord Code.component"

if [[ -n "${DEVELOPER_ID_APPLICATION:-}" ]]; then
  codesign --force --deep --options runtime --timestamp --sign "$DEVELOPER_ID_APPLICATION" \
    "$stage_dir/Library/Audio/Plug-Ins/VST3/Chord Code.vst3"
  codesign --force --deep --options runtime --timestamp --sign "$DEVELOPER_ID_APPLICATION" \
    "$stage_dir/Library/Audio/Plug-Ins/Components/Chord Code.component"
fi

unsigned_pkg="$build_dir/Chord Code ${version} unsigned.pkg"
pkgbuild --root "$stage_dir" --identifier com.audiomakers.chordcodepro.pkg \
  --version "$version" --install-location / "$unsigned_pkg"

output_pkg="$dist_dir/Chord Code ${version} macOS.pkg"
if [[ -n "${DEVELOPER_ID_INSTALLER:-}" ]]; then
  productsign --sign "$DEVELOPER_ID_INSTALLER" --timestamp "$unsigned_pkg" "$output_pkg"
else
  mv "$unsigned_pkg" "$output_pkg"
fi

if [[ -n "${APPLE_ID:-}" || -n "${APPLE_TEAM_ID:-}" || -n "${APPLE_APP_SPECIFIC_PASSWORD:-}" ]]; then
  : "${APPLE_ID:?APPLE_ID is required for notarization}"
  : "${APPLE_TEAM_ID:?APPLE_TEAM_ID is required for notarization}"
  : "${APPLE_APP_SPECIFIC_PASSWORD:?APPLE_APP_SPECIFIC_PASSWORD is required for notarization}"
  if [[ -z "${DEVELOPER_ID_APPLICATION:-}" || -z "${DEVELOPER_ID_INSTALLER:-}" ]]; then
    echo "Notarization requires both Developer ID identities." >&2
    exit 1
  fi

  xcrun notarytool submit "$output_pkg" \
    --apple-id "$APPLE_ID" \
    --team-id "$APPLE_TEAM_ID" \
    --password "$APPLE_APP_SPECIFIC_PASSWORD" \
    --wait
  xcrun stapler staple "$output_pkg"
  xcrun stapler validate "$output_pkg"
fi

echo "Instalador criado em: $output_pkg"
if [[ -z "${DEVELOPER_ID_APPLICATION:-}" ]]; then
  echo "Build de teste sem Developer ID. Para distribuição pública, configure assinatura e notarização."
fi
