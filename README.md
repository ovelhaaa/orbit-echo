# Orbit Echo (Orbit Delay)

O **Orbit Echo** é um efeito de delay projetado para transportar seus sons para novas texturas espaciais e rítmicas. Mais do que apenas uma repetição de ecos, ele é uma ferramenta musical que adiciona profundidade, atmosfera e movimento a qualquer fonte sonora. Desenvolvido para músicos e sound designers, o Orbit Echo permite esculpir o ambiente sonoro, indo de sutis espaços em estéreo até paisagens sonoras etéreas, ricas e imersivas.

## 🎵 Características Musicais

- **Texturas e Atmosferas Ricas (Diffusion):** Ao invés de ecos frios, estéreis e exatos, o Orbit Echo conta com estágios de difusão (*Diffuser*) que "espalham" as repetições. Com um simples ajuste, seus delays podem se transformar em caudas suaves parecidas com reverberação, criando camadas celestiais perfeitas para guitarras ambientais, vocais ou sintetizadores.
- **Moldagem de Tom Quente e Orgânica (Tone):** Filtros integrados permitem que você dome o brilho das repetições ou corte frequências indesejadas. Isso significa que seus ecos se encaixam perfeitamente na mixagem, escurecendo organicamente a cada repetição — capturando a essência e o calor característicos dos delays analógicos clássicos.
- **Abertura Estéreo (Stereo Spread):** Amplie espacialmente seus ecos para que eles abracem os lados do espectro estéreo. Deixe a fonte sonora central limpa enquanto as repetições se movimentam e dançam ao redor de todo o campo sonoro, criando uma imagem imersiva.
- **Tweakabilidade e Expressão ao Vivo:** Sinta-se livre para "tocar" o efeito! O motor sonoro é equipado com suavização extrema de parâmetros, permitindo que você altere agressivamente tempos de delay, ganho e feedback em tempo real para criar distorções de pitch (*pitch-warping*) e manipulações no estilo "fita" de maneira totalmente fluida, sem cliques ou engasgos de áudio.

---

## ⚙️ Debaixo do Capô (Detalhes Técnicos)

Apesar de sua alma analógica e foco musical, o **Orbit Echo** é, em seu núcleo, uma biblioteca de Processamento Digital de Sinais (DSP) escrita em C++17 com rigor em **altíssima performance e portabilidade**. Ele foi construído para operar de maneira eficiente tanto em DAWs e aplicações nativas de desktop quanto em ambientes com recursos limitados, como microcontroladores (ex: ESP32-S3) e navegadores web via WebAssembly (Wasm).

### Estrutura do Projeto

- `core/`: O coração DSP em C++, completamente independente de plataforma e sem alocações dinâmicas de memória durante a execução.
- `targets/`: Integrações para plataformas específicas:
  - `embedded_esp32s3/`: Código e infraestrutura para rodar o efeito direto no hardware ESP32-S3 (com mapeamento de áudio e interface de display TFT).
  - `wasm/`: Exportação e wrappers para rodar no navegador, integrando-se diretamente com a Web Audio API através do JavaScript.
- `tests/` e `example_orbit_delay.cpp`: Uma robusta suíte de testes (com CTest) para verificar a estabilidade de filtros e a fidelidade de áudio ("golden tests"), além de exemplos de benchmarking de processamento (medindo nanosegundos por amostra).

### Como Compilar e Usar

O motor é gerenciado primariamente através do **CMake** (versão 3.16 ou superior).

#### 1. Compilação Nativa (Desktop) e Testes Locais
Para criar a biblioteca estática localmente e executar a bateria de testes integrados:
```bash
mkdir build && cd build
cmake .. -DBUILD_TESTING=ON
cmake --build .
ctest --output-on-failure
```

#### 2. Dispositivos ESP32-S3
Requer que o ambiente **ESP-IDF** esteja configurado no sistema.
```bash
cd targets/embedded_esp32s3
idf.py build
idf.py flash monitor
```

#### 3. WebAssembly (Wasm)
Requer a toolchain **Emscripten** (emcc).
```bash
mkdir build_wasm && cd build_wasm
emcmake cmake ..
emmake make
```

### Exemplo Rápido em C++

A API modular e Orientada a Objetos permite uma integração limpa em outros projetos. O excerto abaixo ilustra o processamento em blocos estéreo:

```cpp
#include "core/include/orbit_delay_core.h"

// Definição dos buffers globais pré-alocados para ambiente embarcado
constexpr uint32_t MAX_DELAY_SAMPLES = 48000;
float delayBufferL[MAX_DELAY_SAMPLES];
float delayBufferR[MAX_DELAY_SAMPLES];

int main() {
    // Instanciação e ligação dos buffers de áudio ao Engine
    orbit::dsp::OrbitDelayCore fx;
    fx.attachBuffers(delayBufferL, delayBufferR, MAX_DELAY_SAMPLES);

    // Setup de parâmetros musicais e modelagem de tom
    fx.reset(48000.0f);           // Sample rate
    fx.setFeedback(0.55f);        // Quantidade de repetições
    fx.setMix(0.4f);              // Balanço Dry/Wet
    fx.setOffsetSamples(7200.0f); // Tempo base do delay
    fx.setDiffuserStages(3);      // Quantidade de espalhamento/reverb
    fx.setToneHz(5500.0f);        // Escurecimento das repetições

    // Processamento de áudio estéreo em lote
    constexpr uint32_t numSamples = 512;
    float inL[numSamples], inR[numSamples], outL[numSamples], outR[numSamples];
    // (A entrada do sinal de áudio inL e inR vai aqui)

    fx.processStereo(inL, inR, outL, outR, numSamples);

    return 0;
}
```
