// Exemplo mínimo de binding UI -> mesma C API usada no embedded.
export function bindOrbitUiControls(api, root = document) {
  const controls = [
    ['orbit', 'orbit_wasm_set_orbit', parseFloat],
    ['offsetSamples', 'orbit_wasm_set_offset_samples', parseFloat],
    ['stereoSpread', 'orbit_wasm_set_stereo_spread', parseFloat],
    ['feedback', 'orbit_wasm_set_feedback', parseFloat],
    ['mix', 'orbit_wasm_set_mix', parseFloat],
    ['inputGain', 'orbit_wasm_set_input_gain', parseFloat],
    ['outputGain', 'orbit_wasm_set_output_gain', parseFloat],
    ['toneHz', 'orbit_wasm_set_tone_hz', parseFloat],
    ['smearAmount', 'orbit_wasm_set_smear_amount', parseFloat],
    ['diffuserStages', 'orbit_wasm_set_diffuser_stages', (v) => Math.round(parseFloat(v))],
    ['dcBlockEnabled', 'orbit_wasm_set_dc_block_enabled', (v) => (v === '1' || v === 'true' ? 1 : 0)]
  ];

  controls.forEach(([id, fn, parse]) => {
    const el = root.getElementById(id);
    if (!el || typeof api[fn] !== 'function') return;
    const apply = () => api[fn](parse(el.value));
    el.addEventListener('input', apply);
    apply();
  });
}
