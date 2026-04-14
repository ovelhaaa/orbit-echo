import createOrbitModule from './orbit_delay_wasm.js';

const BLOCK_SIZE = 128;
const MAX_DELAY_SAMPLES = 48000 * 2;
const TAP_BUFFER_SIZE = 8;
const TAP_MIN_BPM = 20;
const TAP_MAX_BPM = 240;
const TAP_EXPIRE_MS = 4000;
const REPEAT_STORAGE_KEY = 'orbit-echo:transport:repeat-mode';
const PROJECT_REPEAT_PREFIX = 'orbit-echo:transport:repeat-project:';

const PRESETS = {
  A: {
    label: 'Preset A · Reverse Legacy',
    readMode: 1,
    feedback: 0.45,
    mix: 0.45,
    orbit: 0,
    offsetSamples: 0,
    stereoSpread: 0,
    toneHz: 1800,
    smearAmount: 0,
    tempoBpm: 120,
    noteDivision: 1,
    dcBlockEnabled: 1,
    shimmerMode: 0
  },
  B: {
    label: 'Preset B · Orbit Modern',
    readMode: 0,
    feedback: 0.4,
    mix: 0.35,
    orbit: 0.35,
    offsetSamples: 180,
    stereoSpread: 0.5,
    toneHz: 6500,
    smearAmount: 0.25,
    tempoBpm: 120,
    noteDivision: 1,
    dcBlockEnabled: 1,
    shimmerMode: 1
  }
};

const els = {
  file: document.getElementById('audioFile'),
  processBtn: document.getElementById('processBtn'),
  status: document.getElementById('status'),
  preview: document.getElementById('preview'),
  downloadLink: document.getElementById('downloadLink'),
  presetMode: document.getElementById('presetMode'),
  feedback: document.getElementById('feedback'),
  mix: document.getElementById('mix'),
  orbit: document.getElementById('orbit'),
  offsetSamples: document.getElementById('offsetSamples'),
  stereoSpread: document.getElementById('stereoSpread'),
  toneHz: document.getElementById('toneHz'),
  smearAmount: document.getElementById('smearAmount'),
  tempoBpm: document.getElementById('tempoBpm'),
  tapTempoBtn: document.getElementById('tapTempoBtn'),
  resetTapBtn: document.getElementById('resetTapBtn'),
  tapTempoOut: document.getElementById('tapTempoOut'),
  noteDivision: document.getElementById('noteDivision'),
  readMode: document.getElementById('readMode'),
  dcBlockEnabled: document.getElementById('dcBlockEnabled'),
  shimmerMode: document.getElementById('shimmerMode'),
  playPauseBtn: document.getElementById('playPauseBtn'),
  repeatBtn: document.getElementById('repeatBtn'),
  transportState: document.getElementById('transportState')
};

const outputs = {
  presetMode: document.getElementById('presetModeOut'),
  feedback: document.getElementById('feedbackOut'),
  mix: document.getElementById('mixOut'),
  orbit: document.getElementById('orbitOut'),
  offsetSamples: document.getElementById('offsetSamplesOut'),
  stereoSpread: document.getElementById('stereoSpreadOut'),
  toneHz: document.getElementById('toneHzOut'),
  smearAmount: document.getElementById('smearAmountOut'),
  tempoBpm: document.getElementById('tempoBpmOut'),
  noteDivision: document.getElementById('noteDivisionOut'),
  readMode: document.getElementById('readModeOut'),
  dcBlockEnabled: document.getElementById('dcBlockEnabledOut'),
  shimmerMode: document.getElementById('shimmerModeOut')
};

const tapState = {
  timestamps: [],
  lastTapAt: 0
};

const repeatState = {
  mode: 'off',
  loopStart: 0,
  loopEnd: 0,
  projectKey: '',
  monitorId: 0
};

function getProjectRepeatKey() {
  const file = els.file?.files?.[0];
  if (!file) return '';
  return `${PROJECT_REPEAT_PREFIX}${file.name}:${file.size}:${file.lastModified}`;
}

