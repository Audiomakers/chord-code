# CHORD CODE PRO — build macOS

O projeto gera dois formatos no macOS:

- VST3 universal (`arm64` e `x86_64`);
- Audio Unit (`.component`);

Execute em um Mac com Xcode, CMake e as ferramentas de linha de comando instaladas:

```bash
chmod +x scripts/build-macos-installer.sh
./scripts/build-macos-installer.sh
```

O `.pkg` instala:

- `/Library/Audio/Plug-Ins/VST3/Chord Code Pro.vst3`;
- `/Library/Audio/Plug-Ins/Components/Chord Code Pro.component`;

O produto é distribuído como plugin: VST3 no Windows e VST3/AU no macOS. Não há build Standalone.

Para uma distribuição sem alertas do Gatekeeper, configure os certificados:

```bash
export DEVELOPER_ID_APPLICATION="Developer ID Application: SUA EMPRESA (TEAMID)"
export DEVELOPER_ID_INSTALLER="Developer ID Installer: SUA EMPRESA (TEAMID)"
./scripts/build-macos-installer.sh
```

Depois, envie o pacote para notarização usando `notarytool` e aplique o ticket com `stapler`.

## GitHub Actions

O workflow `Build macOS plugins` compila e valida automaticamente uma versão
universal sem assinatura. Ele gera um artefato privado contendo VST3, AU e o
instalador `.pkg` de teste.

O workflow `Release signed macOS installer` importa os certificados em um
keychain temporário, assina os plugins e o instalador, envia o pacote para a
notarização da Apple, aplica o ticket e publica o `.pkg` em uma release do
repositório privado.

Configure estes secrets somente para o workflow de release:

- `APPLE_CERTIFICATES_P12_BASE64`;
- `APPLE_CERTIFICATE_PASSWORD`;
- `APPLE_KEYCHAIN_PASSWORD`;
- `APPLE_DEVELOPER_ID_APPLICATION`;
- `APPLE_DEVELOPER_ID_INSTALLER`;
- `APPLE_ID`;
- `APPLE_TEAM_ID`;
- `APPLE_APP_SPECIFIC_PASSWORD`.

Nunca envie certificados, senhas ou arquivos `.p12` para o repositório.
