// Exemplo mínimo de AudioWorkletProcessor chamando o WASM por bloco.
// Espera o bundle gerado em ./orbit_delay_wasm.js (EMSCRIPTEN MODULARIZE + ES6).
import createOrbitModule from './orbit_delay_wasm.js';

class OrbitDelayProcessor extends AudioWorkletProcessor {
  static get parameterDescriptors() {
    return [
      { name: 'orbit', defaultValue: 0.35, minValue: 0.0, maxValue: 1.0 },
      { name: 'offsetSamples', defaultValue: 0.0, minValue: -24000.0, maxValue: 24000.0 },
      { name: 'tempoBpm', defaultValue: 120.0, minValue: 20.0, maxValue: 320.0 },
      { name: 'noteDivision', defaultValue: 1.0, minValue: 0.0625, maxValue: 4.0 },
      { name: 'stereoSpread', defaultValue: 0.5, minValue: 0.0, maxValue: 1.0 },
      { name: 'feedback', defaultValue: 0.4, minValue: 0.0, maxValue: 0.98 },
      { name: 'mix', defaultValue: 0.35, minValue: 0.0, maxValue: 1.0 },
      { name: 'inputGain', defaultValue: 1.0, minValue: 0.0, maxValue: 4.0 },
      { name: 'outputGain', defaultValue: 1.0, minValue: 0.0, maxValue: 4.0 },
      { name: 'toneHz', defaultValue: 6500.0, minValue: 20.0, maxValue: 20000.0 },
      { name: 'smearAmount', defaultValue: 0.25, minValue: 0.0, maxValue: 1.0 },
      { name: 'diffuserStages', defaultValue: 4.0, minValue: 0.0, maxValue: 8.0 },
      { name: 'dcBlockEnabled', defaultValue: 1.0, minValue: 0.0, maxValue: 1.0 }
    ];
  }

  constructor(options = {}) {
    super();
    this.blockSize = 128;
    this.ready = this.boot(options.processorOptions || {});
  }

  async boot(processorOptions) {
    this.module = await createOrbitModule();
    this.api = {
      init: this.module.cwrap('orbit_wasm_init', 'number', ['number', 'number']),
      free: this.module.cwrap('orbit_wasm_free', null, []),
      process: this.module.cwrap('orbit_wasm_process_stereo', 'number', ['number', 'number', 'number', 'number', 'number']),
      set_orbit: this.module.cwrap('orbit_wasm_set_orbit', 'number', ['number']),
      set_offset_samples: this.module.cwrap('orbit_wasm_set_offset_samples', 'number', ['number']),
      set_tempo_bpm: this.module.cwrap('orbit_wasm_set_tempo_bpm', 'number', ['number']),
      set_note_division: this.module.cwrap('orbit_wasm_set_note_division', 'number', ['number']),
      set_stereo_spread: this.module.cwrap('orbit_wasm_set_stereo_spread', 'number', ['number']),
      set_feedback: this.module.cwrap('orbit_wasm_set_feedback', 'number', ['number']),
      set_mix: this.module.cwrap('orbit_wasm_set_mix', 'number', ['number']),
      set_input_gain: this.module.cwrap('orbit_wasm_set_input_gain', 'number', ['number']),
      set_output_gain: this.module.cwrap('orbit_wasm_set_output_gain', 'number', ['number']),
      set_tone_hz: this.module.cwrap('orbit_wasm_set_tone_hz', 'number', ['number']),
      set_smear_amount: this.module.cwrap('orbit_wasm_set_smear_amount', 'number', ['number']),
      set_diffuser_stages: this.module.cwrap('orbit_wasm_set_diffuser_stages', 'number', ['number']),
      set_dc_block_enabled: this.module.cwrap('orbit_wasm_set_dc_block_enabled', 'number', ['number']),
      set_read_mode: this.module.cwrap('orbit_wasm_set_read_mode', 'number', ['number'])
    };

    const delaySize = processorOptions.delaySize || 48000;
    if (!this.api.init(sampleRate, delaySize)) {
      throw new Error('orbit_wasm_init falhou');
    }

    const bytes = this.blockSize * Float32Array.BYTES_PER_ELEMENT;
    this.ptrInL = this.module._malloc(bytes);
    this.ptrInR = this.module._malloc(bytes);
    this.ptrOutL = this.module._malloc(bytes);
    this.ptrOutR = this.module._malloc(bytes);
  }

  applyParams(parameters) {
    const at = (name) => (parameters[name]?.length ? parameters[name][0] : OrbitDelayProcessor.parameterDescriptors.find(p => p.name === name).defaultValue);
    this.api.set_orbit(at('orbit'));
    this.api.set_offset_samples(at('offsetSamples'));
    this.api.set_tempo_bpm(at('tempoBpm'));
    this.api.set_note_division(at('noteDivision'));
    this.api.set_stereo_spread(at('stereoSpread'));
    this.api.set_feedback(at('feedback'));
    this.api.set_mix(at('mix'));
    this.api.set_input_gain(at('inputGain'));
    this.api.set_output_gain(at('outputGain'));
    this.api.set_tone_hz(at('toneHz'));
    this.api.set_smear_amount(at('smearAmount'));
    this.api.set_diffuser_stages(Math.round(at('diffuserStages')));
    this.api.set_dc_block_enabled(at('dcBlockEnabled') >= 0.5 ? 1 : 0);
    this.api.set_read_mode(1);
  }

  process(inputs, outputs, parameters) {
    if (!this.module || !this.api) {
      return true;
    }

    const input = inputs[0] || [];
    const output = outputs[0] || [];
    if (output.length < 2) {
      return true;
    }

    const inL = input[0] || new Float32Array(this.blockSize);
    const inR = input[1] || inL;
    const outL = output[0];
    const outR = output[1];

    this.applyParams(parameters);

    this.module.HEAPF32.set(inL, this.ptrInL >> 2);
    this.module.HEAPF32.set(inR, this.ptrInR >> 2);

    this.api.process(this.ptrInL, this.ptrInR, this.ptrOutL, this.ptrOutR, inL.length);

    outL.set(this.module.HEAPF32.subarray(this.ptrOutL >> 2, (this.ptrOutL >> 2) + inL.length));
    outR.set(this.module.HEAPF32.subarray(this.ptrOutR >> 2, (this.ptrOutR >> 2) + inL.length));
    return true;
  }
}

registerProcessor('orbit-delay-processor', OrbitDelayProcessor);
