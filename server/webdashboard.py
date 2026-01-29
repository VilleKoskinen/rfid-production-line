from flask import Flask, render_template_string
import sqlite3
from datetime import datetime
import json

app = Flask(__name__)
DB_NAME = "rfid_logs.db"

# ==========================================
# 1. CONFIGURATION
# ==========================================

# Map MULTIPLE MAC addresses to the SAME logical Station Name
STATION_MAPPING = {
    # -- STATION 1 (Two Operators) --
    "C1:D0:91:F8:35:0C": "Station 1 (Start)",
    "E9:60:02:9C:8A:7D": "Station 1 (Start)", 

    # -- STATION 2 (Add your 2nd MAC here) --
    "DD:05:25:D1:A1:0C": "Station 2",
    "CB:C1:86:F3:E3:97": "Station 2", 

    # -- STATION 3  Add your 2nd MAC here) --
    "AA:BB:CC:DD:EE:FF": "Station 3",
    # "XX:XX:XX:XX:XX:XX": "Station 3",

    # -- STATION 4 (Two Operators - Finished Goods) --
    "AA:BB:CC:DD:EE:FF": "Station 4 (End)",
    # "XX:XX:XX:XX:XX:XX": "Station 4 (End)", 
}

# The specific name that counts as a finished unit
FINAL_STATION_NAME = "Station 4 (End)"

# LOGIC: Define the Visual Order of stations for the Flow Chart
# (Since the mapping above has duplicates, we list the unique names in order here)
STATION_ORDER = ["Station 1 (Start)", "Station 2", "Station 3", "Station 4 (End)"]

# AUTOMATICALLY FIND ALL MACS BELONGING TO FINAL STATION
# This creates a list like ['CB:C1...', 'XX:XX...']
FINAL_STATION_MACS = [mac for mac, name in STATION_MAPPING.items() if name == FINAL_STATION_NAME]

# ==========================================
# 2. HTML TEMPLATE
# ==========================================
HTML_TEMPLATE = """
<!DOCTYPE html>
<html>
<head>
    <title>Production Line Monitor</title>
    <meta http-equiv="refresh" content="5">
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        body { font-family: 'Segoe UI', sans-serif; background: #1e293b; color: white; padding: 20px; margin: 0; }
        .container { max-width: 1400px; margin: 0 auto; }
        
        .top-row { display: flex; gap: 20px; margin-bottom: 20px; flex-wrap: wrap;}
        .box { background: #334155; padding: 20px; border-radius: 10px; flex: 1; min-width: 300px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); }
        
        .hero-box { 
            background: linear-gradient(135deg, #10b981 0%, #059669 100%); 
            padding: 30px; border-radius: 10px; text-align: center; color: white; flex: 0 0 250px;
            display: flex; flex-direction: column; justify-content: center;
        }
        .hero-number { font-size: 5em; font-weight: bold; line-height: 1; margin-bottom: 10px;}
        .hero-label { font-size: 1.2em; opacity: 0.9; text-transform: uppercase; letter-spacing: 1px; }

        .flow-container { 
            display: flex; gap: 10px; margin-bottom: 20px; 
            background: #334155; padding: 20px; border-radius: 10px;
            overflow-x: auto;
        }
        .station-card {
            background: #475569; flex: 1; padding: 15px; border-radius: 8px; text-align: center; min-width: 120px;
            border-top: 4px solid #64748b;
        }
        .station-card.final { border-top: 4px solid #10b981; }
        .station-count { font-size: 2em; font-weight: bold; }
        .station-name { color: #cbd5e1; font-size: 0.85em; }
        .arrow { font-size: 1.5em; color: #64748b; align-self: center; }

        table { width: 100%; border-collapse: collapse; background: #334155; border-radius: 10px; overflow: hidden; }
        th { background: #475569; padding: 12px; text-align: left; color: #cbd5e1; }
        td { padding: 12px; border-bottom: 1px solid #475569; color: #e2e8f0; }
        h3 { border-bottom: 1px solid #475569; padding-bottom: 10px; margin-top: 0; color: #94a3b8; font-size: 0.9em; text-transform: uppercase;}
        
        canvas { max-height: 250px; }
    </style>
</head>
<body>
<div class="container">

    <div class="top-row">
        <!-- 1. Total Units (Sum of all Final Station Readers) -->
        <div class="hero-box">
            <div class="hero-label">Units Today</div>
            <div class="hero-number">{{ finished_today }}</div>
            <div style="opacity: 0.7">{{ final_station_name }}</div>
        </div>

        <!-- 2. Hourly Chart -->
        <div class="box">
            <h3>Today's Hourly Output</h3>
            <canvas id="hourlyChart"></canvas>
        </div>

        <!-- 3. History Chart -->
        <div class="box">
            <h3>Last 7 Days Production</h3>
            <canvas id="historyChart"></canvas>
        </div>
    </div>

    <!-- MIDDLE SECTION: STATION FLOW -->
    <div class="flow-container">
        {% for name in station_order %}
            <div class="station-card {% if name == final_station_name %}final{% endif %}">
                <div class="station-name">{{ name }}</div>
                <!-- Logic: We look up the name in the aggregated counts dictionary -->
                <div class="station-count">{{ counts.get(name, 0) }}</div>
            </div>
            {% if not loop.last %} <div class="arrow">➔</div> {% endif %}
        {% endfor %}
    </div>

    <!-- BOTTOM SECTION: LOGS -->
    <div class="box">
        <h3>Live Activity Feed</h3>
        <table>
            <thead>
                <tr><th>Time</th><th>Station</th><th>Carrier UID</th></tr>
            </thead>
            <tbody>
                {% for row in logs %}
                <tr>
                    <td>{{ row['timestamp'].split(' ')[1] }}</td>
                    <td>{{ location_map.get(row['mac_address'], 'Unknown') }}</td>
                    <td style="font-family: monospace">{{ row['uid'] }}</td>
                </tr>
                {% endfor %}
            </tbody>
        </table>
    </div>

</div>

<script>
    const hourlyLabels = {{ hourly_labels | safe }};
    const hourlyData = {{ hourly_data | safe }};
    const historyLabels = {{ history_labels | safe }};
    const historyData = {{ history_data | safe }};

    new Chart(document.getElementById('hourlyChart'), {
        type: 'line',
        data: {
            labels: hourlyLabels,
            datasets: [{
                label: 'Units / Hour',
                data: hourlyData,
                borderColor: '#38bdf8',
                backgroundColor: 'rgba(56, 189, 248, 0.2)',
                borderWidth: 2,
                tension: 0.4,
                fill: true,
                pointRadius: 3
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: { legend: { display: false } },
            scales: { y: { beginAtZero: true, grid: { color: '#475569' } }, x: { grid: { display: false } } }
        }
    });

    new Chart(document.getElementById('historyChart'), {
        type: 'bar',
        data: {
            labels: historyLabels,
            datasets: [{
                label: 'Daily Production',
                data: historyData,
                backgroundColor: '#10b981',
                borderRadius: 4
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: { legend: { display: false } },
            scales: { y: { beginAtZero: true, grid: { color: '#475569' } }, x: { grid: { display: false } } }
        }
    });
</script>
</body>
</html>
"""

