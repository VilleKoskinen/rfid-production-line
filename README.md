# IoT Production Line Tracker (RFID & QR)

An Industrial IoT (IIoT) solution designed to track manufacturing units through a multi-stage production line. This system combines low-level firmware for hardware tracking with a high-level web dashboard for real-time visualization.

System Architecture

The system consists of two main components:

1.  **Edge Nodes (Firmware):** nRF52840 microcontrollers running the **Zephyr RTOS**. These nodes interface with RFID scanners and QR code readers to detect units as they pass through various stages of the production line.
2.  **Gateway & Dashboard (Server):** A **Raspberry Pi** (or PC) running a **Flask** web server. It collects data from the edge nodes, processes the production flow, and serves a real-time dashboard to visualize unit locations and bottlenecks.

Project Structure

*   **`/firmware`**: Zephyr-based C/C++ code for the nRF52840. Includes driver logic for RFID/QR modules and communication protocols.
*   **`/server`**: Python-based Flask application. Includes API endpoints for data ingestion and the frontend dashboard templates.

Tech Stack

-   **Hardware:** nRF52840 (Nordic Semiconductor), Raspberry Pi.
-   **Firmware:** Zephyr RTOS, C.
-   **Backend:** Python, Flask, Socket.IO (for real-time updates).
-   **Frontend:** HTML5, CSS3, JavaScript.
