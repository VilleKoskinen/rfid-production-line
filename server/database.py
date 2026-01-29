import sqlite3
from datetime import datetime

DB_NAME = "rfid_logs.db"

def init_db():
    """Create the table if it doesn't exist"""
    conn = sqlite3.connect(DB_NAME)
    c = conn.cursor()
    c.execute('''
        CREATE TABLE IF NOT EXISTS scans (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            uid TEXT,
            mac_address TEXT,
            rssi INTEGER
        )
    ''')
    conn.commit()
    conn.close()

def log_scan(uid, rssi, mac):
    """Insert a new scan event"""
    conn = sqlite3.connect(DB_NAME)
    c = conn.cursor()
    
    # Get readable timestamp
    now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    
    c.execute("INSERT INTO scans (timestamp, uid, rssi, mac_address) VALUES (?, ?, ?, ?)",
              (now, uid, rssi, mac))
    conn.commit()
    conn.close()
    print(f"   [DB] Saved UID: {uid} from {mac}")

# Run this file once directly to create the database
if __name__ == "__main__":
    init_db()
    print("Database file created.")