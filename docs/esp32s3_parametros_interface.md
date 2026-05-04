# Mapeamento de parâmetros musicais — ESP32/S3

Este documento lista os parâmetros musicais relevantes no DSP e identifica o status de implementação na UI atual (encoder + tela TFT).

## Implementados e acessíveis pela UI (encoder+tela)

- **MIX** (`ap.mix`) — implementado e editável.
- **BPM** (`tempoBpm` local da UI) — implementado e editável, usado para calcular delay em amostras.
- **FEEDBACK** (`ap.feedback`) — implementado e editável.
- **DIVISION** (`noteDivision` local da UI) — implementado e editável, usado com BPM.
- **OFFSET** (`ap.offsetSamples`) — implementado de forma indireta via `tempoDelaySamples * OFFSET`.
- **FOCUS** (`ap.toneHz`) — implementado e editável.
- **BYPASS** — implementado por botão dedicado (força `mix=0`).

## Implementados no bridge/core, mas **não acessíveis** na UI atual

- **ORBIT** (`ap.orbit`) — fixo em `0.5f` na UI.
- **STEREO SPREAD** (`ap.stereoSpread`) — não exposto na UI.
- **INPUT GAIN** (`ap.inputGain`) — não exposto na UI.
- **OUTPUT GAIN** (`ap.outputGain`) — não exposto na UI.
- **SMEAR / DIFFUSION AMOUNT** (`ap.smearAmount`) — fixo em `0.2f` na UI.
- **DIFFUSER STAGES** (`ap.diffuserStages`) — fixo em `2` na UI.
- **DC BLOCK ENABLE** (`ap.dcBlockEnabled`) — não exposto na UI; como `updateBridge()` cria `AudioParams` novo, as republicações da UI reimpõem o default (`true`).
- **READ MODE** (`ap.readMode`) — não exposto na UI; as republicações da UI também reimpõem o default (`Accidental`).

## Implementados no core DSP, mas **não roteados** pelo `AudioParams` atual

- **TEMPO BPM** (`setTempoBpm`) — existe no core, mas a app usa cálculo local de `offsetSamples`.
- **NOTE DIVISION** (`setNoteDivision`) — existe no core, mas a app usa cálculo local de `offsetSamples`.
- **SHIMMER MODE** (`setShimmerMode`) — existe no core, sem campo no `AudioParams` e sem UI.

## Observações de UX para reestruturação

- A UI de grade 2x3 já está no limite visual para nomes + slider + estado, com pouco espaço para valor numérico.
- Parâmetros com grande impacto musical que valeria priorizar em uma segunda camada/página:
  - `STEREO SPREAD`
  - `SMEAR`
  - `ORBIT`
  - `READ MODE`
- Parâmetros de “setup” (menos mexidos ao vivo), candidatos a menu avançado:
  - `INPUT/OUTPUT GAIN`
  - `DIFFUSER STAGES`
  - `DC BLOCK`
  - `SHIMMER MODE`
