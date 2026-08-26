#include <WiFi.h>
#include <NimBLEDevice.h> 
#include <ctype.h>

#define SLEEP_TIME  300  
#define uS_TO_S_FACTOR 1000000ULL 

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

bool isKnownWiFi(const char* mac) {
    if (mac == nullptr) return false;
    for (int i = 0; i < KNOWN_WIFI_COUNT; i++) {
        if (strcasecmp(mac, known_wifi[i]) == 0) return true;
    }
    return false;
}

bool isKnownBLE(const char* mac) {
    if (mac == nullptr) return false;
    for (int i = 0; i < KNOWN_BLE_COUNT; i++) {
        if (strcasecmp(mac, known_ble[i]) == 0) return true;
    }
    return false;
}

void scanWiFi() {
    Serial.println("\n[*] Starting Wi-Fi Scan...");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    int16_t networks = WiFi.scanNetworks();

    if (networks == WIFI_SCAN_FAILED) {
        Serial.println("[-] Critical Error: Wi-Fi scan failed at physical level.");
        return;
    }
    else if (networks == WIFI_SCAN_RUNNING) {
        Serial.println("[-] Error: Wi-Fi transceiver is still busy with another task.");
        return;
    }
    else if (networks == 0) {
        Serial.println("[-] No Wi-Fi networks found in the area.");
    }
    else if (networks > 0) {
        Serial.printf("[+] Found %d Wi-Fi networks.\n", networks);
        for (int i = 0; i < networks; ++i) {
            String bssid = WiFi.BSSIDstr(i);
            String ssid = WiFi.SSID(i);
            int32_t rssi = WiFi.RSSI(i);

            if (bssid.length() == 0) continue;

            if (!isKnownWiFi(bssid.c_str())) {
                Serial.printf("[WI-FI ALERT] Unknown network -> SSID: %s | MAC: %s | Signal: %d dBm\n",
                    ssid.c_str(), bssid.c_str(), rssi);
            }
            else {
                Serial.printf("[WI-FI INFO] Known detected: %s\n", ssid.c_str());
            }
        }
    }

    WiFi.scanDelete();
}

float estimateDistance(int rssi, int txPower) {
    if (txPower > 0 || txPower < -100) {
        txPower = -59;
    }
    float n = 3.2;
    return pow(10, ((float)(txPower - rssi) / (10 * n)));
}

String getAppleActionDescription(uint8_t actionCode) {
    switch (actionCode) {
    case 0x00: return "Status Unknown / Idle";
    case 0x01: return "Activity Reporting Disabled";
    case 0x03: return "Locked / Screen Off";
    case 0x05: return "Audio Playing (Screen Locked)";
    case 0x07: return "Unlocked / Screen On (In-Use)";
    case 0x09: return "Video Streaming (Screen Active)";
    case 0x0A: return "Apple Watch on Wrist (Unlocked)";
    case 0x0B: return "Recent User Interaction";
    case 0x0D: return "Driving Focus Mode (In a Vehicle)";
    case 0x0E: return "Active Phone or FaceTime Call";
    default: return "Reserved / Unknown Code (" + String(actionCode) + ")";
    }
}

String getAdvTypeDescription(uint8_t advType) {
    switch (advType) {
    case 0x00: return "Connectable Undirected (ADV_IND)";
    case 0x01: return "Connectable Directed (ADV_DIRECT_IND)";
    case 0x02: return "Scannable Undirected (ADV_SCAN_IND)";
    case 0x03: return "Non-Connectable Undirected (ADV_NONCONN_IND)";
    case 0x04: return "Scan Response (SCAN_RSP)";
    default: return "Extended / Unknown (0x" + String(advType, HEX) + ")";
    }
}

String getAddrTypeDescription(uint8_t addrType) {
    switch (addrType) {
    case 0x00: return "Public (Static)";
    case 0x01: return "Random (Private Static/Resolvable/Non-Resolvable)";
    case 0x02: return "Public Identity";
    case 0x03: return "Random Identity";
    default: return "Unknown (0x" + String(addrType, HEX) + ")";
    }
}