function readStoredRepeatMode() {
  const projectKey = repeatState.projectKey;
  const projectValue = projectKey ? localStorage.getItem(projectKey) : null;
  const globalValue = localStorage.getItem(REPEAT_STORAGE_KEY);
  const value = projectValue ?? globalValue;
  return value === 'on' ? 'on' : 'off';
}

function persistRepeatMode() {
  localStorage.setItem(REPEAT_STORAGE_KEY, repeatState.mode);
  if (repeatState.projectKey) {
    localStorage.setItem(repeatState.projectKey, repeatState.mode);
  }
}

function updateTransportStateLabel() {
  if (!els.transportState) return;
  if (!els.preview.src) {
    els.transportState.textContent = 'Sem áudio carregado.';
    return;
  }

  const repeatLabel = repeatState.mode === 'on' ? 'Repeat On' : 'Repeat Off';
  const now = Number.isFinite(els.preview.currentTime) ? els.preview.currentTime : 0;
  const end = Number.isFinite(repeatState.loopEnd) ? repeatState.loopEnd : 0;
  els.transportState.textContent = `${els.preview.paused ? 'Pause' : 'Play'} · ${repeatLabel} · ${now.toFixed(2)}s / ${end.toFixed(2)}s`;
}

function updateRepeatUi() {
  if (!els.repeatBtn) return;
  const isOn = repeatState.mode === 'on';
  els.repeatBtn.classList.toggle('is-active', isOn);
  els.repeatBtn.setAttribute('aria-pressed', isOn ? 'true' : 'false');
  els.repeatBtn.textContent = isOn ? '⟲ Repeat On' : '⟲ Repeat Off';
  els.repeatBtn.title = isOn
    ? 'Repeat: On (atalho R · A/B Loop futuro)'
    : 'Repeat: Off (atalho R · A/B Loop futuro)';
  updateTransportStateLabel();
}

function updatePlayPauseUi() {
  if (!els.playPauseBtn) return;
  els.playPauseBtn.textContent = els.preview.paused ? '▶ Play' : '⏸ Pause';
  updateTransportStateLabel();
}

function refreshLoopPoints() {
  const duration = Number.isFinite(els.preview.duration) ? els.preview.duration : 0;
  repeatState.loopStart = 0;
  repeatState.loopEnd = duration;
  updateTransportStateLabel();
}

function monitorRepeatLoop() {
  if (repeatState.monitorId) {
    cancelAnimationFrame(repeatState.monitorId);
    repeatState.monitorId = 0;
  }

  const tick = () => {
    if (!els.preview || els.preview.paused || repeatState.mode !== 'on') {
      repeatState.monitorId = 0;
      updateTransportStateLabel();
      return;
    }

    const loopEnd = repeatState.loopEnd;
    if (loopEnd > 0) {
      const epsilon = Math.min(0.03, Math.max(0.005, loopEnd / 40));
      if (els.preview.currentTime >= loopEnd - epsilon) {
        els.preview.currentTime = repeatState.loopStart;
      }
    }

    updateTransportStateLabel();
    repeatState.monitorId = requestAnimationFrame(tick);
  };

  repeatState.monitorId = requestAnimationFrame(tick);
}

function toggleRepeatMode() {
  repeatState.mode = repeatState.mode === 'on' ? 'off' : 'on';
  persistRepeatMode();
  updateRepeatUi();
  if (repeatState.mode === 'on' && !els.preview.paused) {
    monitorRepeatLoop();
  }
}

