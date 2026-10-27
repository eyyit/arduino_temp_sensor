/* Small, local-only UI for the pinned ESPHome EventSource API. */
export const CLIMATE = ['temperature_fahrenheit','temperature_celsius','humidity','pressure'];
export const normalize = name => name.split('/').pop().toLowerCase().replace(/[^a-z0-9]+/g,'_').replace(/^_|_$/g,'');
export function ingest(model, event, now) {
  if (!event || typeof event.id !== 'string' || !event.id.startsWith('sensor/')) return false;
  const key = normalize(event.id);
  model.values[key] = typeof event.value === 'number' && Number.isFinite(event.value) ? event.value : null;
  model.received[key] = now;
  return true;
}
export function health(model, now) {
  if (!model.connected) return {fresh:false, label:'Reconnecting', age:null};
  const age = model.values.sensor_sample_age;
  const received = model.received.sensor_sample_age;
  if (!Number.isFinite(age) || !Number.isFinite(received)) return {fresh:false, label:'Waiting for readings', age:null};
  const effectiveAge = age + Math.max(0, now - received) / 1000;
  const valid = CLIMATE.every(key => Number.isFinite(model.values[key]) &&
    Number.isFinite(model.received[key]) && now - model.received[key] < 15000);
  const fresh = valid && model.values.sensor_sample_fresh === 1 && effectiveAge < 15;
  return {fresh, label:fresh ? 'Live readings' : 'Readings unavailable', age:effectiveAge};
}
export function duration(seconds) {
  if (!Number.isFinite(seconds) || seconds < 0) return '—';
  const days = Math.floor(seconds / 86400), hours = Math.floor(seconds / 3600) % 24;
  const minutes = Math.floor(seconds / 60) % 60;
  return days ? `${days}d ${hours}h` : hours ? `${hours}h ${minutes}m` : `${minutes}m`;
}
export function pretty(value, unit='') {
  return Number.isFinite(value) ? value.toFixed(2) + (unit ? ` ${unit}` : '') : '—';
}

