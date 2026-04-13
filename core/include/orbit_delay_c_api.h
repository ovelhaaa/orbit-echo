#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque handle para a instância C++ interna do Orbit Delay. */
typedef struct OrbitDelayHandle OrbitDelayHandle;

/**
 * Cria/inicializa uma instância e anexa buffers de delay.
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
 * Libera a instância criada por orbit_init.
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
bool orbit_set_lowpass_cutoff_hz(OrbitDelayHandle* handle, float value);
bool orbit_set_diffusion(OrbitDelayHandle* handle, float value);
bool orbit_set_diffuser_stages(OrbitDelayHandle* handle, uint32_t value);
bool orbit_set_dc_block_enabled(OrbitDelayHandle* handle, bool enabled);

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
