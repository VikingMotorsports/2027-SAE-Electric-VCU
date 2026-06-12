// =============================================================================
// dashboard.js — VCU Pit Telemetry Dashboard
// PSU Viking Motorsports | FSAE Electric 2027
//
// PURPOSE:
//   This file is the "brain" of the live telemetry dashboard running in the
//   pit. It does three main things:
//     1. Connects to the Flask server via WebSocket (a live, two-way connection)
//     2. Sets up Chart.js line charts to display sensor data over time
//     3. Listens for incoming MQTT telemetry packets and updates the UI
//
// HOW DATA FLOWS:
//   Car sensors → CAN bus → VCU (STM32) → Raspberry Pi (MQTT broker)
//     → Flask server (Python) → THIS FILE (JavaScript in the browser)
//
// DEPENDENCIES:
//   - Socket.IO  (WebSocket library, loaded in index.html)
//   - Chart.js   (charting library, loaded in index.html)
// =============================================================================


// ── SECTION 1: WebSocket Connection ──────────────────────────────────────────
//
// Socket.IO is a library that keeps a persistent, real-time connection open
// between this browser page and the Flask server (dashboard.py).
// Unlike a normal HTTP request (which asks once and closes), a WebSocket
// stays open so the server can PUSH data to us the moment it arrives.
//
// `io()` automatically connects to whatever server served this page.
// No URL is needed — Flask-SocketIO handles the handshake.
// ─────────────────────────────────────────────────────────────────────────────
const socket = io();


// ── SECTION 2: Connection Event Listeners ────────────────────────────────────
//
// socket.on('event_name', callback) is how Socket.IO lets us react to events.
// Think of it like setting up a phone — these lines define what happens when
// the call connects or drops. The callback function runs automatically when
// the event fires; we don't call it ourselves.
// ─────────────────────────────────────────────────────────────────────────────

// Fires once when the browser successfully connects to the Flask server.
socket.on('connect', function() {
  setStatus('Connected', true);           // Updates the status badge in the UI
  log('Connected to dashboard server.');  // Adds a timestamped entry to the log box
});

// Fires if the connection drops (e.g., server crash, network loss).
// The page will show "Disconnected" so the pit crew knows data is stale.
socket.on('disconnect', function() {
  setStatus('Disconnected', false);
  log('Lost connection to server.');
});


// ── SECTION 3: Chart Configuration ───────────────────────────────────────────
//
// All charts are "rolling window" line charts — they always show the last
// MAX_POINTS data points and drop older ones as new data comes in.
// This is like the scrolling ECG display you see in hospital monitors.
// ─────────────────────────────────────────────────────────────────────────────

// How many data points are visible on each chart at once.
// At 1 packet/second, this = 60 seconds of history.
const MAX_POINTS = 60;

// Returns an array of 60 `null` values.
// Chart.js renders null as a gap, so the chart starts empty instead of at zero.
function emptyData()   { return Array(MAX_POINTS).fill(null); }

// Returns an array of 60 empty strings — one label per point on the X-axis.
// We hide the X-axis labels (no timestamps shown), so these are just placeholders.
function emptyLabels() { return Array(MAX_POINTS).fill('');   }

// Builds and returns a Chart.js `options` object that all charts share.
// Taking yMin/yMax/stepSize as parameters lets each chart set its own Y-axis
// range without duplicating all the shared styling code.
//
// Parameters:
//   yMin     — lowest value shown on the Y-axis (e.g., 0)
//   yMax     — highest value shown on the Y-axis (e.g., 100 for percentage)
//   stepSize — gap between Y-axis tick marks (e.g., 25 → shows 0, 25, 50, 75, 100)
function baseChartOptions(yMin, yMax, stepSize) {
  return {
    animation: false,   // Disable animations for performance — data updates rapidly
    responsive: true,   // Chart resizes automatically if the browser window changes
    scales: {
      x: { display: false },  // Hide X-axis entirely — timestamps clutter the view
      y: {
        min: yMin,
        max: yMax,
        ticks: { color: '#888', stepSize: stepSize },  // Gray tick labels
        grid:  { color: '#1a1a1a' }                    // Dark grid lines (matches dark UI theme)
      }
    },
    plugins: {
      legend: { labels: { color: '#e0e0e0' } }  // Light-colored legend text
    }
  };
}


// ── SECTION 4: Chart Initialization ──────────────────────────────────────────
//
// Each chart is created by:
//   1. Finding its <canvas> element in index.html by ID
//   2. Getting a "2D drawing context" from that canvas (required by Chart.js)
//   3. Calling `new Chart(ctx, config)` with the chart type, data, and options
//
// Chart.js draws directly onto HTML <canvas> elements using the browser's
// 2D graphics engine — no SVG or DOM elements involved.
// ─────────────────────────────────────────────────────────────────────────────

