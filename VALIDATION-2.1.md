# CHORD CODE PRO 2.1.1 — entrega e validação

## Correção de distribuição 2.1.1

- Identificada a causa da atualização não aparecer no FL Studio: o arquivo manual versionado podia coexistir com `Chord Code Pro.vst3`, compartilhando o mesmo class ID, e a configuração de build Windows ainda anunciava internamente `2.0.0`.
- A versão binária agora é `2.1.1`.
- O arquivo manual voltou a se chamar exatamente `Chord Code Pro.vst3`, para substituir a versão antiga.
- O instalador agora remove somente os caminhos antigos conhecidos do produto antes de instalar o bundle atualizado no caminho VST3 padrão.
- O instalador exige que o FL Studio esteja fechado e orienta o rescan de plugins previamente verificados.

## Escopo implementado

- Revisão compacta: janela final fixa em `750 × 459`, mantendo a proporção exata do canvas mestre `1536 × 940` por transformação única do conteúdo.
- Correção da colisão visual entre `CHORD CODE` e o badge `PRO`, com fitting horizontal limitado à área definida no Figma.
- Ícones de configurações, play, lock, variação, aleatório, histórico, arrastar e salvar agora usam os SVGs originais extraídos do node `2:2` e embutidos no binário.
- Posições dos ícones dos cards corrigidas para os estados neutro e selecionado; waveform reposicionada nos valores exatos do Figma.
- Build configurada para `VST3 + Standalone` em Windows e macOS, acrescentando `AU` automaticamente quando compilada em macOS.
- Editor JUCE substituído sem recriar o processor ou o motor musical.
- Canvas fixo `1536 × 940`, sem resize, usando o frame Figma `2:2` como coordenada mestre.
- Tokens centrais de cor, tipografia Inter embutida, raios, bordas e dimensões em `ChordCodeTheme`.
- Componentes reais para dropdowns, cards, controle segmentado, velocidade, continuar e barra de ações.
- Cards com estados neutro, hover, selecionado, lock e playback; acorde continua branco no estado selecionado.
- Lock por slot preserva o acorde completo e o gerador considera os slots travados.
- Variação individual usa o histórico sequencial/contextual já existente no gerador.
- Continuar estende a frase a oito acordes e alterna entre os dois grupos de quatro.
- Voicing e ritmo conectados aos parâmetros existentes; comprimento e velocidade influenciam playback/MIDI conforme o motor atual.
- Aleatório agora modifica apenas tonalidade, escala, estilo, vibe, complexidade, tensão e progressão. Timbre, voicing, ritmo e comprimento permanecem intactos.
- Histórico passa a restaurar também ritmo, comprimento e velocidade, além da progressão e seus parâmetros harmônicos.
- Playback usa a velocidade global e realça o card ativo por um timer de UI de 30 Hz.
- Timer, FileChooser e callbacks assíncronos são encerrados/neutralizados antes do teardown do editor.
- Bassline MIDI foi preservada no motor e exposta no menu de configurações, sem alterar a barra principal do Figma.
- Updater permanece externo: abre a página oficial sem HTTP na audio thread e sem tentar substituir o VST carregado.

## Auditoria visual Figma → JUCE

| Elemento | Figma | Implementação |
|---|---:|---:|
| Frame mestre | 1536 × 940 | 1536 × 940 |
| Janela apresentada | escala proporcional | 750 × 459 |
| Header divider | y 90.5 | y 90.5 |
| Dropdowns linha 1 | y 140.5, h 50 | y 140, h 50 |
| Dropdowns linha 2 | y 242.5, h 50 | y 242, h 50 |
| Cards | y 332.5, h 252 | y 332, h 252 |
| Cards principais | w 310 | w 310 |
| Continuar | w 142 | w 142 |
| Grupos inferiores | y 608.5, h 118 | y 608, h 118 |
| Ações | y 758.5, h 84 | y 758, h 84 |
| Gerar | 340 × 84 | 340 × 84 |
| Laranja principal | #FF5706 | #FF5706 |
| Fundo | #020304 | #020304 |
| Fonte | Inter | Inter embutida |

O arquivo `Reference/Chord-Code-Pro-2.1-preview.png` é uma renderização de auditoria das mesmas coordenadas e tokens usados pelo editor. Não é uma captura do binário Windows em execução.

## Testes executados neste ambiente

- Core tests: **aprovados** (`Chord Code Pro 2.0 core: all tests passed`).
- Build Windows x64 VST3: **aprovada**.
- Build Windows x64 Standalone: **aprovada**.
- Build do instalador com VST3 e Standalone embutidos: **aprovada**.
- Binários identificados como PE32+ x86-64: **aprovado**.
- Exports VST3 `InitDll`, `GetPluginFactory`, `ExitDll`: **presentes**.
- Categoria VST3 `Instrument|Synth`: **presente**.
- Versão binária `2.1.1`: **presente**.
- MIDI Format 0 / um único track: **coberto por teste automatizado**.
- Locks, variações, oito acordes, condução de vozes, estilos e Jazz: **cobertos pelos testes de core existentes**.

## Testes que exigem Windows + FL Studio

Não foi possível executar o FL Studio neste ambiente Linux. Permanecem como validação manual obrigatória no computador Windows:

- scan do VST3 no FL Studio;
- abrir, fechar, remover e recriar a instância repetidamente;
- drag MIDI para o Channel Rack/Piano Roll;
- confirmação visual de que somente uma pattern/categoria é criada;
- save/load do projeto com restauração completa;
- BPM Sync durante transporte;
- comparação lado a lado do VST realmente aberto com o Figma em escala 100%.

## Arquivos de código alterados

- `CMakeLists.txt`
- `Source/PluginEditor.h`
- `Source/PluginEditor.cpp`
- `Source/PluginProcessor.h`
- `Source/PluginProcessor.cpp`
- `Source/LookAndFeel.h`
- `Source/LookAndFeel.cpp`
- `Source/ChordCodeTheme.h`
- `Source/ChordCodeTheme.cpp`
- `Source/Fonts/InterRegular.inc`
- `Source/Fonts/InterBold.inc`
- `Installer/Installer.cpp`
- `README-2.0.md`

## Pendências conscientes

- Favoritos não aparecem no node atual `2:2`; por fidelidade, não foi inventado um botão extra na barra principal.
- A janela/popup detalhado de histórico e uma tela completa de configurações dependem de frames futuros do Figma; a arquitetura e os menus funcionais foram mantidos sem criar um redesign arbitrário.
- O checker remoto de atualização ainda não consulta uma API de versão; o fluxo seguro abre o updater/site externo fora da audio thread.
- O preview interno aplica comprimento, BPM, timbre, voicing e velocidade; os modos rítmicos completos permanecem garantidos no MIDI exportado. Humanize/Bounce em áudio em tempo real ainda pode ser aprofundado sem alterar o arquivo MIDI.
