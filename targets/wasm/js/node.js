// Exemplo mínimo para Node.js (ESM): processamento estéreo em bloco.
import createOrbitModule from './orbit_delay_wasm.js';

const module = await createOrbitModule();

const init = module.cwrap('orbit_wasm_init', 'number', ['number', 'number']);
const freeFx = module.cwrap('orbit_wasm_free', null, []);
const process = module.cwrap('orbit_wasm_process_stereo', 'number', ['number', 'number', 'number', 'number', 'number']);
const setFeedback = module.cwrap('orbit_wasm_set_feedback', 'number', ['number']);
const setMix = module.cwrap('orbit_wasm_set_mix', 'number', ['number']);
const setToneHz = module.cwrap('orbit_wasm_set_tone_hz', 'number', ['number']);

const blockSize = 128;
const ok = init(48000, 48000);
if (!ok) throw new Error('orbit_wasm_init falhou');

setFeedback(0.42);
setMix(0.35);
setToneHz(6200);

const bytes = blockSize * Float32Array.BYTES_PER_ELEMENT;
const ptrInL = module._malloc(bytes);
const ptrInR = module._malloc(bytes);
const ptrOutL = module._malloc(bytes);
const ptrOutR = module._malloc(bytes);

const inL = new Float32Array(blockSize);
const inR = new Float32Array(blockSize);
inL[0] = 1.0; // impulso para teste rápido

module.HEAPF32.set(inL, ptrInL >> 2);
module.HEAPF32.set(inR, ptrInR >> 2);
process(ptrInL, ptrInR, ptrOutL, ptrOutR, blockSize);

const outL = module.HEAPF32.slice(ptrOutL >> 2, (ptrOutL >> 2) + blockSize);
console.log('Primeiras 8 amostras L:', Array.from(outL.slice(0, 8)));

module._free(ptrInL);
module._free(ptrInR);
module._free(ptrOutL);
module._free(ptrOutR);
freeFx();