// ── 4a. LIVE: Pedal Position Chart ───────────────────────────────────────────
// Shows throttle pedal travel as a percentage (0% = released, 100% = full throttle).
// This is actively populated from real MQTT data right now.
const pedalCtx = document.getElementById('pedal-chart').getContext('2d');
const pedalChart = new Chart(pedalCtx, {
  type: 'line',
  data: {
    labels: emptyLabels(),  // 60 empty X-axis labels (hidden)
    datasets: [{
      label: 'Pedal Position (%)',
      data: emptyData(),                         // Start with 60 empty slots
      borderColor: '#6df5a8',                    // Green line color
      backgroundColor: 'rgba(0, 255, 136, 0.08)', // Very faint green fill under the line
      borderWidth: 2,
      pointRadius: 0,   // No dots on each data point — cleaner at high update rates
      tension: 0.3,     // Slight curve smoothing between points (0 = straight lines)
      fill: true,       // Fill the area under the line
    }]
  },
  options: baseChartOptions(0, 100, 25)  // Y-axis: 0–100%, ticks every 25%
});

// ── 4b. FUTURE: Pack Voltage / Current / Power Chart ─────────────────────────
// Three datasets on one chart — useful for seeing the relationship between
// voltage sag, current draw, and resulting power output during a run.
// Marked FUTURE: will auto-populate once the VCU publishes these MQTT fields.
const powerCtx = document.getElementById('power-chart').getContext('2d');
const powerChart = new Chart(powerCtx, {
  type: 'line',
  data: {
    labels: emptyLabels(),
    datasets: [
      // Each object in this array is one line on the chart.
      // `fill: false` means no shaded area — keeps multi-line charts readable.
      { label: 'Voltage (V)',  data: emptyData(), borderColor: '#6df5a8', borderWidth: 1.5, pointRadius: 0, tension: 0.3, fill: false },
      { label: 'Current (A)', data: emptyData(), borderColor: '#ffe66d', borderWidth: 1.5, pointRadius: 0, tension: 0.3, fill: false },
      { label: 'Power (kW)',  data: emptyData(), borderColor: '#ff6b6b', borderWidth: 1.5, pointRadius: 0, tension: 0.3, fill: false },
    ]
  },
  options: baseChartOptions(0, 400, 100)  // Y-axis: 0–400, ticks every 100
});

// ── 4c. FUTURE: Battery & Motor Temperature Chart ─────────────────────────────
// Thermal monitoring is critical for battery safety.
// Marked FUTURE: will auto-populate once the VCU publishes these MQTT fields.
const tempCtx = document.getElementById('temp-chart').getContext('2d');
const tempChart = new Chart(tempCtx, {
  type: 'line',
  data: {
    labels: emptyLabels(),
    datasets: [
      { label: 'Battery Temp (°C)', data: emptyData(), borderColor: '#ffe66d', borderWidth: 1.5, pointRadius: 0, tension: 0.3, fill: false },
      { label: 'Motor Temp (°C)',   data: emptyData(), borderColor: '#ff6b6b', borderWidth: 1.5, pointRadius: 0, tension: 0.3, fill: false },
    ]
  },
  options: baseChartOptions(0, 120, 30)  // Y-axis: 0–120°C, ticks every 30°C
});


// ── SECTION 5: Rolling Window Helper ─────────────────────────────────────────
//
// This is the core mechanism that makes charts scroll in real time.
//
// Chart.js stores data as a fixed-length array. To "scroll" the chart left,
// we add one new value to the END and remove one old value from the FRONT.
// This keeps the array always at MAX_POINTS length.
//
//   Before: [10, 20, 30, ... , 80]   ← 60 values
//   push(90) → [10, 20, 30, ... , 80, 90]  ← 61 values (too many)
//   shift()  →     [20, 30, ... , 80, 90]  ← back to 60 (oldest dropped)
//
// Parameters:
//   chart        — the Chart.js chart object to update
//   datasetIndex — which dataset (line) within that chart to push to (0-indexed)
//   value        — the new data point to add
// ─────────────────────────────────────────────────────────────────────────────
function pushToChart(chart, datasetIndex, value) {
  chart.data.labels.push('');    // Add a blank label for the new point
  chart.data.labels.shift();     // Remove the oldest label from the front

  chart.data.datasets[datasetIndex].data.push(value);   // Add new value to end
  chart.data.datasets[datasetIndex].data.shift();        // Drop oldest value from front

  // NOTE: chart.update() is NOT called here — the caller does it once after
  // pushing all datasets. This avoids triggering multiple redraws per packet.
}


