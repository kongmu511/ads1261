ZPlate Mobile (Flutter)

This scaffold implements a mobile version of the ZPlate GUI using Flutter and Syncfusion charts.

Features
- UDP listener on port 5555 to receive data packets from the ESP32 (same packet layout as desktop GUI)
- Real-time line charts for 4 channels using `syncfusion_flutter_charts`
- Android Wi‑Fi scanning and connect/disconnect using `wifi_iot` (Android only; iOS has platform restrictions)
- Start/Stop streaming commands sent via UDP to the ESP32 at 192.168.4.1:5555

Limitations
- Wi‑Fi scanning/connection is Android-only and requires runtime permissions (ACCESS_FINE_LOCATION) and manifest permissions.
- Listing clients connected to the ESP32 AP is not implemented; mobile OSes do not expose an easy cross-platform API for that.

How to run
1. Install Flutter and ensure `flutter` is on PATH.
2. From this folder run:

```bash
cd mobile/flutter_app
flutter pub get
flutter run -d <your-device-or-emulator>
```

Android manifest notes
Add the following permissions to `android/app/src/main/AndroidManifest.xml` (inside `<manifest>`):

```xml
<uses-permission android:name="android.permission.INTERNET" />
<uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />
<uses-permission android:name="android.permission.ACCESS_WIFI_STATE" />
<uses-permission android:name="android.permission.CHANGE_WIFI_STATE" />
<uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" />
```

You may need to request runtime location permission on Android 6+ for Wi‑Fi scanning. The `wifi_iot` plugin provides helper calls.

Runtime permissions and local test
- The app requests `ACCESS_FINE_LOCATION` at startup to allow scanning on Android. You must still grant the permission on-device.
- A local test-data generator button is available in the Stream tab to produce synthetic waveforms for quick verification when the ESP32 isn't reachable.

UDP test sender
- A small UDP test sender is included at `mobile/flutter_app/test_udp_sender.py`. Run it on your PC to send packets to the device running the app:

```bash
python mobile/flutter_app/test_udp_sender.py --ip 192.168.4.1 --port 5555 --rate 100
```

This helps verify charts without hardware.

UDP packet interpretation
- The app expects 20-byte little-endian packets matching the desktop app:
  - uint32 timestamp_us
  - int32 ch1
  - int32 ch2
  - int32 ch3
  - int32 ch4

Customize
- Change the target ESP32 IP by editing `_apIp` in `lib/main.dart`.

