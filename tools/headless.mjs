// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Javier Sánchez
// Headless runner (prueba sin navegador) for an fm1-base app.wasm: boots it, plays a script, writes screen PNGs (raw->python) and a WAV.
// usage: node headless.mjs app.wasm outdir [--min-peak P]   (outdir/script.json: [kind, a, b] steps)
import fs from 'fs';
const [,, wasmPath, outDir, ...rest] = process.argv;
const minPeak = rest[0] === '--min-peak' ? +rest[1] : 0;
fs.mkdirSync(outDir, { recursive: true });
let mem;
const wasi = new Proxy({
  clock_time_get: (id, p, out) => { new DataView(mem.buffer).setBigUint64(out, 0n, true); return 0; },
  fd_write: (fd, iov, n, out) => { new DataView(mem.buffer).setUint32(out, 0, true); return 0; },
}, { get: (t, k) => (k in t ? t[k] : () => 0) });
const env = new Proxy({}, { get: () => () => 0 });
const { instance } = await WebAssembly.instantiate(fs.readFileSync(wasmPath), { wasi_snapshot_preview1: wasi, env });
const ex = instance.exports; mem = ex.memory;
if (ex._initialize) ex._initialize();
ex.web_master(2800); ex.web_boot();
const audio = [];
function run(ms) {                       // 44.1 frames per ms; render in 128-frame quanta
  let frames = Math.round(ms * 44.1);
  while (frames > 0) {
    const n = Math.min(128, frames);
    ex.web_render(n);
    const l = new Float32Array(mem.buffer, ex.web_out_l(), n), r = new Float32Array(mem.buffer, ex.web_out_r(), n);
    for (let i = 0; i < n; i++) audio.push(l[i], r[i]);
    frames -= n;
  }
}
function shot(name) {
  const fb = new Uint16Array(mem.buffer, ex.web_fb(), 240 * 240).slice();
  fs.writeFileSync(`${outDir}/${name}.rgb565`, Buffer.from(fb.buffer));
}
const script = JSON.parse(fs.readFileSync(`${outDir}/script.json`, 'utf8'));
for (const [kind, a, b] of script) {
  if (kind === 'wait') run(a);
  else if (kind === 'key') ex.web_keys(a >>> 0);
  else if (kind === 'btn') ex.web_buttons(a >>> 0);
  else if (kind === 'enc') ex.web_enc(a, b);
  else if (kind === 'shot') shot(a);
}
const pcm = Buffer.alloc(44 + audio.length * 2);
pcm.write('RIFF', 0); pcm.writeUInt32LE(36 + audio.length * 2, 4); pcm.write('WAVEfmt ', 8);
pcm.writeUInt32LE(16, 16); pcm.writeUInt16LE(1, 20); pcm.writeUInt16LE(2, 22); pcm.writeUInt32LE(44100, 24);
pcm.writeUInt32LE(44100 * 4, 28); pcm.writeUInt16LE(4, 32); pcm.writeUInt16LE(16, 34); pcm.write('data', 36);
pcm.writeUInt32LE(audio.length * 2, 40);
let peak = 0;
audio.forEach((v, i) => { peak = Math.max(peak, Math.abs(v)); pcm.writeInt16LE(Math.max(-32768, Math.min(32767, Math.round(v * 32767))), 44 + i * 2); });
fs.writeFileSync(`${outDir}/audio.wav`, pcm);
console.log(JSON.stringify({ seconds: audio.length / 2 / 44100, peak: +peak.toFixed(3), cpu: ex.web_cpu(), blits: ex.web_blits() }));
if (peak < minPeak) { console.error(`headless: peak ${peak} below ${minPeak}: no sound`); process.exit(1); }
