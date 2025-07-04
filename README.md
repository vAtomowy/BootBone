# BootBone

**BootBone** is a lightweight, extensible program core designed for a family of IoT devices based on the **ESP32-C3** platform. It serves as the foundational bootloader and runtime environment for your main application.

## 🔧 Key Features

- **Initial Web Configuration Portal**
  - On first startup (or after a reset), BootBone launches a minimal web server.
  - It provides a user-friendly web interface to configure Wi-Fi credentials.
  - Once configured, it automatically jumps to the main application.

- **Main Application Boot and OTA Updates**
  - Seamlessly loads the main application after setup.
  - Includes support for **remote Over-The-Air (OTA) updates** of the main app.

- **FreeRTOS-Based Background Services**
  - BootBone includes FreeRTOS tasks that continue running even after the main app is launched.
  - These services are exposed as a reusable library and include:
    - WebSocket communication with a defined server.
    - Encryption and authentication mechanisms.
    - Access to the **Non-Volatile Storage (NVS)**.

- **Shared Persistent Configuration Layer**
  - Manages a shared memory space for storing device-specific "concrete" values such as:
    - Device serial number.
    - Associated device type.
  - This data can be safely accessed by both the boot core and the main application.

## 🧩 Modular and Extensible

BootBone is designed to act as a reusable component in your firmware stack:
- It acts as a **library** linked with your application.
- Handles low-level and shared concerns common across your fleet of devices.
- Keeps your main firmware clean and focused only on application logic.

## 📦 Use Cases

- Rapid prototyping of connected devices using ESP32-C3.
- Secure boot + persistent identity management.
- Separation of core platform code from product-specific features.

---

Start building smarter IoT systems with a reliable, extensible, and secure foundation – start with **BootBone**.
