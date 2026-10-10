# Chord Code 1.2.1

Esta versão traz a interface baseada no novo UX, mantendo o motor de geração e a identidade interna do plugin compatíveis com sessões existentes.

## Incluído

- exportação e arraste MIDI da progressão completa, incluindo os oito acordes;
- nomes dos acordes nos arquivos MIDI salvos;
- menus de escolha de acorde com preview ao passar o mouse;
- card ativo laranja durante a reprodução, com brilho sutil;
- transições de preview com cauda suave para reduzir estalos;
- timbres de piano elétrico e órgão mais distintos;
- nome exibido como Chord Code;
- interface fixa em 960×547, com popups compactos e legíveis;
- Windows VST3, Standalone e instalador;
- macOS VST3 e AU universais para Intel e Apple Silicon.

## Requisitos

- Windows 10 ou superior, 64-bit;
- macOS 10.15 Catalina ou superior;
- JUCE 8.0.12 e CMake 3.22 ou superior para compilação.

O FL Studio decide como organiza um MIDI solto no Channel Rack. Um VST3 não consegue mover canais importados entre Filter Groups depois do drop.
