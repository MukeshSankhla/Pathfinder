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
        .form-group {
            margin-bottom: 16px;
        }
        label {
            display: block;
            margin-bottom: 8px;
            font-size: 0.9rem;
            color: var(--text-muted);
        }
        input {
            width: 100%;
            padding: 12px;
            background: rgba(0, 0, 0, 0.2);
            border: 1px solid var(--border-color);
            border-radius: 8px;
            color: white;
            font-family: inherit;
            box-sizing: border-box;
            transition: all 0.3s;
        }
        input:focus {
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
        }
        button:hover {
            background: var(--primary-hover);
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
        }
        .toast {
            position: fixed;
            bottom: 20px;
            left: 50%;
            transform: translateX(-50%);
            background: #10b981;
            color: white;
            padding: 12px 24px;
            border-radius: 30px;
            opacity: 0;
            transition: opacity 0.3s;
            pointer-events: none;
        }
    </style>
</head>
<body>

    <div class="container">
        <h1>Waypoint Nav</h1>

        <div class="card">
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
            <button onclick="addWaypoint()">Save Waypoint</button>
        </div>

        <div class="card">
            <h2 style="margin-top:0; font-size:1.2rem; margin-bottom:16px;">Saved Locations</h2>
            <div id="waypointList">
                <p style="color: #94a3b8; text-align:center;">Loading...</p>
            </div>
        </div>
    </div>

    <div id="toast" class="toast">Saved Successfully</div>

    <script>
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
                    <button class="btn-delete" onclick="deleteWaypoint(${index})">Del</button>
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
                    showToast();
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

        function showToast() {
            const t = document.getElementById('toast');
            t.style.opacity = '1';
            setTimeout(() => { t.style.opacity = '0'; }, 2000);
        }

        fetchWaypoints();
    </script>
</body>
</html>
)rawliteral";

#endif
