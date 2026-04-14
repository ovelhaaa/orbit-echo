#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque handle para a instância C++ interna do Orbit Delay. */
typedef struct OrbitDelayHandle OrbitDelayHandle;

typedef enum OrbitReadMode {
    ORBIT_READ_MODE_ORBIT = 0,
    ORBIT_READ_MODE_ACCIDENTAL_REVERSE = 1,
} OrbitReadMode;

/**
 * Cria/inicializa uma instância em modo estéreo e anexa buffers de delay.
 *
 * Parâmetros obrigatórios:
 * - delay_buffer_l e delay_buffer_r devem ser não nulos.
 * - delay_size_l e delay_size_r devem ser válidos para o core interno.
 *
 * Os buffers são de propriedade do chamador e devem permanecer válidos por toda
 * a vida útil do OrbitDelayHandle.
 *
 * Política de erro:
 * - Retorna NULL se qualquer ponteiro obrigatório for nulo, se tamanhos forem inválidos
 *   ou se a criação/anexação falhar.
 */
OrbitDelayHandle* orbit_init(float sample_rate,
                             float* delay_buffer_l,
                             uint32_t delay_size_l,
                             float* delay_buffer_r,
                             uint32_t delay_size_r);

/**
 * Cria/inicializa uma instância em modo mono.
 *
 * Parâmetros obrigatórios:
 * - delay_buffer deve ser não nulo.
 * - delay_size deve ser válido para o core interno.
 *
 * O buffer é de propriedade do chamador e deve permanecer válido por toda
 * a vida útil do OrbitDelayHandle.
 *
 * Política de erro:
 * - Retorna NULL em caso de ponteiro/tamanho inválido, falha de alocação ou attach.
 */
OrbitDelayHandle* orbit_init_mono(float sample_rate, float* delay_buffer, uint32_t delay_size);

/**
 * Libera a instância criada por orbit_init/orbit_init_mono.
 *
 * Política de erro:
 * - handle nulo: no-op.
 */
void orbit_free(OrbitDelayHandle* handle);

/**
 * Reseta estado interno DSP.
 *
 * Política de erro:
 * - handle nulo: no-op.
 */
void orbit_reset(OrbitDelayHandle* handle, float sample_rate);

/**
 * Setters de parâmetros.
 *
 * Política de erro comum:
 * - handle nulo: retorna false e não altera estado.
 * - handle válido: retorna true.
 */
bool orbit_set_orbit(OrbitDelayHandle* handle, float value);
bool orbit_set_offset_samples(OrbitDelayHandle* handle, float value);
bool orbit_set_stereo_spread(OrbitDelayHandle* handle, float value);
bool orbit_set_feedback(OrbitDelayHandle* handle, float value);
bool orbit_set_mix(OrbitDelayHandle* handle, float value);
bool orbit_set_input_gain(OrbitDelayHandle* handle, float value);
bool orbit_set_output_gain(OrbitDelayHandle* handle, float value);
bool orbit_set_tone_hz(OrbitDelayHandle* handle, float value);
bool orbit_set_smear_amount(OrbitDelayHandle* handle, float value);
bool orbit_set_diffuser_stages(OrbitDelayHandle* handle, uint32_t value);
bool orbit_set_dc_block_enabled(OrbitDelayHandle* handle, bool enabled);
bool orbit_set_read_mode(OrbitDelayHandle* handle, OrbitReadMode mode);

/** Reanexa buffers estéreo com semântica canônica (L, R, size compartilhado). */
bool orbit_attach_buffers(OrbitDelayHandle* handle, float* delay_buffer_l, float* delay_buffer_r, uint32_t delay_size);
/** Reanexa buffer mono com semântica canônica. */
bool orbit_attach_buffer_mono(OrbitDelayHandle* handle, float* delay_buffer, uint32_t delay_size);

/** Alias legado temporário. */
bool orbit_set_lowpass_cutoff_hz(OrbitDelayHandle* handle, float value);
/** Alias legado temporário. */
bool orbit_set_diffusion(OrbitDelayHandle* handle, float value);

/**
 * Processamento em bloco mono.
 *
 * Política de erro:
 * - handle nulo ou buffers inválidos: retorna false e não processa.
 * - handle/buffers válidos: retorna true.
 */
bool orbit_process_mono(OrbitDelayHandle* handle,
                        const float* input,
                        float* output,
                        uint32_t num_samples);

/**
 * Processamento em bloco estéreo.
 *
 * Política de erro:
 * - handle nulo ou buffers inválidos (qualquer ponteiro nulo): retorna false e não processa.
 * - handle/buffers válidos: retorna true.
 */
bool orbit_process_stereo(OrbitDelayHandle* handle,
                          const float* input_l,
                          const float* input_r,
                          float* output_l,
                          float* output_r,
                          uint32_t num_samples);

#ifdef __cplusplus
} // extern "C"
#endif
