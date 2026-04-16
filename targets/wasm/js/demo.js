import createOrbitModule from './orbit_delay_wasm.js';

const BLOCK_SIZE = 128;
const MAX_DELAY_SAMPLES = 48000 * 2;
const DEFAULT_UI_SAMPLE_RATE = 48000;
const TONE_MIN_HZ = 300;
const TONE_MAX_HZ = 12000;
const TAP_BUFFER_SIZE = 8;
const TAP_MIN_BPM = 20;
const TAP_MAX_BPM = 240;
const TAP_EXPIRE_MS = 4000;
const BYPASS_XFADE_SECONDS = 0.03;
const REPEAT_STORAGE_KEY = 'orbit-echo:transport:repeat-mode';
const PROJECT_REPEAT_PREFIX = 'orbit-echo:transport:repeat-project:';
const PRESET_SCHEMA_VERSION = 1;
const PARAM_KEYS = [
  'orbit',
  'offsetSamples',
  'tempoBpm',
  'noteDivision',
  'stereoSpread',
  'feedback',
  'mix',
  'inputGain',
  'outputGain',
  'toneHz',
  'smearAmount',
  'shimmerMode',
  'dcBlockEnabled',
  'readMode'
];

const PRESETS = {
  A: {
    label: 'Preset A · Reverse Legacy',
    readMode: 1,
    feedback: 0.45,
    mix: 0.45,
    orbit: 0.25,
    offsetSamples: 0.0,
    stereoSpread: 0.0,
    inputGain: 0.0,
    outputGain: 0.0,
    toneHz: 1800.0,
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
    offsetSamples: 3.8,
    stereoSpread: 12.0,
    inputGain: 0.0,
    outputGain: -1.5,
    toneHz: 6500.0,
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
  presetJsonFile: document.getElementById('presetJsonFile'),
  applyPresetJsonBtn: document.getElementById('applyPresetJsonBtn'),
  presetImportStatus: document.getElementById('presetImportStatus'),
  feedback: document.getElementById('feedback'),
  mix: document.getElementById('mix'),
  orbit: document.getElementById('orbit'),
  offsetSamples: document.getElementById('offsetSamples'),
  stereoSpread: document.getElementById('stereoSpread'),
  inputGain: document.getElementById('inputGain'),
  outputGain: document.getElementById('outputGain'),
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
  bypassBtn: document.getElementById('bypassBtn'),
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
  inputGain: document.getElementById('inputGainOut'),
  outputGain: document.getElementById('outputGainOut'),
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

const bypassState = {
  enabled: false
};

const previewGraph = {
  audioCtx: null,
  dryElement: null,
    dryGain: null,
  wetGain: null
};

const previewUrls = {
  dry: '',
  wet: ''
};

let uiSampleRate = DEFAULT_UI_SAMPLE_RATE;

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
  const fxLabel = bypassState.enabled ? 'FX OFF (Bypass)' : 'FX ON';
  const now = Number.isFinite(els.preview.currentTime) ? els.preview.currentTime : 0;
  const end = Number.isFinite(repeatState.loopEnd) ? repeatState.loopEnd : 0;
  els.transportState.textContent = `${els.preview.paused ? 'Pause' : 'Play'} · ${fxLabel} · ${repeatLabel} · ${now.toFixed(2)}s / ${end.toFixed(2)}s`;
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

function updateBypassUi() {
  if (!els.bypassBtn) return;
  const isBypassed = bypassState.enabled;
  els.bypassBtn.classList.toggle('is-bypass', isBypassed);
  els.bypassBtn.classList.toggle('is-active', !isBypassed);
  els.bypassBtn.setAttribute('aria-pressed', isBypassed ? 'true' : 'false');
  els.bypassBtn.textContent = isBypassed ? 'FX OFF' : 'FX ON';
  els.bypassBtn.title = isBypassed ? 'Bypass global: ligado' : 'Bypass global: desligado';
  updateTransportStateLabel();
}


const realtimeState = {
  isOfflineProcessing: false,
  ptrInL: 0,
  ptrInR: 0,
  ptrOutL: 0,
  ptrOutR: 0,
  inBlockL: null,
  inBlockR: null
};

async function ensurePreviewGraph() {
  if (previewGraph.audioCtx) return;
  const audioCtx = new AudioContext({ sampleRate: DEFAULT_UI_SAMPLE_RATE });
  uiSampleRate = audioCtx.sampleRate;

  const drySource = audioCtx.createMediaElementSource(els.preview);
  const dryGain = audioCtx.createGain();
  const wetGain = audioCtx.createGain();

  dryGain.gain.value = 0;
  wetGain.gain.value = 1;

  const scriptNode = audioCtx.createScriptProcessor(BLOCK_SIZE, 2, 2);

  const bytes = BLOCK_SIZE * 4; // Float32Array.BYTES_PER_ELEMENT
  realtimeState.ptrInL = module._malloc(bytes);
  realtimeState.ptrInR = module._malloc(bytes);
  realtimeState.ptrOutL = module._malloc(bytes);
  realtimeState.ptrOutR = module._malloc(bytes);
  realtimeState.inBlockL = new Float32Array(BLOCK_SIZE);
  realtimeState.inBlockR = new Float32Array(BLOCK_SIZE);

  if (api) {
    api.reset(uiSampleRate);
    applyParams();
  }

  scriptNode.onaudioprocess = (e) => {
    const inputBuffer = e.inputBuffer;
    const outputBuffer = e.outputBuffer;

    if (realtimeState.isOfflineProcessing || !api) {
      for (let channel = 0; channel < outputBuffer.numberOfChannels; channel++) {
        const inputData = inputBuffer.getChannelData(channel);
        const outputData = outputBuffer.getChannelData(channel);
        outputData.set(inputData);
      }
      return;
    }

    const numSamples = inputBuffer.length;
    // ensure size
    if (numSamples !== BLOCK_SIZE) {
       console.warn("ScriptProcessorNode delivered an unexpected buffer size: " + numSamples);
       // we only process BLOCK_SIZE due to static allocation, we truncate or bypass for this simple real time implementation
       if (numSamples > BLOCK_SIZE) {
           console.error("Buffer size too big, bypassing");
            for (let channel = 0; channel < outputBuffer.numberOfChannels; channel++) {
                outputBuffer.getChannelData(channel).set(inputBuffer.getChannelData(channel));
            }
            return;
       }
    }

    const processLen = Math.min(numSamples, BLOCK_SIZE);

    const inL = inputBuffer.getChannelData(0);
    const inR = inputBuffer.numberOfChannels > 1 ? inputBuffer.getChannelData(1) : inL;
    const outL = outputBuffer.getChannelData(0);
    const outR = outputBuffer.numberOfChannels > 1 ? outputBuffer.getChannelData(1) : outL;

    realtimeState.inBlockL.fill(0);
    realtimeState.inBlockR.fill(0);
    realtimeState.inBlockL.set(inL.subarray(0, processLen));
    realtimeState.inBlockR.set(inR.subarray(0, processLen));

    module.HEAPF32.set(realtimeState.inBlockL, realtimeState.ptrInL >> 2);
    module.HEAPF32.set(realtimeState.inBlockR, realtimeState.ptrInR >> 2);

    api.process(realtimeState.ptrInL, realtimeState.ptrInR, realtimeState.ptrOutL, realtimeState.ptrOutR, processLen);

    outL.set(module.HEAPF32.subarray(realtimeState.ptrOutL >> 2, (realtimeState.ptrOutL >> 2) + processLen));
    if (outputBuffer.numberOfChannels > 1) {
      outR.set(module.HEAPF32.subarray(realtimeState.ptrOutR >> 2, (realtimeState.ptrOutR >> 2) + processLen));
    }  };

  drySource.connect(dryGain).connect(audioCtx.destination);
  drySource.connect(scriptNode).connect(wetGain).connect(audioCtx.destination);

  previewGraph.audioCtx = audioCtx;
  previewGraph.dryElement = els.preview;

  previewGraph.dryGain = dryGain;
  previewGraph.wetGain = wetGain;
  previewGraph.scriptNode = scriptNode;
}


function applyBypassCrossfade() {
  if (!previewGraph.audioCtx || !previewGraph.dryGain || !previewGraph.wetGain) return;
  const now = previewGraph.audioCtx.currentTime;
  const dryTarget = bypassState.enabled ? 1 : 0;
  const wetTarget = bypassState.enabled ? 0 : 1;
  previewGraph.dryGain.gain.cancelScheduledValues(now);
  previewGraph.wetGain.gain.cancelScheduledValues(now);
  previewGraph.dryGain.gain.setValueAtTime(previewGraph.dryGain.gain.value, now);
  previewGraph.wetGain.gain.setValueAtTime(previewGraph.wetGain.gain.value, now);
  previewGraph.dryGain.gain.linearRampToValueAtTime(dryTarget, now + BYPASS_XFADE_SECONDS);
  previewGraph.wetGain.gain.linearRampToValueAtTime(wetTarget, now + BYPASS_XFADE_SECONDS);
}


async function toggleBypassMode() {
  if (!els.preview.src) return;
  await ensurePreviewGraph();
  if (previewGraph.audioCtx?.state === 'suspended') {
    await previewGraph.audioCtx.resume();
  }
  bypassState.enabled = !bypassState.enabled;
  applyBypassCrossfade();
  updateBypassUi();
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
  updateBypassUi();

  els.playPauseBtn?.addEventListener('click', async () => {
    if (!els.preview.src) return;
    await ensurePreviewGraph();
    if (previewGraph.audioCtx?.state === 'suspended') {
      await previewGraph.audioCtx.resume();
    }
    if (els.preview.paused) {


      await els.preview.play();
    } else {

      els.preview.pause();
    }
    updatePlayPauseUi();
  });

  els.bypassBtn?.addEventListener('click', async () => {
    if (!els.preview.src) return;
    await toggleBypassMode();
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
  els.preview.addEventListener('pause', () => {

    updatePlayPauseUi();
  });
  els.preview.addEventListener('seeked', () => {

    if (repeatState.mode === 'on' && repeatState.loopEnd > 0 && els.preview.currentTime >= repeatState.loopEnd) {
      els.preview.currentTime = repeatState.loopStart;
    }
    updateTransportStateLabel();
  });
  els.preview.addEventListener('loadedmetadata', () => {
    refreshLoopPoints();
    els.playPauseBtn.disabled = false;
    if (els.bypassBtn) els.bypassBtn.disabled = false;
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

  document.addEventListener('keydown', (event) => {
    if (event.repeat) return;
    const key = event.key.toLowerCase();
    if (key !== 'b') return;
    event.preventDefault();
    if (!els.preview.src) return;
    toggleBypassMode().catch(() => {});
  });
}

function clamp(value, min, max) {
  return Math.min(max, Math.max(min, value));
}

function gainDbToLinear(db) {
  return 10 ** (db / 20);
}

function msToSamples(ms) {
  return ms * (uiSampleRate / 1000);
}

function hzToToneNorm(hz) {
  const clamped = clamp(hz, TONE_MIN_HZ, TONE_MAX_HZ);
  return Math.log(clamped / TONE_MIN_HZ) / Math.log(TONE_MAX_HZ / TONE_MIN_HZ);
}

function toneNormToHz(norm) {
  const clamped = clamp(norm, 0, 1);
  return TONE_MIN_HZ * (TONE_MAX_HZ / TONE_MIN_HZ) ** clamped;
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

  if (key === 'toneHz') {
    output.textContent = `${Math.round(toneNormToHz(Number(input.value)))} Hz`;
    return;
  }

  if (key === 'offsetSamples' || key === 'stereoSpread') {
    output.textContent = `${Number(input.value).toFixed(1)} ms`;
    return;
  }

  if (key === 'inputGain' || key === 'outputGain') {
    output.textContent = `${Number(input.value).toFixed(1)} dB`;
    return;
  }

  if (['tempoBpm'].includes(key)) {
    output.textContent = String(Math.round(Number(input.value)));
    return;
  }

  if (['noteDivision', 'readMode', 'shimmerMode', 'dcBlockEnabled', 'presetMode'].includes(key)) {
    const label = input.options[input.selectedIndex]?.textContent;
    output.textContent = label || input.value;
    return;
  }

  if (key === 'orbit' || key === 'feedback' || key === 'mix' || key === 'smearAmount') {
    output.textContent = Number(input.value).toFixed(3);
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
    if (key === 'toneHz') {
      els[key].value = String(hzToToneNorm(Number(value)));
      return;
    }
    if (els[key]) els[key].value = String(value);
  });
  Object.keys(outputs).forEach(renderOutput);
  updateTapTempoOut();
}

let module;
let api;

const paramConverters = {
  orbit: (v) => Number(v),
  offsetSamples: (v) => msToSamples(Number(v)),
  tempoBpm: (v) => Number(v),
  noteDivision: (v) => Number(v),
  stereoSpread: (v) => msToSamples(Number(v)),
  feedback: (v) => Number(v),
  mix: (v) => Number(v),
  inputGain: (v) => gainDbToLinear(Number(v)),
  outputGain: (v) => gainDbToLinear(Number(v)),
  toneHz: (v) => toneNormToHz(Number(v)),
  smearAmount: (v) => Number(v),
  shimmerMode: (v) => Number(v),
  dcBlockEnabled: (v) => Number(v),
  readMode: (v) => Number(v)
};

const presetValidators = {
  orbit: (v) => expectNumberRange(v, 0, 1),
  offsetSamples: (v) => expectNumberRange(v, -500, 500),
  tempoBpm: (v) => expectNumberRange(v, TAP_MIN_BPM, TAP_MAX_BPM),
  noteDivision: (v) => expectOneOf(v, [0.25, 0.5, 0.75, 1, 1.5, 2, 3]),
  stereoSpread: (v) => expectNumberRange(v, 0, 100),
  feedback: (v) => expectNumberRange(v, 0, 0.95),
  mix: (v) => expectNumberRange(v, 0, 1),
  inputGain: (v) => expectNumberRange(v, -24, 12),
  outputGain: (v) => expectNumberRange(v, -24, 12),
  toneHz: (v) => expectNumberRange(v, TONE_MIN_HZ, TONE_MAX_HZ),
  smearAmount: (v) => expectNumberRange(v, 0, 1),
  shimmerMode: (v) => expectOneOf(v, [0, 1]),
  dcBlockEnabled: (v) => expectOneOf(v, [0, 1]),
  readMode: (v) => expectOneOf(v, [0, 1])
};

function setPresetImportStatus(message, type = 'info') {
  if (!els.presetImportStatus) return;
  const prefix = type === 'error' ? '❌ ' : type === 'warn' ? '⚠️ ' : '✅ ';
  els.presetImportStatus.textContent = `${prefix}${message}`;
}

function expectNumber(value) {
  const n = Number(value);
  if (!Number.isFinite(n)) return null;
  return n;
}

function expectNumberRange(value, min, max) {
  const n = expectNumber(value);
  if (n === null || n < min || n > max) return null;
  return n;
}

function expectOneOf(value, accepted) {
  const n = expectNumber(value);
  if (n === null || !accepted.includes(n)) return null;
  return n;
}

function normalizePresetJson(raw) {
  if (!raw || typeof raw !== 'object' || Array.isArray(raw)) {
    throw new Error('Formato inválido: o preset deve ser um objeto JSON.');
  }

  const schemaVersion = raw.schemaVersion ?? 1;
  if (schemaVersion !== PRESET_SCHEMA_VERSION) {
    throw new Error(`Incompatibilidade de schema: recebido v${schemaVersion}, esperado v${PRESET_SCHEMA_VERSION}.`);
  }

  const sourceParams = raw.params && typeof raw.params === 'object' ? raw.params : raw;
  const nextValues = {};
  const warnings = [];
  const errors = [];

  Object.entries(sourceParams).forEach(([key, value]) => {
    if (key === 'label' || key === 'schemaVersion' || key === 'name') return;
    if (!PARAM_KEYS.includes(key)) {
      warnings.push(`Parâmetro desconhecido "${key}" foi ignorado.`);
      return;
    }

    const validator = presetValidators[key];
    const valid = validator ? validator(value) : null;
    if (valid === null) {
      errors.push(`"${key}" inválido (${JSON.stringify(value)}).`);
      return;
    }
    nextValues[key] = valid;
  });

  if (Object.keys(nextValues).length === 0) {
    errors.push('Nenhum parâmetro válido encontrado para aplicar.');
  }

  return { nextValues, warnings, errors };
}

function hasPresetOverwrite(nextValues) {
  return Object.entries(nextValues).some(([key, value]) => {
    const control = els[key];
    if (!control) return false;
    const current = Number(control.value);
    if (!Number.isFinite(current)) return true;
    const target = key === 'toneHz' ? hzToToneNorm(Number(value)) : Number(value);
    return Math.abs(current - target) > 1e-9;
  });
}

function applyNormalizedPresetValues(nextValues) {
  Object.entries(nextValues).forEach(([key, value]) => {
    const control = els[key];
    if (!control) return;
    if (key === 'toneHz') {
      control.value = String(hzToToneNorm(Number(value)));
      return;
    }
    control.value = String(value);
  });
  Object.keys(outputs).forEach(renderOutput);
  updateTapTempoOut();
  refreshLoopPoints();
  if (api) applyParams();
}

function readFileAsText(file) {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onerror = () => reject(reader.error);
    reader.onload = () => resolve(String(reader.result || ''));
    reader.readAsText(file, 'utf-8');
  });
}

async function handlePresetJsonImport() {
  const file = els.presetJsonFile?.files?.[0];
  if (!file) {
    setPresetImportStatus('Selecione um arquivo .json antes de aplicar.', 'error');
    return;
  }

  if (!file.name.toLowerCase().endsWith('.json')) {
    setPresetImportStatus('Arquivo inválido: use extensão .json.', 'error');
    return;
  }

  try {
    const text = await readFileAsText(file);
    const parsed = JSON.parse(text);
    const { nextValues, warnings, errors } = normalizePresetJson(parsed);
    if (errors.length > 0) {
      setPresetImportStatus(`Preset rejeitado: ${errors.join(' ')}`, 'error');
      return;
    }

    if (hasPresetOverwrite(nextValues)) {
      const confirmed = window.confirm('Este preset vai sobrescrever os parâmetros atuais. Deseja continuar?');
      if (!confirmed) {
        setPresetImportStatus('Aplicação cancelada pelo usuário.', 'warn');
        return;
      }
    }

    applyNormalizedPresetValues(nextValues);
    if (warnings.length > 0) {
      setPresetImportStatus(`Preset aplicado com avisos: ${warnings.join(' ')}`, 'warn');
      return;
    }
    setPresetImportStatus('Preset aplicado com sucesso.');
  } catch (err) {
    if (err instanceof SyntaxError) {
      setPresetImportStatus('JSON inválido: verifique a sintaxe do arquivo.', 'error');
      return;
    }
    setPresetImportStatus(`Falha ao aplicar preset: ${err instanceof Error ? err.message : String(err)}`, 'error');
  }
}

async function initWasm() {
  module = await createOrbitModule();
  api = {
    init: module.cwrap('orbit_wasm_init', 'number', ['number', 'number']),
    free: module.cwrap('orbit_wasm_free', null, []),
    process: module.cwrap('orbit_wasm_process_stereo', 'number', ['number', 'number', 'number', 'number', 'number']),
    reset: module.cwrap('orbit_wasm_reset', null, ['number']),
    setOrbit: module.cwrap('orbit_wasm_set_orbit', 'number', ['number']),
    setOffsetSamples: module.cwrap('orbit_wasm_set_offset_samples', 'number', ['number']),
    setTempoBpm: module.cwrap('orbit_wasm_set_tempo_bpm', 'number', ['number']),
    setNoteDivision: module.cwrap('orbit_wasm_set_note_division', 'number', ['number']),
    setStereoSpread: module.cwrap('orbit_wasm_set_stereo_spread', 'number', ['number']),
    setFeedback: module.cwrap('orbit_wasm_set_feedback', 'number', ['number']),
    setMix: module.cwrap('orbit_wasm_set_mix', 'number', ['number']),
    setInputGain: module.cwrap('orbit_wasm_set_input_gain', 'number', ['number']),
    setOutputGain: module.cwrap('orbit_wasm_set_output_gain', 'number', ['number']),
    setToneHz: module.cwrap('orbit_wasm_set_tone_hz', 'number', ['number']),
    setSmearAmount: module.cwrap('orbit_wasm_set_smear_amount', 'number', ['number']),
    setShimmerMode: module.cwrap('orbit_wasm_set_shimmer_mode', 'number', ['number']),
    setDcBlockEnabled: module.cwrap('orbit_wasm_set_dc_block_enabled', 'number', ['number']),
    setReadMode: module.cwrap('orbit_wasm_set_read_mode', 'number', ['number'])
  };

  const ok = api.init(48000, MAX_DELAY_SAMPLES);
  if (!ok) throw new Error('orbit_wasm_init falhou');
  uiSampleRate = DEFAULT_UI_SAMPLE_RATE;

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

function setParam(key) {
  if (!api) return;
  const el = els[key];
  const converter = paramConverters[key];
  if (!el || !converter) return;

  const value = converter(el.value);
  switch (key) {
    case 'orbit': api.setOrbit(value); break;
    case 'offsetSamples': api.setOffsetSamples(value); break;
    case 'tempoBpm': api.setTempoBpm(value); break;
    case 'noteDivision': api.setNoteDivision(value); break;
    case 'stereoSpread': api.setStereoSpread(value); break;
    case 'feedback': api.setFeedback(value); break;
    case 'mix': api.setMix(value); break;
    case 'inputGain': api.setInputGain(value); break;
    case 'outputGain': api.setOutputGain(value); break;
    case 'toneHz': api.setToneHz(value); break;
    case 'smearAmount': api.setSmearAmount(value); break;
    case 'shimmerMode': api.setShimmerMode(value); break;
    case 'dcBlockEnabled': api.setDcBlockEnabled(value); break;
    case 'readMode': api.setReadMode(value); break;
    default: break;
  }
}

function applyParams() {
  Object.keys(paramConverters).forEach(setParam);
}


function bindRealtimeParamUpdates() {
  const pendingKeys = new Set();
  let rafScheduled = false;

  const flushPending = () => {
    rafScheduled = false;
    pendingKeys.forEach((key) => setParam(key));
    pendingKeys.clear();
  };

  const scheduleParamUpdate = (key) => {
    pendingKeys.add(key);
    if (rafScheduled) return;
    rafScheduled = true;
    requestAnimationFrame(flushPending);
  };

  for (const key of PARAM_KEYS) {
    const el = els[key];
    if (!el) continue;
    const evt = el.tagName === 'SELECT' ? 'change' : 'input';
    el.addEventListener(evt, () => {
      scheduleParamUpdate(key);
    });
  }
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

  if (typeof realtimeState !== 'undefined') {
    realtimeState.isOfflineProcessing = true;
  }

  try {
    const { sampleRate, left, right } = await decodeToStereoFloat(file);
    const offlineUiSampleRate = sampleRate;
    if (api) {
      api.reset(offlineUiSampleRate);
      applyParams();
    }

    els.status.textContent = 'Processando com Orbit Echo...';

    // We run offline processing exactly like it was
    const outL = new Float32Array(left.length);
    const outR = new Float32Array(right.length);

    const bytes = BLOCK_SIZE * 4;
    const ptrInL = module._malloc(bytes);
    const ptrInR = module._malloc(bytes);
    const ptrOutL = module._malloc(bytes);
    const ptrOutR = module._malloc(bytes);

    const inBlockL = new Float32Array(BLOCK_SIZE);
    const inBlockR = new Float32Array(BLOCK_SIZE);

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

    module._free(ptrInL);
    module._free(ptrInR);
    module._free(ptrOutL);
    module._free(ptrOutR);

    // Re-sync realtime
    if (api && previewGraph.audioCtx) {
      api.reset(previewGraph.audioCtx.sampleRate);
      applyParams();
    }

    const dryWavBlob = encodeWavFromFloatStereo(left, right, sampleRate);
    const wavBlob = encodeWavFromFloatStereo(outL, outR, sampleRate);
    const dryUrl = URL.createObjectURL(dryWavBlob);
    const wetUrl = URL.createObjectURL(wavBlob);
    if (previewUrls.dry) URL.revokeObjectURL(previewUrls.dry);
    if (previewUrls.wet) URL.revokeObjectURL(previewUrls.wet);
    previewUrls.dry = dryUrl;
    previewUrls.wet = wetUrl;

    await ensurePreviewGraph();
    els.preview.src = dryUrl;
    els.preview.currentTime = 0;

    applyBypassCrossfade();
    if (els.bypassBtn) els.bypassBtn.disabled = false;
    updateBypassUi();

    els.downloadLink.href = wetUrl;
    els.downloadLink.hidden = false;
    refreshLoopPoints();
    els.downloadLink.textContent = 'Baixar WAV processado';
    els.status.textContent = 'Concluído! Ouça no player em tempo real ou baixe o arquivo processado.';
  } catch (err) {
    els.status.textContent = `Erro: ${err instanceof Error ? err.message : String(err)}`;
  } finally {
    if (typeof realtimeState !== 'undefined') {
      realtimeState.isOfflineProcessing = false;
    }
    els.processBtn.disabled = false;
  }
});

els.presetMode.addEventListener('change', () => {
  applyPreset(els.presetMode.value);
  if (api) applyParams();
});
els.applyPresetJsonBtn?.addEventListener('click', () => {
  handlePresetJsonImport();
});

bindOutputs();
bindRealtimeParamUpdates();
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
  if (previewUrls.dry) URL.revokeObjectURL(previewUrls.dry);
  if (previewUrls.wet) URL.revokeObjectURL(previewUrls.wet);
});