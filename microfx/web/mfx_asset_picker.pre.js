// microfx web/ — the no-preload asset picker (GOAL 4, -DYODA_WASM_PRELOAD=OFF builds).
// Runs as --pre-js: holds the emscripten runtime on a run-dependency until the user picks
// their own game folder, then copies the needed files into MEMFS and lets main() start.
// The canonical data-file name for this build arrives from CMake in MFX_DATA_NAME (a tiny
// generated config pre-js linked just before this file).
//
// Expected picks: the folder holding YODESK.DTA / YODADEMO.DTA / DESKTOP.DAW (+ optional
// sfx/ subfolder and a settings .INI). Uses <input webkitdirectory> — Chrome/Firefox/Safari.

var Module = typeof Module !== 'undefined' ? Module : {};
Module['preRun'] = Module['preRun'] || [];
Module['preRun'].push(function () {
  var dataName = typeof MFX_DATA_NAME !== 'undefined' ? MFX_DATA_NAME : 'YODESK.DTA';
  addRunDependency('yoda-assets');

  var overlay = document.createElement('div');
  overlay.style.cssText =
    'position:fixed;inset:0;background:#0c1c18;color:#e6e6d8;z-index:1000;' +
    'display:flex;flex-direction:column;align-items:center;justify-content:center;' +
    'font:16px sans-serif;text-align:center;padding:2em;';
  overlay.innerHTML =
    '<h2 style="margin:0 0 .4em">Desktop Adventures</h2>' +
    '<p style="max-width:34em">Pick the game folder containing <b>' + dataName +
    '</b> (an <b>sfx/</b> subfolder and a settings <b>.INI</b> are used too when present). ' +
    'Files stay in your browser — nothing is uploaded.</p>' +
    '<p id="mfx-pick-err" style="color:#ff9d9d"></p>';
  var input = document.createElement('input');
  input.type = 'file';
  input.webkitdirectory = true;
  input.multiple = true;
  input.style.cssText = 'margin-top:1em;font:inherit;';
  overlay.appendChild(input);
  document.body.appendChild(overlay);

  input.addEventListener('change', function () {
    var files = Array.prototype.slice.call(input.files);
    var dta = null, ini = null, sfx = [];
    files.forEach(function (f) {
      var rel = (f.webkitRelativePath || f.name).split('/');
      if (rel.length > 1) rel.shift();                    // drop the picked folder's own name
      var base = rel[rel.length - 1], lower = base.toLowerCase();
      if (lower === dataName.toLowerCase()) dta = f;
      else if (!ini && /\.ini$/.test(lower)) ini = f;
      else if (rel.length >= 2 && rel[rel.length - 2].toLowerCase() === 'sfx' &&
               /\.(wav|mid)$/.test(lower)) sfx.push({ f: f, name: lower });
    });
    if (!dta) {
      document.getElementById('mfx-pick-err').textContent =
        dataName + ' not found in that folder — pick the game’s install folder.';
      return;
    }
    var pending = 1 + sfx.length + (ini ? 1 : 0);
    function done() { if (--pending === 0) { overlay.remove(); removeRunDependency('yoda-assets'); } }
    function put(file, path) {
      file.arrayBuffer().then(function (buf) {
        FS.writeFile(path, new Uint8Array(buf));
        done();
      });
    }
    put(dta, '/' + dataName);                              // canonical name: MEMFS is case-sensitive
    if (ini) put(ini, '/yoda.INI');                        // else the game writes a fresh one
    else FS.writeFile('/yoda.INI', '[OPTIONS]\nTerrain=1\nMIDILoad=1\nLCount=1\n');
    try { FS.mkdir('/sfx'); } catch (e) {}
    sfx.forEach(function (s) { put(s.f, '/sfx/' + s.name); });  // lowercase: the snd layer's retry case
  });
});