String getPhyDescription(uint8_t phy) {
    switch (phy) {
    case 1: return "LE 1M (Standard)";
    case 2: return "LE 2M (High Speed)";
    case 3: return "LE Coded (Long Range)";
    default: return "Unknown PHY (" + String(phy) + ")";
    }
}

String getDataStatusDescription(uint8_t status) {
    switch (status) {
    case 0: return "Complete";
    case 1: return "Incomplete (More data expected, open an issue if this flags)";
    case 2: return "Truncated (Incomplete, no more expected)";
    default: return "Unknown (" + String(status) + ")";
    }
}

void printHexDump(const std::vector<uint8_t>& payload) {
    Serial.println("  Raw Payload:");
    size_t len = payload.size();

    for (size_t i = 0; i < len; i += 16) {
        Serial.printf("    %04X:  ", i);

        for (size_t j = 0; j < 16; j++) {
            if (i + j < len) {
                Serial.printf("%02X ", payload[i + j]);
            }
            else {
                Serial.print("   ");
            }
            if (j == 7) Serial.print(" ");
        }

        Serial.print(" |");

        for (size_t j = 0; j < 16; j++) {
            if (i + j < len) {
                char c = (char)payload[i + j];
                if (isprint(c)) {
                    Serial.print(c);
                }
                else {
                    Serial.print('.');
                }
            }
            else {
                Serial.print(' ');
            }
        }
        Serial.println("|");
    }
}

void parseAndDecodeADStructures(const std::vector<uint8_t>& payload) {
    Serial.println("\n  [+] Decoded advertising data blocks:");
    size_t idx = 0;

    while (idx < payload.size()) {
        uint8_t len = payload[idx];
        if (len == 0) break;

        if (idx + 1 + len > payload.size()) {
            Serial.println("    [-] Warning: Malformed packet structure (exceeds frame boundaries).");
            break;
        }

        uint8_t type = payload[idx + 1];
        const uint8_t* val = &payload[idx + 2];
        uint8_t valLen = len - 1;

        String typeDesc = "Proprietary/Other";
        switch (type) {
        case 0x01: typeDesc = "Flags"; break;
        case 0x02: typeDesc = "Partial List of 16-bit Service UUIDs"; break;
        case 0x03: typeDesc = "Complete List of 16-bit Service UUIDs"; break;
        case 0x08: typeDesc = "Shortened Local Name"; break;
        case 0x09: typeDesc = "Complete Local Name"; break;
        case 0x0A: typeDesc = "TX Power Level"; break;
        case 0x16: typeDesc = "Service Data - 16-bit UUID"; break;
        case 0xFF: typeDesc = "Manufacturer Specific Data"; break;
        }

        Serial.printf("    Block type 0x%02X (%s) | Length: %d bytes\n", type, typeDesc.c_str(), len);

        Serial.print("      Hex  : ");
        for (int i = 0; i < valLen; i++) {
            Serial.printf("%02X ", val[i]);
        }
        Serial.println();

        bool containsPrintable = false;
        String asciiStr = "";
        for (int i = 0; i < valLen; i++) {
            char c = (char)val[i];
            if (isprint(c)) {
                asciiStr += c;
                containsPrintable = true;
            }
            else {
                asciiStr += '.';
            }
        }

        if (containsPrintable) {
            Serial.printf("      ASCII: \"%s\"\n", asciiStr.c_str());
        }

        idx += 1 + len;
    }
}