// ── SECTION 6: Telemetry Data Handler ────────────────────────────────────────
//
// This is the most important event listener. Flask-SocketIO emits a
// 'telemetry_update' event every time a new MQTT message arrives from the car.
//
// `data` is a JavaScript object (parsed from JSON) that looks like:
//   {
//     "wheel_speed_fl": 23.4,
//     "pedal_position": 67.2,
//     "battery_voltage": 312.1,
//     "steering_angle": -5.0
//   }
//
// Not all fields are guaranteed to exist in every packet — the `!== undefined`
// checks below protect against crashing if a field is missing.
// ─────────────────────────────────────────────────────────────────────────────
socket.on('telemetry_update', function(data) {

  // ── 6a. LIVE: Pedal Position ────────────────────────────────────────────
  // Update the numeric display card AND push to the rolling chart.
  update('pedal_position', data.pedal_position);

  if (data.pedal_position !== undefined) {
    // parseFloat() converts the value to a decimal number in case it arrived
    // as a string (MQTT payloads are always strings under the hood).
    pushToChart(pedalChart, 0, parseFloat(data.pedal_position));
    pedalChart.update();  // Tell Chart.js to redraw the chart now
  }

  // ── 6b. FUTURE: Battery & Power ────────────────────────────────────────
  // update() calls populate the stat cards even before the chart is active.
  // The chart only updates when pack_voltage is present (used as the trigger field).
  update('pack_voltage',    data.pack_voltage);
  update('pack_current',    data.pack_current);
  update('pack_power',      data.pack_power);
  update('state_of_charge', data.state_of_charge);

  if (data.pack_voltage !== undefined) {
    pushToChart(powerChart, 0, parseFloat(data.pack_voltage));  // Dataset 0: Voltage
    pushToChart(powerChart, 1, parseFloat(data.pack_current));  // Dataset 1: Current
    pushToChart(powerChart, 2, parseFloat(data.pack_power));    // Dataset 2: Power
    powerChart.update();
  }

  // ── 6c. FUTURE: Thermal & Vehicle ──────────────────────────────────────
  // battery_temp is used as the trigger — if it's missing, we skip the chart
  // update to avoid pushing `NaN` into the temperature datasets.
  update('battery_temp',    data.battery_temp);
  update('motor_temp',      data.motor_temp);
  update('battery_voltage', data.battery_voltage);
  update('steering_angle',  data.steering_angle);

  if (data.battery_temp !== undefined) {
    pushToChart(tempChart, 0, parseFloat(data.battery_temp));  // Dataset 0: Battery temp
    pushToChart(tempChart, 1, parseFloat(data.motor_temp));    // Dataset 1: Motor temp
    tempChart.update();
  }

  // Log the raw JSON packet so the pit crew can verify what the car is sending.
  log('Data received: ' + JSON.stringify(data));
});


// ── SECTION 7: UI Helper Functions ───────────────────────────────────────────


// ── update(elementId, value) ──────────────────────────────────────────────────
// Finds an HTML element by its `id` attribute and sets its visible text.
// Used to populate the stat cards (e.g., "312.1 V", "67 %").
//
// Formatting rules:
//   - Whole numbers (e.g., 5)    → displayed as-is: "5"
//   - Decimals (e.g., 67.2345)   → rounded to 1 decimal place: "67.2"
//   - Strings (e.g., "OK")       → displayed as-is
//
// If the element doesn't exist in the HTML, or the value is undefined,
// this function does nothing (safe no-op — no crash).
// ─────────────────────────────────────────────────────────────────────────────
function update(elementId, value) {
  const el = document.getElementById(elementId);
  if (el && value !== undefined) {
    el.textContent = typeof value === 'number'
      ? (Number.isInteger(value) ? value : value.toFixed(1))
      : value;
  }
}

// ── setStatus(message, isConnected) ──────────────────────────────────────────
// Updates the connection status badge at the top of the dashboard.
// Changes both the text AND the CSS class so the badge turns green/red
// based on the `isConnected` boolean.
//
// The CSS classes 'connected' and 'disconnected' are defined in style.css.
// ─────────────────────────────────────────────────────────────────────────────
function setStatus(message, isConnected) {
  const el = document.getElementById('connection-status');
  el.textContent = message;
  el.className = isConnected ? 'connected' : 'disconnected';
}

// ── log(message) ──────────────────────────────────────────────────────────────
// Adds a new timestamped line to the scrolling log box at the bottom of the UI.
//
// How it works:
//   1. Create a new <p> element
//   2. Set its text to "[HH:MM:SS AM/PM] <message>"
//   3. Use prepend() to insert it at the TOP of the log box
//      (newest entries appear first — no need to scroll down)
//
// The log box grows indefinitely during a session. For long events,
// consider adding a max-entry cap to avoid memory buildup.
// ─────────────────────────────────────────────────────────────────────────────
function log(message) {
  const logBox = document.getElementById('log-box');
  const line = document.createElement('p');
  const time = new Date().toLocaleTimeString();  // e.g., "2:34:07 PM"
  line.textContent = '[' + time + '] ' + message;
  logBox.prepend(line);  // Insert at top so newest log is always visible
}
