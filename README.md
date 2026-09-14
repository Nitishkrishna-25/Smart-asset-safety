# 🛡️ Smart Asset Safety & Data Logging System

> An STM32-based embedded system that watches an asset in real time, catches faults early, and logs & reports everything automatically — no human needed on-site. 🚀

**👤 Student:** Nitish Krishna M H
**🎓 Program:** B.E. Electronics and Communication Engineering, Sri Ramakrishna Engineering College, Coimbatore

![Status](https://img.shields.io/badge/status-in%20progress-yellow)
![Platform](https://img.shields.io/badge/platform-STM32-blue)
![RTOS](https://img.shields.io/badge/RTOS-FreeRTOS-green)

---

## ❗ Problem Statement

Assets like vehicles, machinery, or transported cargo are often left unmonitored once deployed. Faults such as overheating, over-current, or physical impact go unnoticed until real damage or loss has already happened. This project builds a **low-cost, self-contained unit** that continuously watches an asset, catches trouble early, and alerts a remote party with the asset's exact location — automatically. 📍

---

## ⚙️ What the System Does

- 🌡️ Continuously monitors temperature, current draw, and motion/vibration
- ⚠️ Detects abnormal conditions — overheating, overcurrent, crash/tilt events
- 🕒 Logs every event with an accurate RTC timestamp to non-volatile flash
- 📡 Sends GPS-tagged alerts over GSM the moment a fault occurs
- 🔌 Automatically disables the load/motor during a critical fault

---

## 🧩 Technologies Used

| Category | Tech |
|---|---|
| 🧠 Microcontroller | STM32 (Cortex-M) |
| ⏱️ RTOS | FreeRTOS |
| 📶 Sensing | ADC (temp, current), I2C (MPU6050, RTC) |
| 📡 Communication | UART (GPS, GSM) |
| 💾 Storage | SPI (external flash logging) |
| 🎛️ Actuation | GPIO + PWM (load control) |

---

## 🖼️ System Architecture

![Block Diagram](./assets/block-diagram.png)

---

## ✅ Feasibility

All required peripherals — ADC, I2C, UART, SPI, GPIO/PWM — sit on a single STM32 chip, so no extra bridging hardware is needed. FreeRTOS runs fault detection at a higher priority than routine tasks like logging, so response stays real-time even as features grow. Every module (GPS, GSM, MPU6050, RTC, SPI flash) is standard, well-documented, and low-cost — keeping the build practical within the internship timeline. 👍

---

## 🌟 Usefulness

Real-world application across:
- 📦 **Logistics** — cargo condition & location tracking
- 🏭 **Manufacturing** — early fault detection on machinery
- 🚚 **Fleet management** — remote alerts without manual checks

Combining sensing, real-time fault handling, non-volatile logging, and remote alerting into one compact unit makes this a genuine industry use case, not just an academic exercise. 🎯

---

## 📌 Project Status

🔄 Currently in the **requirements analysis & system architecture** phase — hardware bring-up and firmware development follow next.
