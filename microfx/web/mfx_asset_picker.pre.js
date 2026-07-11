// microfx web/ — the no-preload asset picker (GOAL 4, -DYODA_WASM_PRELOAD=OFF builds).
// Runs as --pre-js: holds the emscripten runtime on a run-dependency until game assets exist
// in MEMFS, then lets main() start. Two sources, in order:
//   1. an IndexedDB stash written by the chooser page (microfx/web/chooser.html detects which
//      game a picked folder holds and redirects to the matching build — the stash carries the
//      picked Files across that navigation, and survives refreshes);
//   2. a folder prompt right here (<input webkitdirectory> — Chrome/Firefox/Safari).
// The canonical data-file name for this build arrives from CMake in MFX_DATA_NAME (a tiny
// generated config pre-js linked just before this file).

var Module = typeof Module !== 'undefined' ? Module : {};
Module['preRun'] = Module['preRun'] || [];
Module['preRun'].push(function () {
  var dataName = typeof MFX_DATA_NAME !== 'undefined' ? MFX_DATA_NAME : 'YODESK.DTA';
  addRunDependency('yoda-assets');

  // entries = [{path: 'sfx/armed.wav', file: File}, ...] — classify + write into MEMFS.
  // Returns false (with a reason) when the data file is missing.
  function ingest(entries, onDone, onErr) {
    var dta = null, ini = null, sfx = [];
    entries.forEach(function (e) {
      var rel = e.path.split('/');
      var base = rel[rel.length - 1], lower = base.toLowerCase();
      if (lower === dataName.toLowerCase()) dta = e.file;
      else if (!ini && /\.ini$/.test(lower)) ini = e.file;
      else if (rel.length >= 2 && rel[rel.length - 2].toLowerCase() === 'sfx' &&
               /\.(wav|mid)$/.test(lower)) sfx.push({ f: e.file, name: lower });
    });
    if (!dta) { onErr(dataName + ' not found in that folder.'); return; }
    var pending = 1 + sfx.length + (ini ? 1 : 0);
    function done() { if (--pending === 0) onDone(); }
    function put(file, path) {
      file.arrayBuffer().then(function (buf) {
        FS.writeFile(path, new Uint8Array(buf));
        done();
      });
    }
    put(dta, '/' + dataName);                        // canonical name: MEMFS is case-sensitive
    if (ini) put(ini, '/yoda.INI');                  // else the game writes a fresh one
    else FS.writeFile('/yoda.INI', '[OPTIONS]\nTerrain=1\nMIDILoad=1\nLCount=1\n');
    try { FS.mkdir('/sfx'); } catch (e) {}
    sfx.forEach(function (s) { put(s.f, '/sfx/' + s.name); }); // lowercase: snd layer's retry case
  }

  function showPicker(msg) {
    var overlay = document.createElement('div');
    overlay.style.cssText =
      'position:fixed;inset:0;background:#101418;color:#e6e6d8;z-index:1000;' +
      'display:flex;flex-direction:column;align-items:center;justify-content:center;' +
      'font:16px system-ui,sans-serif;text-align:center;padding:2em;';
    overlay.innerHTML =
      '<h2 style="margin:0 0 .4em">Desktop Adventures</h2>' +
      '<p style="max-width:34em">Pick the game folder containing <b>' + dataName +
      '</b> (an <b>sfx/</b> subfolder and a settings <b>.INI</b> are used too when present). ' +
      'Files stay in your browser — nothing is uploaded.</p>' +
      '<p id="mfx-pick-err" style="color:#ff9d9d">' + (msg || '') + '</p>';
    var input = document.createElement('input');
    input.type = 'file';
    input.webkitdirectory = true;
    input.multiple = true;
    input.style.cssText = 'margin-top:1em;font:inherit;color:inherit;';
    overlay.appendChild(input);
    document.body.appendChild(overlay);
    input.addEventListener('change', function () {
      var entries = Array.prototype.slice.call(input.files).map(function (f) {
        var rel = (f.webkitRelativePath || f.name).split('/');
        if (rel.length > 1) rel.shift();             // drop the picked folder's own name
        return { path: rel.join('/'), file: f };
      });
      ingest(entries,
        function () { overlay.remove(); removeRunDependency('yoda-assets'); },
        function (err) { document.getElementById('mfx-pick-err').textContent = err; });
    });
  }

  // chooser stash first (kept across refreshes on purpose — reload just works)
  try {
    var req = indexedDB.open('mfx-assets', 1);
    req.onupgradeneeded = function () { req.result.createObjectStore('picked'); };
    req.onsuccess = function () {
      var db = req.result;
      try {
        var get = db.transaction('picked').objectStore('picked').get('files');
        get.onsuccess = function () {
          var entries = get.result;
          if (entries && entries.length)
            ingest(entries,
              function () { removeRunDependency('yoda-assets'); },
              function () { showPicker('The stashed folder did not contain ' + dataName + '.'); });
          else showPicker('');
        };
        get.onerror = function () { showPicker(''); };
      } catch (e) { showPicker(''); }
    };
    req.onerror = function () { showPicker(''); };
  } catch (e) { showPicker(''); }
});
