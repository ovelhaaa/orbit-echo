# Orbit Echo (Orbit Delay)

O **Orbit Echo** (ou Orbit Delay) é uma biblioteca de Processamento Digital de Sinais (DSP) escrita em C++17 focada em fornecer um efeito de delay de alta qualidade e com alta performance. O projeto foi arquitetado desde o princípio para ser versátil, podendo ser executado tanto em aplicações nativas de desktop quanto em ambientes com recursos limitados, como microcontroladores (ex: ESP32-S3), além de navegadores web via WebAssembly (Wasm).

## Estrutura do Repositório

A base de código do projeto é dividida em diferentes módulos para garantir a portabilidade do núcleo de processamento de áudio entre diversas plataformas:

- `core/`: Contém todo o código principal de DSP, independente de plataforma.
  - `include/`: Arquivos de cabeçalho (headers) com a definição das classes principais (`OrbitDelayCore`, `DelayLine`, `BiquadLowpass`, `DCBlocker`, `AllpassDiffuser`, `LinearSmoother`, etc) e uma interface compatível com C (`orbit_delay_c_api.h`).
  - `src/`: Implementações em C++ do núcleo de processamento, algoritmos e das APIs.
- `targets/`: Implementações e wrappers específicos para as plataformas e ambientes suportados.
  - `embedded_esp32s3/`: Integração completa para o microcontrolador ESP32-S3. Contém configurações de hardware da placa, uma engine de processamento de áudio, mapeamento de parâmetros e uma interface de usuário utilizando um display TFT.
  - `wasm/`: Bindings e wrappers para exportar a biblioteca para WebAssembly. Permite rodar os efeitos em navegadores modernos, e inclui uma sub-pasta `js/` para integração direta do Wasm com a Web Audio API.
- `tests/`: Conjunto robusto de testes unitários usando CTest/CMake. Abrange testes focados em funcionalidades isoladas (`test_delay_line.cpp`, `test_filters.cpp`, etc.) e testes visando resultados precisos de áudio ("golden tests" através do `render_golden.cpp`).
- `example_orbit_delay.cpp`: Um programa em C++ simples que demonstra a inicialização e o uso básico da biblioteca para processar buffers de áudio. Também inclui benchmarking para avaliar o custo de CPU (nanossegundos por amostra) do processamento.
- `CMakeLists.txt`: Orquestrador e sistema de build do projeto, configurado para C++17. É capaz de definir os arquivos alvo automaticamente a depender das condições ambientais.

## Principais Funcionalidades

A classe DSP primária `OrbitDelayCore` provê os seguintes recursos:
- **Delay Estéreo e Mono**: Suporte otimizado para o processamento de áudio amostra-a-amostra ou processamento vetorizado através de blocos (buffers).
- **Filtros e Modelagem de Tom (Tone & Diffusion)**: Uso de filtros integrados (como Lowpass Biquad, bloqueador DC), modulação de espalhamento e estágios de *Diffuser* baseados em All-Pass para uma propagação e granulação realista dos ecos.
- **Manipulação Estéreo**: Parâmetros como *Stereo Spread* ampliam espacialmente o efeito sonoro.
- **Suavização de Parâmetros (Smoothing)**: Implementação otimizada nativamente para chips embarcados (`MCU`), eliminando cliques bruscos ou ruídos de distorção (*zipper noise*) durante o ajuste ao vivo de variáveis da interface, como tempo e ganho de delay.
- **APIs Modulares**: Oferece a robustez Orientada a Objetos com C++ e uma API limpa puramente em C (`orbit_delay_c_api.h`), abrindo o caminho para FFI (Foreign Function Interfaces) com Rust, Python ou linguagens web.

## Como Compilar e Usar

O projeto utiliza o **CMake** como seu principal gerador de sistema de compilação (necessita CMake 3.16 ou superior).

### 1. Compilação Nativa (Desktop) e Execução de Testes

Para compilar a biblioteca localmente e criar os executáveis dos testes e do exemplo, execute:

```bash
mkdir build
cd build
cmake .. -DBUILD_TESTING=ON
cmake --build .
```

Se desejar executar a bateria de testes integrados do CTest:
```bash
ctest --output-on-failure
```

### 2. Compilação para Dispositivo ESP32-S3

A compilação para sistemas embarcados da Espressif demanda que o **ESP-IDF** esteja configurado no sistema (juntamente da variável ambiental `IDF_PATH`). O CMake raiz detecta o ESP-IDF e mapeia o diretório `targets/embedded_esp32s3`.

Para compilar e enviar ao microcontrolador ESP32-S3:
```bash
cd targets/embedded_esp32s3
idf.py build
idf.py flash monitor
```

### 3. Compilação para WebAssembly (Wasm)

O *target* Wasm faz o uso primário da toolchain **Emscripten** (emcc). Utilize a sua integração nativa com o CMake para processar o projeto Wasm:

```bash
mkdir build_wasm
cd build_wasm
emcmake cmake ..
emmake make
```
Isto fará a compilação gerando tanto o módulo WebAssembly final, quanto os arquivos de integração JavaScript vinculados à pasta Wasm.

## Exemplo Rápido (C++)

O excerto simplificado abaixo ilustra como configurar e utilizar o `OrbitDelayCore` em código C++, provendo som a seus canais:

```cpp
#include "core/include/orbit_delay_core.h"

// Definição dos buffers globais pré-alocados para ambiente embarcado
constexpr uint32_t MAX_DELAY_SAMPLES = 48000;
float delayBufferL[MAX_DELAY_SAMPLES];
float delayBufferR[MAX_DELAY_SAMPLES];

int main() {
    // Instanciação e ligação dos buffers ao Engine.
    orbit::dsp::OrbitDelayCore fx;
    fx.attachBuffers(delayBufferL, delayBufferR, MAX_DELAY_SAMPLES);

    // Reset/Setup de parâmetros de funcionamento padrão
    fx.reset(48000.0f);
    fx.setFeedback(0.55f);
    fx.setMix(0.4f);
    fx.setOffsetSamples(7200.0f);
    fx.setDiffuserStages(3);
    fx.setToneHz(5500.0f);

    // Buffers de processamento
    constexpr uint32_t numSamples = 512;
    float inL[numSamples], inR[numSamples], outL[numSamples], outR[numSamples];
    // (A população dos dados de inL e inR com áudio vai aqui)

    // Processamento estéreo em lote
    fx.processStereo(inL, inR, outL, outR, numSamples);

    return 0;
}
```
