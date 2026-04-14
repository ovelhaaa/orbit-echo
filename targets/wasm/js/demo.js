import createOrbitModule from './orbit_delay_wasm.js';

const BLOCK_SIZE = 128;
const MAX_DELAY_SAMPLES = 48000 * 2;
const READ_MODE_ACCIDENTAL_REVERSE = 1;
const REVERSE_LEGACY_DEFAULTS = {
  toneHz: 1800
};

const els = {
  file: document.getElementById('audioFile'),
  processBtn: document.getElementById('processBtn'),
  status: document.getElementById('status'),
  preview: document.getElementById('preview'),
  downloadLink: document.getElementById('downloadLink'),
  feedback: document.getElementById('feedback'),
  mix: document.getElementById('mix'),
  toneHz: document.getElementById('toneHz'),
  smearAmount: document.getElementById('smearAmount'),
  tempoBpm: document.getElementById('tempoBpm'),
  noteDivision: document.getElementById('noteDivision'),
  readMode: document.getElementById('readMode')
};

const outputs = {
  feedback: document.getElementById('feedbackOut'),
  mix: document.getElementById('mixOut'),
  toneHz: document.getElementById('toneHzOut'),
  smearAmount: document.getElementById('smearAmountOut'),
  tempoBpm: document.getElementById('tempoBpmOut'),
  noteDivision: document.getElementById('noteDivisionOut'),
  readMode: document.getElementById('readModeOut')
};

for (const key of Object.keys(outputs)) {
  const input = els[key];
  const output = outputs[key];
  const render = () => {
    if (key === 'toneHz' || key === 'tempoBpm') {
      output.textContent = String(Math.round(Number(input.value)));
    } else if (key === 'noteDivision' || key === 'readMode') {
      const label = input.options[input.selectedIndex]?.textContent;
      output.textContent = label || input.value;
    } else {
      output.textContent = Number(input.value).toFixed(2);
    }
  };
  input.addEventListener('input', render);
  if (input.tagName === 'SELECT') input.addEventListener('change', render);
  render();
}

let module;
let api;

async function initWasm() {
  module = await createOrbitModule();
  api = {
    init: module.cwrap('orbit_wasm_init', 'number', ['number', 'number']),
    free: module.cwrap('orbit_wasm_free', null, []),
    process: module.cwrap('orbit_wasm_process_stereo', 'number', ['number', 'number', 'number', 'number', 'number']),
    reset: module.cwrap('orbit_wasm_reset', null, []),
    setFeedback: module.cwrap('orbit_wasm_set_feedback', 'number', ['number']),
    setMix: module.cwrap('orbit_wasm_set_mix', 'number', ['number']),
    setToneHz: module.cwrap('orbit_wasm_set_tone_hz', 'number', ['number']),
    setSmearAmount: module.cwrap('orbit_wasm_set_smear_amount', 'number', ['number']),
    setTempoBpm: module.cwrap('orbit_wasm_set_tempo_bpm', 'number', ['number']),
    setNoteDivision: module.cwrap('orbit_wasm_set_note_division', 'number', ['number']),
    setReadMode: module.cwrap('orbit_wasm_set_read_mode', 'number', ['number'])
  };

  const ok = api.init(48000, MAX_DELAY_SAMPLES);
  if (!ok) throw new Error('orbit_wasm_init falhou');
  applyReadModeDefaults();
  api.setReadMode(Number(els.readMode.value));

  els.status.textContent = 'WASM pronto. Selecione um arquivo de áudio.';
  els.processBtn.disabled = false;
}

function readFileAsArrayBuffer(file) {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onerror = () => reject(reader.error);
    reader.onload = () => resolve(reader.result);
    reader.readAsArrayBuffer(file);
  });
}

function encodeWavFromFloatStereo(left, right, sampleRate) {
  const numChannels = 2;
  const bitsPerSample = 16;
  const byteRate = (sampleRate * numChannels * bitsPerSample) / 8;
  const blockAlign = (numChannels * bitsPerSample) / 8;
  const dataSize = left.length * blockAlign;
  const buffer = new ArrayBuffer(44 + dataSize);
  const view = new DataView(buffer);

  const writeString = (offset, str) => {
    for (let i = 0; i < str.length; i += 1) view.setUint8(offset + i, str.charCodeAt(i));
  };

  writeString(0, 'RIFF');
  view.setUint32(4, 36 + dataSize, true);
  writeString(8, 'WAVE');
  writeString(12, 'fmt ');
  view.setUint32(16, 16, true);
  view.setUint16(20, 1, true);
  view.setUint16(22, numChannels, true);
  view.setUint32(24, sampleRate, true);
  view.setUint32(28, byteRate, true);
  view.setUint16(32, blockAlign, true);
  view.setUint16(34, bitsPerSample, true);
  writeString(36, 'data');
  view.setUint32(40, dataSize, true);

  let offset = 44;
  for (let i = 0; i < left.length; i += 1) {
    const l = Math.max(-1, Math.min(1, left[i]));
    const r = Math.max(-1, Math.min(1, right[i]));
    view.setInt16(offset, l < 0 ? l * 0x8000 : l * 0x7fff, true);
    view.setInt16(offset + 2, r < 0 ? r * 0x8000 : r * 0x7fff, true);
    offset += 4;
  }

  return new Blob([buffer], { type: 'audio/wav' });
}

