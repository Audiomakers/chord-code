# Chord Code 1.3.0

Plugin de harmonia da Audio Makers desenvolvido em C++20 e JUCE 8.

## Formatos

- Windows x64: VST3 e aplicativo (instalado com atalho na área de trabalho);
- macOS 10.15 ou superior: VST3 e Audio Unit universais (Intel e Apple Silicon);
- macOS universal: Apple Silicon (`arm64`) e Intel (`x86_64`);
- o instalador Windows instala o VST3 em `Common Files/VST3` e o aplicativo em
  `Program Files/Audio Makers/Chord Code`, com atalhos e desinstalador;
- o pacote macOS instala VST3 e AU em `/Library/Audio/Plug-Ins`.

## Recursos principais

- progressões de 2, 4 ou 8 acordes;
- locks individuais e variações por acorde;
- voicings padrão, fechado, aberto e invertido;
- ritmos Reto, Humanize, Strum e Arpejo;
- exportação e arraste de MIDI;
- sincronização com o BPM do host;
- preview interno com síntese e samples incorporados;
- histórico e memória antirrepetição;
- licença pelo e-mail da compra, verificada na primeira abertura.

## Licença

Na primeira vez que abre, o Chord Code pede o e-mail da compra e consulta
`POST https://download.chordcode.audiomakers.club/api/license/check`.
O e-mail aprovado fica salvo na pasta de dados do usuário
(`Audio Makers/Chord Code`) e vale para o VST3, o AU e o aplicativo.
Depois disso ele funciona offline e reconfere o acesso sempre que há internet;
só uma resposta `authorized: false` do servidor bloqueia o uso, nunca uma
falha de conexão.

## Créditos

O timbre Piano usa samples do Salamander Grand Piano, de Alexander Holm,
licenciados sob CC BY 3.0.

## Compilação e testes

```bash
cmake -S . -B build-core \
  -DCMAKE_BUILD_TYPE=Release \
  -DCCP_FETCH_JUCE=OFF
cmake --build build-core --config Release --target chordcode_pro_tests
ctest --test-dir build-core -C Release --output-on-failure
```

No Windows, com Visual Studio 2022:

```powershell
cmake -S . -B build-ux -G "Visual Studio 17 2022" -A x64
cmake --build build-ux --config Release --target ChordCodeProInstaller ChordCodePro_Standalone
ctest --test-dir build-ux -C Release --output-on-failure
```

O teste `chordcode_audio_tests` verifica transições sem estalos, caudas de preview,
retorno do destaque dos cards, exportação de 4/8 acordes e restauração do estado.

## Build macOS local

Em um Mac com Xcode e CMake:

```bash
./scripts/build-macos-installer.sh
./scripts/validate-macos-build.sh
```

O build gera:

- `Chord Code.vst3`;
- `Chord Code.component`;
- `Chord Code 1.3.0 macOS.pkg`.

## GitHub Actions

- **Build macOS plugins:** compila e valida automaticamente um build universal
  de teste, sem exigir certificados Apple.
- **Release signed macOS installer:** assina, notariza, aplica o ticket da Apple
  e publica o instalador em uma release privada.

As instruções de certificados e secrets estão em
[`Packaging/macOS/README.md`](Packaging/macOS/README.md).

## Identidade do plugin

- produto: `Chord Code`;
- bundle ID: `com.audiomakers.chordcodepro`;
- manufacturer code: `AuMk`;
- plugin code: `CcPr`;
- categoria: Instrument / Synth.

Certificados Apple, senhas, arquivos `.p12`, builds e instaladores gerados não
devem ser adicionados ao repositório.
