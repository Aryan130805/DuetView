# Google Play Store Publishing Guide for DuetView

This guide provides everything needed to submit and publish **DuetView** to the **Google Play Console**.

---

## 1. App Store Listing Metadata

### 1.1 App Name & Tagline
- **App Name (Title):** `DuetView: Second Screen & Mirror` *(under 30 characters)*
- **Short Description (Tagline):** `Turn your Android into a fast second monitor or desktop mirror for Windows PC.` *(under 80 characters)*

### 1.2 Full Description (Store Listing)
```markdown
DuetView transforms your Android phone, tablet, or foldable into a high-performance second monitor and desktop display for your Windows 10/11 PC.

Connect seamlessly over high-speed local Wi-Fi or plug in via USB tethering for ultra-low latency (< 5ms) with zero lag.

🌟 KEY FEATURES

• Dual Display Modes:
  - Extended Monitor: True virtual secondary screen powered by Microsoft IddCx display architecture. Expand your workspace across dual monitors.
  - Desktop Mirror: Low-latency desktop mirroring for presentations, games, and drawing.

• Ultra-Low Latency USB Mode:
  - Connect with a standard USB cable using wired tethering.
  - Sub-5ms input response with zero Wi-Fi congestion or router interference.

• Hardware-Accelerated Performance:
  - Windows Media Foundation H.264 video encoding paired with Android MediaCodec hardware decoding.
  - Smooth 60 FPS, 90 FPS, and 120 FPS high refresh-rate streaming support.

• Instant QR Code Pairing:
  - Pair in seconds using cryptographic QR tokens. No complex IP configuration required.

• Touchscreen & Multi-Touch Input:
  - Control your Windows PC with natural touch gestures, mouse clicks, and scrolling.

• 100% Private & Peer-to-Peer:
  - Zero cloud servers. Your video stream and input controls never leave your local network.

📋 HOW TO GET STARTED
1. Download and run the free companion host "DuetView PC" on your Windows computer (available on GitHub: https://github.com/Aryan130805/DuetView).
2. Open DuetView on your Android device.
3. Connect wirelessly by scanning the pairing QR code, or plug in via USB cable and enable USB Tethering.
4. Enjoy your brand new second display!
```

### 1.3 Category & Tags
- **Application Category:** `Tools` or `Productivity`
- **Tags:** `Second Screen`, `Dual Monitor`, `Remote Desktop`, `Display Mirror`, `Productivity`

---

## 2. Store Listing Graphics

| Asset | Dimensions | Format | Notes |
| :--- | :--- | :--- | :--- |
| **App Icon** | 512 x 512 px | PNG (32-bit, max 1024 KB) | Located at `apps/android_app/assets/images/duetview_icon.png` |
| **Feature Graphic** | 1024 x 500 px | PNG or JPEG | Banner shown at the top of your store listing |
| **Phone Screenshots** | 1080 x 1920 px or 1080 x 2400 px | PNG / JPEG (Min 2, Max 8) | Home screen, Connection screen, Streaming screen |
| **Tablet Screenshots** | 1920 x 1200 px or 2560 x 1600 px | PNG / JPEG | Recommended for 7" and 10" tablet display categories |

---

## 3. Google Play Console Policy Questionnaires

### 3.1 Privacy Policy URL
Set to the raw or rendered GitHub URL:
```
https://github.com/Aryan130805/DuetView/blob/master/PRIVACY_POLICY.md
```

### 3.2 App Access
- **Selection:** `All functionality is available without special access` *(No login or account is required)*.

### 3.3 Ads
- **Selection:** `No, my app does not contain ads`.

### 3.4 Content Rating
- Complete the questionnaire.
- Violence, sexual content, profanity, drugs: `No` to all.
- User-to-user communication: `No`.
- Shares location: `No`.
- **Result:** Rated `PEGI 3` / `Everyone` / `All Ages`.

### 3.5 Target Audience & Content
- Target age groups: `18 and over`, `13-17` (General productivity tool).
- Appeals to children: `No`.

### 3.6 Data Safety Questionnaire
- **Does your app collect or share any of the required user data types?**  
  👉 **Select `No`**.
- DuetView does not collect any user data, personal info, location, diagnostics, or telemetry. Streaming is strictly peer-to-peer on the local network.

---

## 4. Keystore & Production Release Signing

To sign your Android App Bundle (`.aab`) for Google Play:

### Step 1: Generate your release keystore (one-time setup)
Run the following command in terminal:
```bash
keytool -genkey -v -keystore duetview-release.jks -keyalg RSA -keysize 2048 -validity 10000 -alias duetview
```
*Keep your keystore file and passwords safe! If you lose them, you cannot update your app on the Play Store.*

### Step 2: Configure `key.properties`
In `apps/android_app/android/`, create a file named `key.properties` (this is automatically ignored by git):
```properties
storePassword=YOUR_KEYSTORE_PASSWORD
keyPassword=YOUR_KEY_PASSWORD
keyAlias=duetview
storeFile=duetview-release.jks
```

### Step 3: Build the production Android App Bundle (AAB)
```bash
cd apps/android_app
flutter build appbundle --release
```
The signed bundle will be generated at:
```
apps/android_app/build/app/outputs/bundle/release/app-release.aab
```

### Step 4: Upload to Google Play Console
1. In Google Play Console, go to **Release** > **Production** (or **Internal Testing**).
2. Click **Create new release**.
3. Upload `app-release.aab`.
4. Enter release notes (e.g. `Initial release of DuetView: high-performance secondary display and desktop mirror`).
5. Review and roll out!