if (typeof document !== 'undefined') {
  const meta = document.createElement('meta');
  meta.name = 'viewport'; meta.content = 'width=device-width, initial-scale=1'; document.head.append(meta);
  const model = {connected:false, values:{}, received:{}};
  let unit = 'F';
  try { unit = localStorage.getItem('temperature-unit') === 'C' ? 'C' : 'F'; } catch (_) {}
  const root = document.querySelector('esp-app') || document.body.appendChild(document.createElement('main'));
  root.innerHTML = `<main class="sensor-page">
    <header><div><p class="eyebrow">CLIMATE MONITOR</p><h1 id="device-title">Climate sensor</h1></div>
      <span class="status" id="status" role="status" aria-live="polite">Connecting</span></header>
    <section class="readings" aria-label="Current climate readings">
      <article class="temperature"><div class="card-top"><h2>Temperature</h2><div class="units" role="group" aria-label="Temperature unit">
        <button id="unit-f" type="button" aria-pressed="true">°F</button><button id="unit-c" type="button" aria-pressed="false">°C</button></div></div>
        <p class="large-number"><span id="temperature">—</span><span id="temperature-unit" class="unit">°F</span></p>
        <p class="secondary" id="other-temperature">Waiting for a fresh sample</p></article>
      <article><h2>Humidity</h2><p class="number"><span id="humidity">—</span><span class="unit">%</span></p><p class="secondary">Relative humidity</p></article>
      <article><h2>Pressure</h2><p class="number"><span id="pressure">—</span><span class="unit">hPa</span></p><p class="secondary">Air pressure</p></article>
    </section>
    <p class="sample-note" id="sample-note" aria-live="polite">Connecting to the sensor…</p>
    <section class="context" aria-label="Additional climate readings"><div><span>Dew point</span><strong id="dewpoint">—</strong></div>
      <div><span>Altitude estimate</span><strong id="altitude">—</strong></div></section>
    <details><summary>Device &amp; connection <span>Memory, Wi-Fi and uptime</span></summary>
      <dl class="diagnostics"><div><dt>Wi-Fi signal</dt><dd id="wifi">—</dd></div><div><dt>Uptime</dt><dd id="uptime">—</dd></div>
        <div><dt>Free memory</dt><dd id="free-heap">—</dd></div><div><dt>Largest free block</dt><dd id="max-block">—</dd></div>
        <div><dt>Heap fragmentation</dt><dd id="fragmentation">—</dd></div><div><dt>Roaming attempts</dt><dd id="roams">—</dd></div>
        <div><dt>Wi-Fi scans</dt><dd id="scans">—</dd></div><div><dt>Scan errors</dt><dd id="scan-errors">—</dd></div>
        <div><dt>Scan recoveries</dt><dd id="recoveries">—</dd></div></dl>
      <p class="log-destination">Event logs → 10.0.0.2:514 · UDP, best effort</p></details>
    <footer><a href="https://grafana.eyyit.com/d/0VH08B5Zk/environment" target="_blank" rel="noopener">View history in Grafana <span aria-hidden="true">↗</span></a>
      <a href="/metrics">Prometheus metrics</a><span>Updates every 5 seconds</span></footer>
  </main>`;
  const el = id => document.getElementById(id);
  const set = (id, value) => { if (el(id).textContent !== value) el(id).textContent = value; };
  const knownRoom = location.hostname.match(/^sensor_(office|bedroom|livingroom|outside|kid)(?:\.|$)/);
  if (knownRoom) set('device-title', ({livingroom:'Living room',kid:'Kid',office:'Office',bedroom:'Bedroom',outside:'Outside'})[knownRoom[1]] + ' Sensor');
  function render() {
    const now = performance.now(), state = health(model, now), values = model.values;
    set('status', state.label); el('status').classList.toggle('fresh',state.fresh);
    root.classList.toggle('unavailable', !state.fresh);
    const climate = key => state.fresh ? values[key] : null;
    set('temperature', pretty(climate(unit === 'F' ? 'temperature_fahrenheit' : 'temperature_celsius')));
    set('temperature-unit', '°' + unit);
    set('other-temperature', state.fresh ? pretty(climate(unit === 'F' ? 'temperature_celsius' : 'temperature_fahrenheit'),unit === 'F' ? '°C' : '°F') : 'Waiting for a fresh sample');
    set('humidity', pretty(climate('humidity'))); set('pressure', pretty(climate('pressure')));
    set('dewpoint', pretty(climate(unit === 'F' ? 'dew_point_fahrenheit' : 'dew_point_celsius'),'°' + unit));
    set('altitude', pretty(climate('altitude'),'m'));
    set('sample-note', state.fresh ? `Sample age ${state.age.toFixed(1)}s · Measurements every 5 seconds` :
      model.connected ? 'Fresh readings are unavailable. Values will return when the sensor recovers.' : 'Connection interrupted. Reconnecting automatically…');
    const diagnostic = key => model.connected && now - (model.received[key] ?? -Infinity) < 15000 ? values[key] : null;
    set('wifi',pretty(diagnostic('wifi_signal'),'dBm')); set('uptime',duration(diagnostic('uptime')));
    for (const [id,key] of [['free-heap','free_heap'],['max-block','max_free_block_size']]) {
      const value = diagnostic(key); set(id, Number.isFinite(value) ? (value/1024).toFixed(2) + ' KiB' : '—');
    }
    set('fragmentation',pretty(diagnostic('heap_fragmentation_percent'),'%'));
    for (const [id,key] of [['roams','wifi_roam_attempts'],['scans','wifi_roam_scans'],['scan-errors','wifi_roam_scan_errors'],['recoveries','wifi_scan_recoveries']]) {
      const value = diagnostic(key); set(id,Number.isFinite(value) ? String(Math.round(value)) : '—');
    }
    el('unit-f').setAttribute('aria-pressed',String(unit === 'F')); el('unit-c').setAttribute('aria-pressed',String(unit === 'C'));
  }
  for (const value of ['F','C']) el('unit-' + value.toLowerCase()).addEventListener('click',() => {
    unit = value; try { localStorage.setItem('temperature-unit',unit); } catch (_) {} render();
  });
  const events = new EventSource('/events');
  events.addEventListener('open',() => { model.connected=true; model.values={}; model.received={}; render(); });
  events.addEventListener('error',() => { model.connected=false; render(); });
  events.addEventListener('ping',event => {
    try { const data=JSON.parse(event.data); if (typeof data.title === 'string') { set('device-title',data.title); document.title=data.title; } } catch (_) {}
  });
  events.addEventListener('state',event => { try { if (ingest(model,JSON.parse(event.data),performance.now())) render(); } catch (_) {} });
  setInterval(render,1000); render();
}