async function decodeToStereoFloat(file) {
  const data = await readFileAsArrayBuffer(file);
  const audioCtx = new AudioContext();
  const decoded = await audioCtx.decodeAudioData(data.slice(0));
  await audioCtx.close();

  const left = decoded.getChannelData(0);
  const right = decoded.numberOfChannels > 1 ? decoded.getChannelData(1) : left;

  return {
    sampleRate: decoded.sampleRate,
    left: new Float32Array(left),
    right: new Float32Array(right)
  };
}

function applyParams() {
  api.setFeedback(Number(els.feedback.value));
  api.setMix(Number(els.mix.value));
  api.setToneHz(Number(els.toneHz.value));
  api.setSmearAmount(Number(els.smearAmount.value));
  api.setTempoBpm(Number(els.tempoBpm.value));
  api.setNoteDivision(Number(els.noteDivision.value));
  api.setReadMode(Number(els.readMode.value));
}

function applyReadModeDefaults() {
  const readMode = Number(els.readMode.value);
  if (readMode !== READ_MODE_ACCIDENTAL_REVERSE) return;
  els.toneHz.value = String(REVERSE_LEGACY_DEFAULTS.toneHz);
  outputs.toneHz.textContent = String(REVERSE_LEGACY_DEFAULTS.toneHz);
}

function processStereoBuffer(left, right) {
  const outL = new Float32Array(left.length);
  const outR = new Float32Array(right.length);

  const bytes = BLOCK_SIZE * Float32Array.BYTES_PER_ELEMENT;
  const ptrInL = module._malloc(bytes);
  const ptrInR = module._malloc(bytes);
  const ptrOutL = module._malloc(bytes);
  const ptrOutR = module._malloc(bytes);

  const inBlockL = new Float32Array(BLOCK_SIZE);
  const inBlockR = new Float32Array(BLOCK_SIZE);

  try {
    api.reset();
    for (let pos = 0; pos < left.length; pos += BLOCK_SIZE) {
      const len = Math.min(BLOCK_SIZE, left.length - pos);
      inBlockL.fill(0);
      inBlockR.fill(0);
      inBlockL.set(left.subarray(pos, pos + len));
      inBlockR.set(right.subarray(pos, pos + len));

      module.HEAPF32.set(inBlockL, ptrInL >> 2);
      module.HEAPF32.set(inBlockR, ptrInR >> 2);

      api.process(ptrInL, ptrInR, ptrOutL, ptrOutR, BLOCK_SIZE);

      outL.set(module.HEAPF32.subarray(ptrOutL >> 2, (ptrOutL >> 2) + len), pos);
      outR.set(module.HEAPF32.subarray(ptrOutR >> 2, (ptrOutR >> 2) + len), pos);
    }
  } finally {
    module._free(ptrInL);
    module._free(ptrInR);
    module._free(ptrOutL);
    module._free(ptrOutR);
  }

  return { outL, outR };
}

els.processBtn.addEventListener('click', async () => {
  const file = els.file.files?.[0];
  if (!file) {
    els.status.textContent = 'Selecione um arquivo antes de processar.';
    return;
  }

  els.processBtn.disabled = true;
  els.downloadLink.hidden = true;
  els.status.textContent = 'Decodificando áudio...';

  try {
    applyParams();
    const { sampleRate, left, right } = await decodeToStereoFloat(file);

    els.status.textContent = 'Processando com Orbit Echo...';
    const { outL, outR } = processStereoBuffer(left, right);

    const wavBlob = encodeWavFromFloatStereo(outL, outR, sampleRate);
    const url = URL.createObjectURL(wavBlob);

    els.preview.src = url;
    els.downloadLink.href = url;
    els.downloadLink.hidden = false;
    els.downloadLink.textContent = 'Baixar WAV processado';
    els.status.textContent = 'Concluído! Ouça no player ou baixe o arquivo.';
  } catch (err) {
    els.status.textContent = `Erro: ${err instanceof Error ? err.message : String(err)}`;
  } finally {
    els.processBtn.disabled = false;
  }
});

els.readMode.addEventListener('change', () => {
  applyReadModeDefaults();
});

initWasm().catch((err) => {
  els.status.textContent = `Falha ao iniciar WASM: ${err instanceof Error ? err.message : String(err)}`;
});

window.addEventListener('beforeunload', () => {
  if (api) api.free();
});