function initTransport() {
  repeatState.projectKey = getProjectRepeatKey();
  repeatState.mode = readStoredRepeatMode();
  updateRepeatUi();
  updatePlayPauseUi();

  els.playPauseBtn?.addEventListener('click', async () => {
    if (!els.preview.src) return;
    if (els.preview.paused) {
      await els.preview.play();
    } else {
      els.preview.pause();
    }
    updatePlayPauseUi();
  });

  els.repeatBtn?.addEventListener('click', () => {
    if (!els.preview.src) return;
    toggleRepeatMode();
  });

  els.preview.addEventListener('play', () => {
    updatePlayPauseUi();
    if (repeatState.mode === 'on') {
      monitorRepeatLoop();
    }
  });
  els.preview.addEventListener('pause', updatePlayPauseUi);
  els.preview.addEventListener('seeked', () => {
    if (repeatState.mode === 'on' && repeatState.loopEnd > 0 && els.preview.currentTime >= repeatState.loopEnd) {
      els.preview.currentTime = repeatState.loopStart;
    }
    updateTransportStateLabel();
  });
  els.preview.addEventListener('loadedmetadata', () => {
    refreshLoopPoints();
    els.playPauseBtn.disabled = false;
    els.repeatBtn.disabled = false;
    repeatState.projectKey = getProjectRepeatKey();
    repeatState.mode = readStoredRepeatMode();
    updateRepeatUi();
  });
  els.preview.addEventListener('timeupdate', updateTransportStateLabel);
  els.preview.addEventListener('ended', () => {
    if (repeatState.mode === 'on' && repeatState.loopEnd > 0) {
      els.preview.currentTime = repeatState.loopStart;
      els.preview.play().catch(() => {});
      monitorRepeatLoop();
      return;
    }
    updatePlayPauseUi();
  });

  document.addEventListener('keydown', (event) => {
    if (event.repeat) return;
    const key = event.key.toLowerCase();
    if (key !== 'r') return;
    event.preventDefault();
    if (!els.preview.src) return;
    toggleRepeatMode();
  });
}

function clamp(value, min, max) {
  return Math.min(max, Math.max(min, value));
}

function clearTapHistory() {
  tapState.timestamps = [];
  tapState.lastTapAt = 0;
  if (els.tapTempoOut) els.tapTempoOut.textContent = 'Tempo detectado: -- BPM';
}

function updateTapTempoOut() {
  if (!els.tapTempoOut) return;
  const bpm = Math.round(Number(els.tempoBpm.value));
  els.tapTempoOut.textContent = `Tempo detectado: ${bpm} BPM`;
}

function setTempoBpmFromTap(bpm) {
  const clampedBpm = clamp(Math.round(bpm), TAP_MIN_BPM, TAP_MAX_BPM);
  els.tempoBpm.value = String(clampedBpm);
  renderOutput('tempoBpm');
  if (api) api.setTempoBpm(clampedBpm);
  updateTapTempoOut();
}

function median(values) {
  if (values.length === 0) return 0;
  const sorted = [...values].sort((a, b) => a - b);
  const mid = Math.floor(sorted.length / 2);
  if (sorted.length % 2 === 0) return (sorted[mid - 1] + sorted[mid]) / 2;
  return sorted[mid];
}

function handleTapTempo() {
  const now = performance.now();
  if (tapState.lastTapAt && now - tapState.lastTapAt > TAP_EXPIRE_MS) {
    clearTapHistory();
  }

  tapState.lastTapAt = now;
  tapState.timestamps.push(now);
  if (tapState.timestamps.length > TAP_BUFFER_SIZE) {
    tapState.timestamps.shift();
  }

  if (tapState.timestamps.length < 2) return;

  const intervals = [];
  for (let i = 1; i < tapState.timestamps.length; i += 1) {
    intervals.push(tapState.timestamps[i] - tapState.timestamps[i - 1]);
  }

  const intervalMs = median(intervals);
  if (intervalMs <= 0) return;

  const bpm = 60000 / intervalMs;
  setTempoBpmFromTap(bpm);
}

function renderOutput(key) {
  const input = els[key];
  const output = outputs[key];
  if (!input || !output) return;

  if (['toneHz', 'tempoBpm', 'offsetSamples'].includes(key)) {
    output.textContent = String(Math.round(Number(input.value)));
    return;
  }

  if (['noteDivision', 'readMode', 'shimmerMode', 'dcBlockEnabled', 'presetMode'].includes(key)) {
    const label = input.options[input.selectedIndex]?.textContent;
    output.textContent = label || input.value;
    return;
  }

  output.textContent = Number(input.value).toFixed(2);
}

function bindOutputs() {
  for (const key of Object.keys(outputs)) {
    const input = els[key];
    if (!input) continue;
    const render = () => renderOutput(key);
    input.addEventListener('input', render);
    if (input.tagName === 'SELECT') input.addEventListener('change', render);
    render();
  }
}