void scanBLE() {
    Serial.println("\n[*] Starting Bluetooth scan...");

    NimBLEDevice::init("");
    NimBLEScan* pBLEScan = NimBLEDevice::getScan();
    if (pBLEScan == nullptr) {
        Serial.println("[-] Error: Failed to instantiate BLE scanner.");
        return;
    }

    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);

    NimBLEScanResults foundDevices = pBLEScan->getResults(5000, false);
    int devices = foundDevices.getCount();
    Serial.printf("[+] Found %d BLE devices.\n", devices);

    for (int i = 0; i < devices; i++) {
        const NimBLEAdvertisedDevice* device = foundDevices.getDevice(i);
        if (device == nullptr) continue;

        std::string address_str = device->getAddress().toString();
        const char* mac_address = address_str.c_str();
        if (mac_address == nullptr || strlen(mac_address) == 0) continue;

        if (isKnownBLE(mac_address)) continue;

        Serial.println("\n======================================================================");
        Serial.printf("[DEVICE DETECTED] MAC: %s\n", mac_address);
        Serial.println("======================================================================");

        if (!device->getName().empty()) {
            Serial.printf("  Device Name  : %s\n", device->getName().c_str());
        }

        uint8_t addrType = device->getAddressType();
        Serial.printf("  Address Type : %s\n", getAddrTypeDescription(addrType).c_str());
        Serial.printf("  RSSI Signal  : %d dBm\n", device->getRSSI());
        Serial.printf("  Adv Type     : %s\n", getAdvTypeDescription(device->getAdvType()).c_str());
        Serial.printf("  Payload Len  : %u bytes\n", device->getAdvLength());

        Serial.printf("  Connectable  : %s\n", device->isConnectable() ? "Yes" : "No");
        Serial.printf("  Scannable    : %s\n", device->isScannable() ? "Yes" : "No");
        Serial.printf("  Legacy Adv   : %s\n", device->isLegacyAdvertisement() ? "Yes" : "No");

        if (device->getAdvFlags() != 0) {
            Serial.printf("  Adv Flags    : 0x%02X\n", device->getAdvFlags());
        }

#if CONFIG_BT_NIMBLE_EXT_ADV
        if (!device->isLegacyAdvertisement()) {
            Serial.println("  [+] BT 5.x Extended Advertising Parameters:");
            Serial.printf("    Set ID            : %d\n", device->getSetId());
            Serial.printf("    Primary PHY       : %s\n", getPhyDescription(device->getPrimaryPhy()).c_str());
            Serial.printf("    Secondary PHY     : %s\n", getPhyDescription(device->getSecondaryPhy()).c_str());
            Serial.printf("    Data Status       : %s\n", getDataStatusDescription(device->getDataStatus()).c_str());
            if (device->getPeriodicInterval() > 0) {
                Serial.printf("    Periodic Interval : %u ms\n", (uint32_t)device->getPeriodicInterval() * 5 / 4);
            }
        }
#endif

        int txPower = -59;
        if (device->haveTXPower()) {
            txPower = device->getTXPower();
            Serial.printf("  Advertised TX: %d dBm\n", txPower);
        }
        Serial.printf("  Est. Distance: %.2f meters (approx, ideally we should calculate RSSI in a loop over time.)\n", estimateDistance(device->getRSSI(), txPower));

        if (device->haveAppearance()) {
            Serial.printf("  Appearance ID: %d\n", device->getAppearance());
        }

        if (device->haveAdvInterval()) {
            Serial.printf("  Adv Interval : %u ms\n", (uint32_t)device->getAdvInterval() * 5 / 8);
        }

        if (device->haveConnParams()) {
            Serial.printf("  Preferred Min Connection Interval: %u ms\n", (uint32_t)device->getMinInterval() * 5 / 4);
            Serial.printf("  Preferred Max Connection Interval: %u ms\n", (uint32_t)device->getMaxInterval() * 5 / 4);
        }

        if (device->haveURI()) {
            Serial.printf("  Target URI   : %s\n", device->getURI().c_str());
        }

        if (device->haveTargetAddress()) {
            uint8_t targetCount = device->getTargetAddressCount();
            Serial.printf("  Target Addresses (%d):\n", targetCount);
            for (uint8_t t = 0; t < targetCount; t++) {
                Serial.printf("    Target %d: %s\n", t, device->getTargetAddress(t).toString().c_str());
            }
        }

        uint8_t serviceCount = device->getServiceUUIDCount();
        if (serviceCount > 0) {
            Serial.printf("  Advertised Services (%d):\n", serviceCount);
            for (uint8_t s = 0; s < serviceCount; s++) {
                Serial.printf("    UUID: %s\n", device->getServiceUUID(s).toString().c_str());
            }
        }

        uint8_t serviceDataCount = device->getServiceDataCount();
        if (serviceDataCount > 0) {
            Serial.printf("  Service Data Sets (%d):\n", serviceDataCount);
            for (uint8_t sd = 0; sd < serviceDataCount; sd++) {
                NimBLEUUID uuid = device->getServiceDataUUID(sd);
                std::string rawData = device->getServiceData(sd);
                Serial.printf("    UUID %s: ", uuid.toString().c_str());
                for (char c : rawData) {
                    Serial.printf("%02X ", (uint8_t)c);
                }
                Serial.println();
            }
        }

        uint8_t mfgCount = device->getManufacturerDataCount();
        if (mfgCount > 0) {
            Serial.printf("  Manufacturer Data Sets (%d):\n", mfgCount);
            for (uint8_t m = 0; m < mfgCount; m++) {
                std::string mfgData = device->getManufacturerData(m);
                uint16_t companyId = mfgData.length() >= 2 ? (uint16_t)((uint8_t)mfgData[1] << 8 | (uint8_t)mfgData[0]) : 0;
                Serial.printf("    Set %d (Company ID: 0x%04X) -> Hex: ", m, companyId);
                for (char c : mfgData) {
                    Serial.printf("%02X ", (uint8_t)c);
                }
                Serial.println();
            }
        }

        const std::vector<uint8_t>& payload = device->getPayload();

        printHexDump(payload);
        parseAndDecodeADStructures(payload);

        if (device->haveManufacturerData()) {
            std::string mfgData = device->getManufacturerData();
            if (mfgData.length() >= 4) {
                const uint8_t* data = (const uint8_t*)mfgData.data();
                uint16_t companyId = (data[1] << 8) | data[0];

                if (companyId == 0x0006) {
                    Serial.println("\n  [+] DECODING MICROSOFT CDP:");
                    uint8_t beaconType = data[2];
                    uint8_t deviceType = data[3];
                    String devTypeStr = "Unknown Microsoft Device";
                    switch (deviceType) {
                    case 1:  devTypeStr = "Xbox One"; break;
                    case 6:  devTypeStr = "Apple iPhone (Running MS App)"; break;
                    case 7:  devTypeStr = "Apple iPad (Running MS App)"; break;
                    case 8:  devTypeStr = "Android Device"; break;
                    case 9:  devTypeStr = "Windows 10/11 Desktop PC"; break;
                    case 11: devTypeStr = "Windows Phone"; break;
                    case 12: devTypeStr = "Linux Device"; break;
                    case 13: devTypeStr = "Windows IoT Device"; break;
                    case 14: devTypeStr = "Surface Hub"; break;
                    case 15: devTypeStr = "Windows Laptop"; break;
                    case 16: devTypeStr = "Windows Tablet"; break;
                    }
                    Serial.printf("    Beacon Type: 0x%02X\n", beaconType);
                    Serial.printf("    Device Type: %s (Code: 0x%02X)\n", devTypeStr.c_str(), deviceType);
                }

                if (companyId == 0x004C) {
                    Serial.println("\n  [+] DECODING APPLE SYSTEM SERVICES:");

                    size_t idx = 2;
                    while (idx + 2 <= mfgData.length()) {
                        uint8_t type = data[idx];
                        uint8_t len = data[idx + 1];

                        if (idx + 2 + len > mfgData.length()) break;

                        String desc = "Unknown Continuity Event";
                        switch (type) {
                        case 0x05: desc = "AirDrop"; break;
                        case 0x07: desc = "AirPods Proximity Pairing"; break;
                        case 0x08: desc = "Hey Siri Wake Trigger"; break;
                        case 0x0C: desc = "Handoff / Universal Clipboard Synchronization"; break;
                        case 0x0F: desc = "Nearby Action (Wi-Fi Sharing / Apple Pay Setup)"; break;
                        case 0x10: desc = "Nearby Info (Device Status & Telemetry)"; break;
                        case 0x12: desc = "Find My Locator (AirTag / Lost Apple Device Beacon)"; break;
                        }

                        Serial.printf("    Subtype [0x%02X] -> %s (Length: %d)\n", type, desc.c_str(), len);

                        Serial.print("      Sub-Payload: ");
                        for (int p = 0; p < len; p++) { Serial.printf("%02X ", data[idx + 2 + p]); }
                        Serial.println();

                        if (type == 0x10 && len >= 3) {
                            uint8_t statusByte = data[idx + 2];
                            uint8_t statusFlags = (statusByte >> 4) & 0x0F;
                            uint8_t actionCode = statusByte & 0x0F;

                            Serial.println("      Parsed Telemetry (0x10):");
                            Serial.printf("        - Action State: %s\n", getAppleActionDescription(actionCode).c_str());
                            Serial.printf("        - Screen State: %s\n", (actionCode == 0x03 || actionCode == 0x05) ? "Locked / Screen Off" : "Unlocked / Active / In-Use");
                            Serial.printf("        - Primary iCloud Device: %s\n", (statusFlags & 0x01) ? "Yes" : "No");
                            Serial.printf("        - AirDrop Receiving: %s\n", (statusFlags & 0x04) ? "Enabled (Everyone/Contacts)" : "Disabled / Off");
                        }

                        if (type == 0x12) {
                            if (len == 2) {
                                uint8_t statusByte = data[idx + 2];
                                uint8_t payloadVal = data[idx + 3];

                                if (statusByte == 0x00 && (payloadVal <= 0x05)) {
                                    Serial.println("      Parsed Tracking (0x12):");
                                    Serial.printf("        - WARNING: Spoofed / Simulated Beacon \n");
                                    Serial.println("        - Security State : False Flag (Mathematically incapable of tracking??)");
                                    Serial.printf("        - Fake ID Index  : %d\n", payloadVal);
                                }
                                else {
                                    uint8_t batteryBits = (statusByte >> 6) & 0x03;
                                    String batStr = "Unknown";
                                    switch (batteryBits) {
                                    case 0: batStr = "Full / High"; break;
                                    case 1: batStr = "Medium"; break;
                                    case 2: batStr = "Low"; break;
                                    case 3: batStr = "Critically Low"; break;
                                    }
                                    bool ownerConnected = (statusByte & 0x04) != 0;

                                    Serial.println("      Parsed Tracking (0x12):");
                                    Serial.println("        - Security State : Nearby Registered Mode (Owner close or linked within 15 min)");
                                    Serial.printf("        - Battery Level  : %s\n", batStr.c_str());
                                    Serial.printf("        - Owner Proximity: %s\n", ownerConnected ? "Connected/In range" : "Disconnected (Searching)");
                                }
                            }
                            else if (len >= 22) {
                                Serial.println("      Parsed Tracking (0x12):");
                                Serial.println("        - State: FIND MY TRACKER");
                                Serial.println("          Warning: Active tracking key found. Device is separated from owner.");
                            }
                            else {
                                Serial.println("      Parsed Tracking (0x12):");
                                Serial.printf("        - State: Undefined/Malformed Finder Packet (Length: %d)\n", len);
                            }
                        }

                        idx += 2 + len;
                    }
                }
            }
        }
    }
    Serial.println("\n======================================================================");
    pBLEScan->clearResults();
}

void setup() {
    Serial.begin(115200);
    for (int i = 0; i < 50; i++) {
        if (Serial) break;
        delay(100);
    }

    delay(1500); // For hardware stability on some boards  
    Serial.println("\nESP32 CAME BACK TO LIVE");
    scanWiFi();
    scanBLE();
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    NimBLEDevice::deinit(true);
    Serial.printf("\n[*] Entering Deep Sleep for %d seconds...\n", SLEEP_TIME);
    esp_sleep_enable_timer_wakeup(SLEEP_TIME * uS_TO_S_FACTOR);
    esp_deep_sleep_start();
}

void loop() {}