# ==========================================
# 3. BACKEND LOGIC
# ==========================================

def get_db():
    conn = sqlite3.connect(DB_NAME)
    conn.row_factory = sqlite3.Row
    return conn

# Helper to format list for SQL IN clause
def get_sql_mac_list(mac_list):
    return "'" + "','".join(mac_list) + "'"

@app.route('/')
def index():
    conn = get_db()
    today_str = datetime.now().strftime("%Y-%m-%d")
    
    # --- 1. STATION FLOW (Aggregated) ---
    stats = conn.execute(f"SELECT mac_address, COUNT(*) as count FROM scans WHERE timestamp LIKE '{today_str}%' GROUP BY mac_address").fetchall()
    
    # Initialize counts to 0
    station_counts = {name: 0 for name in STATION_ORDER}
    
    for row in stats:
        mac = row['mac_address']
        if mac in STATION_MAPPING:
            name = STATION_MAPPING[mac]
            # ACCUMULATE COUNTS: If two MACs map to "Station 1", add them together
            station_counts[name] += row['count']

    finished_today = station_counts.get(FINAL_STATION_NAME, 0)

    # --- 2. CHART QUERY PREP ---
    # We need to query for ANY of the MAC addresses that belong to the final station
    if FINAL_STATION_MACS:
        mac_sql_string = get_sql_mac_list(FINAL_STATION_MACS)
        
        # --- HOURLY DATA ---
        hourly_query = f"""
            SELECT strftime('%H', timestamp) as hour, COUNT(*) as count 
            FROM scans 
            WHERE mac_address IN ({mac_sql_string}) AND timestamp LIKE '{today_str}%'
            GROUP BY hour ORDER BY hour
        """
        hourly_rows = conn.execute(hourly_query).fetchall()
        h_labels = [r['hour'] + ":00" for r in hourly_rows]
        h_data = [r['count'] for r in hourly_rows]

        # --- HISTORY DATA ---
        history_query = f"""
            SELECT date(timestamp) as day, COUNT(*) as count 
            FROM scans 
            WHERE mac_address IN ({mac_sql_string})
            GROUP BY day ORDER BY day DESC LIMIT 7
        """
        history_rows = conn.execute(history_query).fetchall()
        history_rows.reverse()
        hist_labels = [r['day'] for r in history_rows]
        hist_data = [r['count'] for r in history_rows]
    else:
        h_labels, h_data, hist_labels, hist_data = [], [], [], []

    # --- 3. LOGS ---
    logs = conn.execute("SELECT * FROM scans ORDER BY id DESC LIMIT 8").fetchall()
    conn.close()
    
    return render_template_string(HTML_TEMPLATE, 
                                  finished_today=finished_today,
                                  final_station_name=FINAL_STATION_NAME,
                                  counts=station_counts,
                                  station_order=STATION_ORDER,
                                  location_map=STATION_MAPPING,
                                  logs=logs,
                                  hourly_labels=json.dumps(h_labels),
                                  hourly_data=json.dumps(h_data),
                                  history_labels=json.dumps(hist_labels),
                                  history_data=json.dumps(hist_data))

if __name__ == '__main__':
    app.run(debug=True, host='0.0.0.0', port=5000)
