$ErrorActionPreference = 'Stop'
cmake -S "$PSScriptRoot\.." -B "$PSScriptRoot\..\build" -G "Visual Studio 17 2022" -A x64
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build "$PSScriptRoot\..\build" --config Release --target ChordCodeProInstaller ChordCodePro_Standalone chordcode_pro_tests chordcode_audio_tests
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
ctest --test-dir "$PSScriptRoot\..\build" -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host 'VST3: build\ChordCodePro_artefacts\Release\VST3\Chord Code.vst3'
Write-Host 'Installer: build\Release\Windows\ChordCodeProInstaller.exe'
