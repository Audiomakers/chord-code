# Chord Code 1.3.0

## Novidades
- **Janela redimensionável**: arraste o canto inferior direito (60% a 200%, mantendo a proporção).
- **Tamanho padrão** em Configurações > JANELA: 75%, 90%, 100%, 125% ou 150%. Clicar muda a janela na hora e vira o tamanho de todas as janelas novas do Chord Code no computador. Arrastar o canto muda só a janela aberta.
- **ARRASTAR WAV**: a progressão em áudio, com timbre, ritmo, duração, alternância e BPM. WAV 24-bit 44,1 kHz estéreo, -1 dBFS, loop perfeito.
- **SALVAR** abre um menu: Salvar MIDI ou Salvar WAV.
- **Teclado MIDI** (botão TECLADO no topo):
  - NOTAS: como antes, cada tecla é uma nota.
  - ACORDES DO TOM: cada tecla toca o acorde da escala daquela nota (notas fora da escala vão para o grau de baixo).
  - TECLAS BRANCAS: C D E F G A B = graus I a VII, em qualquer tom e escala.
  - O acorde segue a oitava tocada e usa a complexidade e a forma do acorde escolhidas.
- **Gravar acorde no card**: clique no ● do card, toque um acorde no teclado MIDI e ele entra naquele slot, já travado. Nos modos de acorde basta uma tecla; no modo NOTAS ele lê o acorde que você tocou (nome detectado, inclusive inversões como C/E) e mantém exatamente as notas tocadas. Clicar de novo no ● cancela.
- **ALTERNÂNCIA** (ao lado de DURAÇÃO): IGUAL, LONGO-CURTO ou CURTO-LONGO. Os acordes alternam em pares (1,5x e 0,5x da duração) sem mudar o tamanho do loop. Vale no MIDI, na prévia e no WAV.
- **Batida de referência** (botão BATIDA no topo): TRAP, FUNK BR, DRILL, BOOM BAP, LO-FI, HOUSE, REGGAETON, AFROBEATS, PHONK. Toca junto com o REPRODUZIR no BPM do projeto; com batida ligada o REPRODUZIR fica em loop. Não vai no MIDI nem no WAV.

## Inclui
- Correção do Salvar/Arrastar MIDI no Windows com pasta de usuário acentuada (PR #4).
