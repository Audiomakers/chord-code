# CHORD CODE PRO 1.1.0 — validação

## Build

- Windows x64 VST3: aprovado.
- Windows x64 Standalone: aprovado.
- Instalador com payload VST3 e Standalone: aprovado.
- Versão interna: `1.1.0`.
- Categoria VST3: `Instrument | Synth`.
- Exports VST3: `InitDll`, `GetPluginFactory` e `ExitDll` presentes.
- Hash do VST3 dentro do instalador idêntico ao arquivo manual: aprovado.
- Testes automatizados do motor harmônico e MIDI: aprovados.
- Progressões de 2, 4 e 8 acordes aceitas e validadas pelo motor.
- Reto, Humanize, Strum e Arpejo produzem MIDI estruturalmente distinto.

## Interface

- Dimensão fixa: `960 × 545`.
- Canvas interno: `1536 × 872`.
- Sem redimensionamento livre.
- Rodapé vazio removido.
- Tipografia de labels, dropdowns, segmentos e ações ampliada.
- Branding e controles superiores realinhados.
- Moldura usa o canvas lógico completo; a borda da resolução antiga foi removida.
- Engrenagem branca, sem estado de hover.

## Interações implementadas

- Lock ativo redesenhado, com ícone único e destaque laranja.
- Clique no card reproduz somente o acorde correspondente.
- Waveform animada enquanto o acorde está tocando.
- Slide horizontal entre acordes 1–4 e 5–8.
- Retorno a quatro acordes reduz também a progressão exportada.
- Hover laranja nos elementos clicáveis.
- Comprimento e quantidade de acordes sincronizados: 1/2 = 2, 1 = 4, 2 = 8.
- Humanize, Strum e Arpejo aplicados tanto na prévia quanto no MIDI salvo/arrastado.
- Volume geral em porcentagem, padrão 100%.

## Validação externa ainda necessária

Este ambiente Linux não executa o FL Studio. Ainda é necessário validar no computador Windows: abertura visual, clique/áudio, animação em tempo real, slide 4/8, remoção da instância e drag MIDI.