function applyPreset(presetId) {
  const preset = PRESETS[presetId] || PRESETS.A;
  Object.entries(preset).forEach(([key, value]) => {
    if (key === 'label') return;
    if (els[key]) els[key].value = String(value);
  });
  Object.keys(outputs).forEach(renderOutput);
  updateTapTempoOut();
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
    setOrbit: module.cwrap('orbit_wasm_set_orbit', 'number', ['number']),
    setOffsetSamples: module.cwrap('orbit_wasm_set_offset_samples', 'number', ['number']),
    setTempoBpm: module.cwrap('orbit_wasm_set_tempo_bpm', 'number', ['number']),
    setNoteDivision: module.cwrap('orbit_wasm_set_note_division', 'number', ['number']),
    setStereoSpread: module.cwrap('orbit_wasm_set_stereo_spread', 'number', ['number']),
    setFeedback: module.cwrap('orbit_wasm_set_feedback', 'number', ['number']),
    setMix: module.cwrap('orbit_wasm_set_mix', 'number', ['number']),
    setToneHz: module.cwrap('orbit_wasm_set_tone_hz', 'number', ['number']),
    setSmearAmount: module.cwrap('orbit_wasm_set_smear_amount', 'number', ['number']),
    setShimmerMode: module.cwrap('orbit_wasm_set_shimmer_mode', 'number', ['number']),
    setDcBlockEnabled: module.cwrap('orbit_wasm_set_dc_block_enabled', 'number', ['number']),
    setReadMode: module.cwrap('orbit_wasm_set_read_mode', 'number', ['number'])
  };

  const ok = api.init(48000, MAX_DELAY_SAMPLES);
  if (!ok) throw new Error('orbit_wasm_init falhou');

  applyParams();
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
  api.setOrbit(Number(els.orbit.value));
  api.setOffsetSamples(Number(els.offsetSamples.value));
  api.setTempoBpm(Number(els.tempoBpm.value));
  api.setNoteDivision(Number(els.noteDivision.value));
  api.setStereoSpread(Number(els.stereoSpread.value));
  api.setFeedback(Number(els.feedback.value));
  api.setMix(Number(els.mix.value));
  api.setToneHz(Number(els.toneHz.value));
  api.setSmearAmount(Number(els.smearAmount.value));
  api.setShimmerMode(Number(els.shimmerMode.value));
  api.setDcBlockEnabled(Number(els.dcBlockEnabled.value));
  api.setReadMode(Number(els.readMode.value));
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
    applyParams();

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
    const { sampleRate, left, right } = await decodeToStereoFloat(file);

    els.status.textContent = 'Processando com Orbit Echo...';
    const { outL, outR } = processStereoBuffer(left, right);

    const wavBlob = encodeWavFromFloatStereo(outL, outR, sampleRate);
    const url = URL.createObjectURL(wavBlob);

    els.preview.src = url;
    els.preview.currentTime = 0;
    els.downloadLink.href = url;
    els.downloadLink.hidden = false;
    refreshLoopPoints();
    els.downloadLink.textContent = 'Baixar WAV processado';
    els.status.textContent = 'Concluído! Ouça no player ou baixe o arquivo.';
  } catch (err) {
    els.status.textContent = `Erro: ${err instanceof Error ? err.message : String(err)}`;
  } finally {
    els.processBtn.disabled = false;
  }
});

els.presetMode.addEventListener('change', () => {
  applyPreset(els.presetMode.value);
  if (api) applyParams();
});

bindOutputs();
applyPreset('A');
initTransport();

els.tapTempoBtn?.addEventListener('click', handleTapTempo);
els.resetTapBtn?.addEventListener('click', clearTapHistory);
els.tempoBpm.addEventListener('input', () => {
  updateTapTempoOut();
  refreshLoopPoints();
});

initWasm().catch((err) => {
  els.status.textContent = `Falha ao iniciar WASM: ${err instanceof Error ? err.message : String(err)}`;
});

window.addEventListener('beforeunload', () => {
  if (api) api.free();
});
