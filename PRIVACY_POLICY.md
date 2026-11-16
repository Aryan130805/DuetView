# Privacy Policy for DuetView

**Effective Date:** November 16, 2026  
**Last Updated:** November 16, 2026

DuetView ("the Application", "we", "us", or "our") is developed by Aryan ([@Aryan130805](https://github.com/Aryan130805/DuetView)). This Privacy Policy explains our practices regarding user information and privacy when you use the DuetView Android application and companion Windows PC host.

---

## 1. Zero Cloud Architecture & Peer-to-Peer Streaming

DuetView is designed from the ground up as a **100% local, peer-to-peer (P2P)** companion display solution:

- **No Cloud Servers:** We do not operate, host, or route your video, touch inputs, or network packets through any remote cloud servers.
- **Local Network Only:** All video streaming, touch interactions, keyboard events, and display control stay strictly within your local home/office Wi-Fi network or direct USB cable tether.
- **No Account Required:** You do not need to register, log in, or provide personal credentials to use DuetView.

---

## 2. Information We Collect

**We do not collect, store, sell, or transmit any personal data or usage analytics.**

- **No Personal Identifiable Information (PII):** No names, emails, phone numbers, location data, or device identifiers are collected.
- **No Telemetry or Tracking:** DuetView contains **zero** third-party advertising SDKs, analytics frameworks, or tracking cookies.

---

## 3. Device Permissions & How They Are Used

DuetView requests only the minimum necessary permissions required to provide its core functionality:

### 3.1 Camera (`android.permission.CAMERA`)
- **Purpose:** Used solely for scanning the pairing QR code displayed by the DuetView PC host on your Windows monitor.
- **Data Handling:** Camera frames are processed strictly in real-time on your device using on-device ML/ZXing barcode scanning. Camera feeds are **never saved, recorded, uploaded, or transmitted anywhere**. Once the QR code is decoded, camera access is immediately released.
- **Optional:** You can skip camera permission entirely and connect by manually entering your PC's IP address.

### 3.2 Network & Wi-Fi (`INTERNET`, `ACCESS_NETWORK_STATE`, `ACCESS_WIFI_STATE`, `CHANGE_WIFI_MULTICAST_STATE`)
- **Purpose:** Used to detect your local IP address and communicate with the DuetView Windows PC host over local Wi-Fi or USB tethering (TCP port 52025 and UDP discovery port 52026).
- **Data Handling:** Communication is direct between your Android phone/tablet and your Windows PC. No traffic leaves your local area network (LAN).

---

## 4. Local Data Storage

DuetView stores only minimal, non-sensitive preferences locally on your device using Android's standard private storage (`SharedPreferences`):
- Saved PC hostnames and local IP addresses (for convenient one-tap reconnection).
- Display preferences (e.g. resolution mode, target framerate, scaling).

This data never leaves your device and is deleted automatically if you uninstall the application or clear app storage.

---

## 5. Children's Privacy

DuetView does not address anyone under the age of 13. We do not knowingly collect personal identifiable information from children, nor does our application contain age-inappropriate content or advertising.

---

## 6. Security

Because DuetView operates over your private local network, your data security is safeguarded by your local Wi-Fi encryption (WPA2/WPA3) or direct physical USB tethering. Screen streaming is framed with binary SS02 framing and session-level pairing validation to prevent unauthorized network interception.

---

## 7. Open Source & Transparency

DuetView is an open-source project. You can inspect the complete source code, security posture, and network implementation anytime on GitHub:  
👉 **[https://github.com/Aryan130805/DuetView](https://github.com/Aryan130805/DuetView)**

---

## 8. Changes to This Privacy Policy

We may update this Privacy Policy from time to time. Any updates will be posted to the official repository at the link above.

---

## 9. Contact Us

If you have any questions or feedback regarding this Privacy Policy or DuetView's data practices, please reach out via GitHub Issues:  
📧 **GitHub:** [https://github.com/Aryan130805/DuetView/issues](https://github.com/Aryan130805/DuetView/issues)
