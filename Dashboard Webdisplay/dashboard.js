/**
 * dashboard.js — Browser-Side Telemetry Logic
 * PSU Viking Motorsports | FSAE Electric 2027 | Telemetry Dashboard
 *
 * ─────────────────────────────────────────────────────────
 * WHAT THIS FILE DOES:
 * ─────────────────────────────────────────────────────────
 * This file runs in the browser (not on the server).
 * It connects to dashboard.py via SocketIO (WebSockets),
 * listens for incoming telemetry data, and updates the
 * HTML elements on the page in real time.
 *
 * You never need to refresh the page — data pushes
 * automatically as soon as dashboard.py receives it
 * from the Pi.
 *
 * ─────────────────────────────────────────────────────────
 * HOW IT CONNECTS TO PYTHON:
 * ─────────────────────────────────────────────────────────
 * dashboard.py uses socketio.emit('telemetry_update', data)
 * This file listens with socket.on('telemetry_update', ...)
 * That pair is the handshake — the event name must match.
 *
 * ─────────────────────────────────────────────────────────
 * HOW TO ADD A NEW SENSOR VALUE:
 * ─────────────────────────────────────────────────────────
 * 1. In index.html, add an element with a unique id:
 *       <span id="motor_temp">--</span>
 *
 * 2. In the telemetry_update listener below, add:
 *       update('motor_temp', data.motor_temp);
 *    (the data field name must match what the Pi sends)
 *
 * That's it. No other files need to change.
 *
 * ─────────────────────────────────────────────────────────
 * TROUBLESHOOTING:
 * ─────────────────────────────────────────────────────────
 * Value stuck on '--'  → check the field name in update()
 *                         matches the JSON key from the Pi
 * Page shows offline   → dashboard.py isn't running, or
 *                         the browser can't reach port 5000
 * Log box is empty     → no data arriving; check MQTT
 *                         connection in the Python terminal
 */


// ── Connect to Flask-SocketIO ──────────────────────────────────────────────────
// io() is provided by the Socket.IO client library, loaded in index.html via CDN.
// With no arguments, it connects back to the same server that served this page
// (i.e., dashboard.py running at localhost:5000). No URL needed.
const socket = io();

// ── Connection Events ──────────────────────────────────────────────────────────
// These two listeners fire automatically when the WebSocket connects or drops.
// They update the status indicator on the page so you always know the live state.

socket.on('connect', function() {
    setStatus('Connected', true);        // Green indicator in the UI
    log('Connected to dashboard server.');
});

socket.on('disconnect', function() {
    setStatus('Disconnected', false);    // Red indicator in the UI
    log('Lost connection to server.');
    // Note: Socket.IO will automatically try to reconnect.
    // You don't need to restart anything — just wait for dashboard.py (python file).
});


// ── Receive Telemetry Data ─────────────────────────────────────────────────────
// This is the core of the file. Every time dashboard.py calls:
//   socketio.emit('telemetry_update', data)
//   this function fires with that data object.
//
// The data object is the JSON payload from the Pi, already parsed.
// Each field (e.g. data.wheel_speed_fl) must match the key name
// that the Pi/car is actually sending. Coordinate with Casey if
// field names are wrong or missing.

socket.on('telemetry_update', function(data) {

    // ── Wheel Speeds ──
    // Four corners: fl = front-left, fr = front-right,
    //               rl = rear-left,  rr = rear-right
    update('wheel_fl', data.wheel_speed_fl);
    update('wheel_fr', data.wheel_speed_fr);
    update('wheel_rl', data.wheel_speed_rl);
    update('wheel_rr', data.wheel_speed_rr);

    // ── Battery & Controls ──
    update('battery_voltage', data.battery_voltage);
    update('pedal_position',  data.pedal_position);
    update('steering_angle',  data.steering_angle);

    // ── ADD NEW FIELDS HERE ──
    // Example: update('motor_temp', data.motor_temp);
    // Matching HTML id required in index.html.

    // Log the raw packet for debugging — visible in the on-screen log box
    log('Data received: ' + JSON.stringify(data));
});


// ── Helper: Update a card value ────────────────────────────────────────────────
// Finds an HTML element by its id and sets its text to the new value.
// Silently skips if the element doesn't exist or the value is undefined,
// so a missing field in the data won't crash the whole update cycle.
//
// Usage: update('element_id', data.field_name)
// Requires: a matching <element id="element_id"> in index.html
function update(elementId, value) {
    const el = document.getElementById(elementId);
    if (el && value !== undefined) {
        el.textContent = value;
    }
    // If value IS undefined: the element keeps its previous value (or '--' default).
    // Check the Pi's JSON output if a field is never updating.
}


// ── Helper: Update connection status indicator ─────────────────────────────────
// Sets the text and CSS class of the #connection-status element.
// The class ('connected' or 'disconnected') is what drives the color in style.css.
//
// Requires in index.html: <span id="connection-status"></span>
// Requires in style.css:  .connected { } and .disconnected { } rules
function setStatus(message, isConnected) {
    const el = document.getElementById('connection-status');
    el.textContent = message;
    el.className = isConnected ? 'connected' : 'disconnected';
}


// ── Helper: Add a timestamped line to the log box ─────────────────────────────
// Creates a new <p> element with the current time prepended,
// then inserts it at the TOP of the log box (newest first).
// The log box will keep growing — consider adding a max-line cap
// if memory becomes a concern during long sessions.
//
// Requires in index.html: <div id="log-box"></div>
function log(message) {
    const logBox = document.getElementById('log-box');
    const line   = document.createElement('p');
    const time   = new Date().toLocaleTimeString();
    line.textContent = '[' + time + '] ' + message;
    logBox.prepend(line);  // prepend = newest message at top
}
