#ifndef WEBUI_H
#define WEBUI_H

const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Waypoint Navigator</title>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;600&display=swap" rel="stylesheet">
    <style>
        :root {
            --bg-color: #0f172a;
            --surface-color: rgba(30, 41, 59, 0.7);
            --primary-color: #3b82f6;
            --primary-hover: #2563eb;
            --danger-color: #ef4444;
            --success-color: #10b981;
            --warning-color: #f59e0b;
            --text-color: #f8fafc;
            --text-muted: #94a3b8;
            --border-color: rgba(255, 255, 255, 0.1);
        }
        body {
            font-family: 'Inter', sans-serif;
            background: linear-gradient(135deg, #0f172a 0%, #1e1b4b 100%);
            color: var(--text-color);
            margin: 0;
            padding: 20px;
            min-height: 100vh;
            display: flex;
            flex-direction: column;
            align-items: center;
        }
        .container {
            width: 100%;
            max-width: 500px;
        }
        h1 {
            text-align: center;
            font-weight: 600;
            margin-bottom: 30px;
            background: -webkit-linear-gradient(#60a5fa, #a78bfa);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
        }
        .card {
            background: var(--surface-color);
            backdrop-filter: blur(10px);
            border: 1px solid var(--border-color);
            border-radius: 16px;
            padding: 24px;
            margin-bottom: 24px;
            box-shadow: 0 10px 25px rgba(0, 0, 0, 0.5);
        }
        .card-title {
            font-size: 0.75rem;
            font-weight: 600;
            letter-spacing: 0.1em;
            text-transform: uppercase;
            color: var(--text-muted);
            margin: 0 0 16px 0;
        }
        .form-group {
            margin-bottom: 16px;
        }
        label {
            display: block;
            margin-bottom: 8px;
            font-size: 0.9rem;
            color: var(--text-muted);
        }
        input, select {
            width: 100%;
            padding: 12px;
            background: rgba(0, 0, 0, 0.2);
            border: 1px solid var(--border-color);
            border-radius: 8px;
            color: white;
            font-family: inherit;
            box-sizing: border-box;
            transition: all 0.3s;
            font-size: 0.95rem;
        }
        select option {
            background: #1e293b;
            color: white;
        }
        input:focus, select:focus {
            outline: none;
            border-color: var(--primary-color);
            box-shadow: 0 0 0 2px rgba(59, 130, 246, 0.3);
        }
        button {
            width: 100%;
            padding: 14px;
            background: var(--primary-color);
            color: white;
            border: none;
            border-radius: 8px;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.2s;
            font-family: inherit;
            font-size: 0.95rem;
        }
        button:hover {
            background: var(--primary-hover);
            transform: translateY(-1px);
            box-shadow: 0 4px 12px rgba(59,130,246,0.4);
        }
        .btn-save-tz {
            background: linear-gradient(135deg, #f59e0b, #d97706);
        }
        .btn-save-tz:hover {
            background: linear-gradient(135deg, #fbbf24, #f59e0b);
            box-shadow: 0 4px 12px rgba(245,158,11,0.4);
        }
        .tz-badge {
            display: inline-block;
            margin-top: 10px;
            padding: 4px 12px;
            border-radius: 20px;
            background: rgba(245, 158, 11, 0.15);
            border: 1px solid rgba(245, 158, 11, 0.35);
            color: #fbbf24;
            font-size: 0.8rem;
            font-weight: 600;
        }
        .waypoint-item {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 12px;
            background: rgba(0, 0, 0, 0.2);
            border-radius: 8px;
            margin-bottom: 10px;
            border: 1px solid var(--border-color);
            transition: border-color 0.2s;
        }
        .waypoint-item:hover {
            border-color: rgba(59,130,246,0.4);
        }
        .waypoint-info h3 {
            margin: 0 0 4px 0;
            font-size: 1.1rem;
        }
        .waypoint-info p {
            margin: 0;
            font-size: 0.8rem;
            color: var(--text-muted);
        }
        .btn-delete {
            background: transparent;
            color: var(--danger-color);
            width: auto;
            padding: 8px 12px;
            border: 1px solid rgba(239, 68, 68, 0.3);
        }
        .btn-delete:hover {
            background: rgba(239, 68, 68, 0.1);
            transform: none;
            box-shadow: none;
        }
        .toast {
            position: fixed;
            bottom: 24px;
            left: 50%;
            transform: translateX(-50%) translateY(10px);
            background: #10b981;
            color: white;
            padding: 12px 28px;
            border-radius: 30px;
            opacity: 0;
            transition: opacity 0.3s, transform 0.3s;
            pointer-events: none;
            font-weight: 600;
            box-shadow: 0 4px 20px rgba(16,185,129,0.4);
        }
        .toast.show {
            opacity: 1;
            transform: translateX(-50%) translateY(0);
        }
    </style>
</head>
<body>

    <div class="container">
        <h1>&#x1F9ED; Waypoint Nav</h1>

        <!-- ======================== -->
        <!-- Waypoint Add Card        -->
        <!-- ======================== -->
        <div class="card">
            <p class="card-title">&#x1F4CD; Add Waypoint</p>
            <div class="form-group">
                <label>Location Name</label>
                <input type="text" id="wpName" placeholder="e.g. Basecamp" maxlength="19">
            </div>
            <div class="form-group">
                <label>Latitude</label>
                <input type="number" id="wpLat" step="any" placeholder="e.g. 37.7749">
            </div>
            <div class="form-group">
                <label>Longitude</label>
                <input type="number" id="wpLon" step="any" placeholder="e.g. -122.4194">
            </div>
            <button onclick="addWaypoint()">&#x2795; Save Waypoint</button>
        </div>

        <!-- ======================== -->
        <!-- Saved Locations Card     -->
        <!-- ======================== -->
        <div class="card">
            <p class="card-title">&#x1F5FA; Saved Locations</p>
            <div id="waypointList">
                <p style="color: #94a3b8; text-align:center;">Loading...</p>
            </div>
        </div>

        <!-- ======================== -->
        <!-- Timezone Settings Card   -->
        <!-- ======================== -->
        <div class="card">
            <p class="card-title">&#x1F552; Timezone Settings</p>
            <div class="form-group">
                <label for="tzSelect">Select Your Timezone</label>
                <select id="tzSelect">
                    <option value="-12.0">UTC-12:00 &mdash; International Date Line West</option>
                    <option value="-11.0">UTC-11:00 &mdash; Midway Island, Samoa</option>
                    <option value="-10.0">UTC-10:00 &mdash; Hawaii (HST)</option>
                    <option value="-9.5">UTC-09:30 &mdash; Marquesas Islands</option>
                    <option value="-9.0">UTC-09:00 &mdash; Alaska (AKST)</option>
                    <option value="-8.0">UTC-08:00 &mdash; Pacific (PST), Los Angeles</option>
                    <option value="-7.0">UTC-07:00 &mdash; Mountain (MST), Denver</option>
                    <option value="-6.0">UTC-06:00 &mdash; Central (CST), Chicago</option>
                    <option value="-5.0">UTC-05:00 &mdash; Eastern (EST), New York</option>
                    <option value="-4.5">UTC-04:30 &mdash; Venezuela</option>
                    <option value="-4.0">UTC-04:00 &mdash; Atlantic, Santiago</option>
                    <option value="-3.5">UTC-03:30 &mdash; Newfoundland (NST)</option>
                    <option value="-3.0">UTC-03:00 &mdash; Brazil, Buenos Aires</option>
                    <option value="-2.0">UTC-02:00 &mdash; Mid-Atlantic</option>
                    <option value="-1.0">UTC-01:00 &mdash; Azores, Cape Verde</option>
                    <option value="0.0">UTC+00:00 &mdash; London, Dublin (GMT/WET)</option>
                    <option value="1.0">UTC+01:00 &mdash; Central Europe (CET), Paris</option>
                    <option value="2.0">UTC+02:00 &mdash; Eastern Europe, Cairo (EET)</option>
                    <option value="3.0">UTC+03:00 &mdash; Moscow, Nairobi, Riyadh</option>
                    <option value="3.5">UTC+03:30 &mdash; Tehran (IRST)</option>
                    <option value="4.0">UTC+04:00 &mdash; Dubai, Baku (GST)</option>
                    <option value="4.5">UTC+04:30 &mdash; Kabul (AFT)</option>
                    <option value="5.0">UTC+05:00 &mdash; Pakistan (PKT), Karachi</option>
                    <option value="5.5">UTC+05:30 &mdash; India (IST), Mumbai, Delhi</option>
                    <option value="5.75">UTC+05:45 &mdash; Nepal (NPT)</option>
                    <option value="6.0">UTC+06:00 &mdash; Bangladesh (BST), Almaty</option>
                    <option value="6.5">UTC+06:30 &mdash; Myanmar, Yangon (MMT)</option>
                    <option value="7.0">UTC+07:00 &mdash; Bangkok, Jakarta, Hanoi</option>
                    <option value="8.0">UTC+08:00 &mdash; China, Singapore, Perth (CST)</option>
                    <option value="8.75">UTC+08:45 &mdash; Eucla, Western Australia</option>
                    <option value="9.0">UTC+09:00 &mdash; Japan, Korea (JST)</option>
                    <option value="9.5">UTC+09:30 &mdash; Adelaide, Darwin (ACST)</option>
                    <option value="10.0">UTC+10:00 &mdash; Sydney, Brisbane (AEST)</option>
                    <option value="10.5">UTC+10:30 &mdash; Lord Howe Island</option>
                    <option value="11.0">UTC+11:00 &mdash; Solomon Islands, Magadan</option>
                    <option value="12.0">UTC+12:00 &mdash; New Zealand (NZST), Fiji</option>
                    <option value="12.75">UTC+12:45 &mdash; Chatham Islands (NZCT)</option>
                    <option value="13.0">UTC+13:00 &mdash; Tonga, Phoenix Islands</option>
                    <option value="14.0">UTC+14:00 &mdash; Line Islands, Kiribati</option>
                </select>
            </div>
            <div id="tzBadge" class="tz-badge" style="display:none;"></div>
            <br>
            <button class="btn-save-tz" id="saveTzBtn" onclick="saveTimezone()">&#x1F4BE; Save Timezone to Device</button>
        </div>
    </div>

    <div id="toast" class="toast">Saved Successfully</div>

    <script>
        // ============================================================
        // Timezone functions
        // ============================================================
        async function loadTimezone() {
            try {
                const res = await fetch('/api/timezone');
                const data = await res.json();
                const sel = document.getElementById('tzSelect');
                // Find closest matching option value
                let best = null, bestDiff = Infinity;
                for (const opt of sel.options) {
                    const diff = Math.abs(parseFloat(opt.value) - data.tz);
                    if (diff < bestDiff) { bestDiff = diff; best = opt; }
                }
                if (best) best.selected = true;
                updateTzBadge();
            } catch(e) {
                console.error('Failed to load timezone', e);
            }
        }

        function updateTzBadge() {
            const sel = document.getElementById('tzSelect');
            const badge = document.getElementById('tzBadge');
            if (!sel.value) return;
            const val = parseFloat(sel.value);
            const sign = val >= 0 ? '+' : '';
            badge.textContent = 'Active offset: UTC' + sign + val.toFixed(2);
            badge.style.display = 'inline-block';
        }

        document.getElementById('tzSelect').addEventListener('change', updateTzBadge);

        async function saveTimezone() {
            const tz = parseFloat(document.getElementById('tzSelect').value);
            try {
                const res = await fetch('/api/timezone', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ tz: tz })
                });
                if (res.ok) {
                    showToast('Timezone saved! UTC' + (tz >= 0 ? '+' : '') + tz.toFixed(2));
                } else {
                    alert('Failed to save timezone');
                }
            } catch(e) {
                console.error(e);
            }
        }

        // ============================================================
        // Waypoint functions
        // ============================================================
        async function fetchWaypoints() {
            try {
                const res = await fetch('/api/waypoints');
                const data = await res.json();
                renderWaypoints(data);
            } catch (e) {
                console.error('Failed to fetch', e);
            }
        }

        function renderWaypoints(waypoints) {
            const list = document.getElementById('waypointList');
            list.innerHTML = '';
            if (waypoints.length === 0) {
                list.innerHTML = '<p style="color: #94a3b8; text-align:center;">No waypoints saved.</p>';
                return;
            }
            
            waypoints.forEach((wp, index) => {
                const div = document.createElement('div');
                div.className = 'waypoint-item';
                div.innerHTML = `
                    <div class="waypoint-info">
                        <h3>${wp.n}</h3>
                        <p>${wp.lat.toFixed(6)}, ${wp.lon.toFixed(6)}</p>
                    </div>
                    <button class="btn-delete" onclick="deleteWaypoint(${wp.id})">&#x1F5D1;</button>
                `;
                list.appendChild(div);
            });
        }

        async function addWaypoint() {
            const name = document.getElementById('wpName').value;
            const lat = parseFloat(document.getElementById('wpLat').value);
            const lon = parseFloat(document.getElementById('wpLon').value);

            if (!name || isNaN(lat) || isNaN(lon)) {
                alert("Please fill all fields correctly.");
                return;
            }

            const data = { n: name, lat: lat, lon: lon };
            
            try {
                const res = await fetch('/api/waypoints', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(data)
                });
                
                if(res.ok) {
                    document.getElementById('wpName').value = '';
                    document.getElementById('wpLat').value = '';
                    document.getElementById('wpLon').value = '';
                    showToast('Waypoint saved!');
                    fetchWaypoints();
                } else {
                    alert("Failed to save (memory full?)");
                }
            } catch(e) {
                console.error(e);
            }
        }

        async function deleteWaypoint(id) {
            if(!confirm("Delete this waypoint?")) return;
            try {
                const res = await fetch('/api/waypoints?id=' + id, { method: 'DELETE' });
                if(res.ok) {
                    fetchWaypoints();
                }
            } catch(e) {
                console.error(e);
            }
        }

        function showToast(msg) {
            const t = document.getElementById('toast');
            t.textContent = msg || 'Saved Successfully';
            t.classList.add('show');
            setTimeout(() => { t.classList.remove('show'); }, 2500);
        }

        // Init
        loadTimezone();
        fetchWaypoints();
    </script>
</body>
</html>
)rawliteral";

#endif
