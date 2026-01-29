import asyncio
import time
from bleak import BleakScanner
import database

# Initialize DB on startup
database.init_db()

# Debounce storage
last_seen = {}
DEBOUNCE_SECONDS = 5

def detection_callback(device, advertisement_data):
    # Filter for our specific device name
    if device.name == "RFID_Reader":

        # Check for Manufacturer Data (0xFFFF == 65535)
        if 65535 in advertisement_data.manufacturer_data:
            data = advertisement_data.manufacturer_data[65535]

            status = data[0]

            if status == 1:
                uid_bytes = data[1:5]
                uid_hex = uid_bytes.hex().upper()
                
                # Get Signal and MAC Address
                rssi = advertisement_data.rssi
                mac_address = device.address

                now = time.time()

                # --- Debounce Logic ---
                last_time = last_seen.get(uid_hex, 0)
                if now - last_time < DEBOUNCE_SECONDS:
                    return  # Skip repeated UID detections

                last_seen[uid_hex] = now
                # -----------------------

                print(f"READER DETECTED! Signal: {rssi}dBm | UID: {uid_hex} | MAC: {mac_address}")
                
                # SAVE TO DATABASE
                database.log_scan(uid_hex, rssi, mac_address)

            else:
                pass

async def main():
    print("Scanning for RFID Readers...")
    scanner = BleakScanner(detection_callback)
    await scanner.start()

    # Run forever
    while True:
        await asyncio.sleep(1)

if __name__ == "__main__":
    asyncio.run(main())
