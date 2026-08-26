Firmware designed for pretty much any ESP32 board compatible with the Arduino SDK, it captures and parses environmental radio signals while staying completely undetectable from the environment.

It can parse protocols like **Apple Continuity** and **Microsoft CDP**, while operating in a deep sleep state cycle.

It runs several heuristics to classify devices (**Windows Desktops/Laptops with its OS version, Xbox One, Android, iOS**), continuity events (**AirDrop, AirPods proximity pairing, and Siri wake triggers**), and active offline-finding beacons (**AirTags/SmartTags**)

It can differentiate the battery level, detect spoofed and simulated packets, detect both private and public random MAC addresses, detect if the owner of an Apple device is connected or if the device is separated from the owner, and infer the distance from all the devices it discovers in meters using RSSI.

The firmware is compiled without **Bluetooth 5.x** support because I tested this on a ULP coprocessor to ensure it works with practically any Espressif's board. However, if you want BT 5.x support, set `CONFIG_BT_NIMBLE_EXT_ADV` and my firmware will automatically compile the sketch with it and report primary/secondary PHY along its periodic interval.

Firmware should be flashed by using the **USB-to-UART** port:

<img width="2160" height="2880" alt="image" src="https://github.com/user-attachments/assets/99a5d4ac-847c-44df-92c6-65596404a0a6" />

It automatically performs a clean radio driver teardown to prevent driver lockups during warm boot, and I used 115200 baud to flash the firmware. If the hardware reset via RTS pin doesn't work (happens on some boards like mine), try pressing the BOOT button during 3s and disconnect/connect the cable, and after pressing the EN button you should start receiving events via the COM port (make sure to install your drivers first, in my case CP210x):

<img width="1086" height="1005" alt="image" src="https://github.com/user-attachments/assets/fdbaab95-ca22-42fd-917b-c016ca968fdd" />

`Example in Serial Monitor:`

<img width="992" height="409" alt="image" src="https://github.com/user-attachments/assets/a6b32371-bc9e-4ce9-b642-fcf673c95ea3" />

<img width="1007" height="962" alt="image" src="https://github.com/user-attachments/assets/5b8599d7-534c-463c-9850-e7e7bd49c95b" />


It also supports whitelisting MAC addresses of Wi-Fi and Bluetooth APs: At the top of the main code file, configure your addresses to filter alerts:

```cpp
const int KNOWN_WIFI_COUNT = 2;
const char* known_wifi[] = {
  "aa:bb:cc:dd:ee:ff",  
  "11:22:33:44:55:66"   
};

const int KNOWN_BLE_COUNT = 2;
const char* known_ble[] = {
  "ff:ee:dd:cc:bb:aa",  
  "66:55:44:33:22:11"   
};
```

You can also tune the deep sleep cycle before waking up the hardware beacon, I put 5 minutes. 
