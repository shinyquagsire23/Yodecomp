#!/usr/bin/env node
// GOAL 4 — the WASM browser oracle (puppeteer-core + system Chrome). Boots yoda.html, clicks
// through the title, screenshots the canvas, and reports audio-graph + canvas-pixel stats, so
// a headless session can verify the browser build without eyes. See CLAUDE.md "WASM debugging".
//
// Setup (once):   npm install puppeteer-core            (uses the installed Chrome — no download)
// Serve:          (cd build-wasm && python3 -m http.server 8777 &)
// Run:            node tools/wasm_boottest.js [url] [shotPrefix]
//   defaults:     url=http://localhost:8777/yoda.html  shotPrefix=wasmboot
// Exit 0 = canvas painted + audio context running. Screenshots: <prefix>{0,1,2}.png.
//
// Picker builds (-DYODA_WASM_PRELOAD=OFF): pass the game folder as a 3rd arg to drive the
// <input webkitdirectory> (CDP uploads a directory fine), e.g.
//   node tools/wasm_boottest.js http://localhost:8778/yoda.html pick /path/to/YodaFull

// resolve puppeteer-core from the CWD too (npm install puppeteer-core in any scratch dir and
// run this script from there — node otherwise only searches relative to tools/)
let puppeteer;
try { puppeteer = require('puppeteer-core'); }
catch (e) {
  puppeteer = require('module').createRequire(process.cwd() + '/')('puppeteer-core');
}

const CHROME = process.env.CHROME_PATH ||
  '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';
const url = process.argv[2] || 'http://localhost:8777/yoda.html';
const pfx = process.argv[3] || 'wasmboot';
const assetDir = process.argv[4] || null;

(async () => {
  const browser = await puppeteer.launch({
    executablePath: CHROME,
    headless: 'new',
    args: ['--no-sandbox', '--autoplay-policy=no-user-gesture-required',
           '--window-size=1400,1100'],
  });
  const page = await browser.newPage();
  await page.setViewport({ width: 1360, height: 1060 });

  // audio-graph probe: SDL3's emscripten driver creates an AudioContext + processor node —
  // state 'running' proves the game got a real sound session (not the null-backend mute).
  await page.evaluateOnNewDocument(() => {
    window.__audio = { contexts: 0 };
    const AC = window.AudioContext || window.webkitAudioContext;
    window.AudioContext = class extends AC {
      constructor(...a) { super(...a); window.__audio.contexts++; window.__audio.ctx = this; }
    };
  });

  const logs = [];
  page.on('console', m => logs.push(`[console.${m.type()}] ${m.text()}`));
  page.on('pageerror', e => logs.push(`[pageerror] ${e.message}`));

  await page.goto(url, { waitUntil: 'load', timeout: 60000 });

  if (assetDir) {                      // picker build: feed the directory input
    await new Promise(r => setTimeout(r, 2000));
    const input = await page.$('input[type=file]');
    if (!input) { console.error('picker input not found'); process.exit(2); }
    await input.uploadFile(assetDir);
  }

  await new Promise(r => setTimeout(r, 12000));                 // boot + title
  const canvas = await page.$('#canvas');
  if (!canvas) { console.error('no #canvas'); process.exit(2); }
  await canvas.screenshot({ path: `${pfx}0.png` });

  const box = await canvas.boundingBox();
  await page.mouse.click(box.x + box.width * 0.29, box.y + box.height * 0.5); // title → new world
  await new Promise(r => setTimeout(r, 15000));                 // worldgen + zone entry
  await canvas.screenshot({ path: `${pfx}1.png` });
  await page.keyboard.press('Space');                           // dismiss the opening bubble
  await new Promise(r => setTimeout(r, 800));
  for (let i = 0; i < 4; i++) {
    await page.keyboard.down('ArrowRight'); await new Promise(r => setTimeout(r, 260));
    await page.keyboard.up('ArrowRight');   await new Promise(r => setTimeout(r, 140));
  }
  await canvas.screenshot({ path: `${pfx}2.png` });

  const stats = await page.evaluate(() => {
    const c = document.getElementById('canvas');
    const t = document.createElement('canvas');
    t.width = c.width; t.height = c.height;
    const x = t.getContext('2d');
    x.drawImage(c, 0, 0);
    const d = x.getImageData(0, 0, t.width, t.height).data;
    let nz = 0;
    for (let i = 0; i < d.length; i += 4)
      if (d[i] | d[i + 1] | d[i + 2]) nz++;
    const a = window.__audio;
    return { w: c.width, h: c.height, nonblank: nz,
             audioContexts: a.contexts, audioState: a.ctx ? a.ctx.state : 'none' };
  });

  console.log(logs.filter(l => !l.includes('404')).slice(-10).join('\n'));
  console.log('STATS:', JSON.stringify(stats));
  await browser.close();
  const ok = stats.nonblank > 1000 && stats.audioContexts > 0 && stats.audioState === 'running';
  console.log(ok ? 'WASM BOOT ORACLE: PASS' : 'WASM BOOT ORACLE: FAIL');
  process.exit(ok ? 0 : 1);
})().catch(e => { console.error('FATAL', e); process.exit(2); });
