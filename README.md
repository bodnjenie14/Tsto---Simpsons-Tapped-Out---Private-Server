# TSTO Server and APK Patching Guide

## UPDATE NOTES

Update 1 v0.02
- **RESTART BUG FIXED:** No longer need to reboot server if you close game.
- **Ios/Apple Support:** Fully supports ios/apple devices. Non jailbroke via sideloady and jailbroke devices.
- **Taps Event can be completed:** Dont need to minuplate time to finish taps.
- **Webpanel Dashboard:** ``http://localhost/dashboard`` - Can be used for basic servercontrols and to restore old events.

---

## Overview
This guide provides instructions to set up the TSTO server, patch the `tsto.apk`, and configure the system to use the server and DLC files.

---

## APK Patching Requirements
- **Python 3** (only for patching the APK; a C++ version is on the way).
- **Patched APK**: Created using the provided GUI patcher (`windows_gui_patcher.py` script).
- **30GB of free disk space** for the DLC.

---


## Private Server Requirements
- Windows Operating system.
  
---

## Steps to Patch APK and Configure the Server

### 1. Patch the APK
1. Download the `tsto.apk` file from the repository and place it into the `apk_patcher` folder.
   - Ensure the file is named `tsto.apk`.
2. Use the **Windows GUI Patcher** (`windows_gui_patcher.py`) to patch the `tsto.apk`:
   - Navigate to the `apk_patcher` folder.
   - Open the terminal in this folder:
     - Hold **Shift** and **Right-Click** anywhere inside the folder, then select **"Open PowerShell window here"** (or **"Open Command Prompt here"** depending on your system).
   - Run the following command:
     ```
     python windows_gui_patcher.py
     ```
   - The graphical user interface (GUI) will open.
   - Enter the following details:
     - **Server IP:** Enter the server IP in the format: `http://[ip_here]:80`.
     - **DLC IP:** Enter the DLC IP in the format: `http://[ip_here]:80`.
   - Click on the **"Patch APK"** button to create a patched APK.
3. The patched APK will be saved in the same folder as the original `tsto.apk`.
4. Transfer the patched APK to your mobile device or BlueStacks.
5. Install the patched APK.



---

### 2. IP Address Example
- **Server IP:** `http://192.168.1.1:80`
- **DLC IP:** `http://192.168.1.2:80`

---

### 3. Download and Add DLC
1. Download the DLC.
2. Place the downloaded DLC file into the `DLC` folder.
   - (More detailed instructions can be found in the `instructions` folder.)

---

### 4. Setup the Town File
1. Add your town file to the `towns/` folder.
2. Rename the file to `mytown.pb`:
   - **If the save is from `tsto.de`:** The file name will be your email address. Rename it to `mytown.pb`.
   - **If the save is from `tsto.me`:** The file name will be `protoland.pb`. Rename it to `mytown.pb`.

---

### 5. Adjust Windows Folder Options (if needed)
- If necessary, enable the option to view and edit known file extensions:
  - Open File Explorer.
  - Go to **View > Options > Change folder and search options**.
  - Under the **View** tab, uncheck **Hide extensions for known file types**.

---

### 6. Install DLC on the Server
- Do **not** open any application until the DLC is properly installed on the server.

---

### 7. Launch the Server
1. Open the server by running `tsto_server.exe`.
2. A console window will appear—keep this window open.

---

### 8. Run the APK
- Once the server is running, open the patched APK on your mobile device or BlueStacks.

---

## Additional Notes
- **GUI Patcher Location:** The `windows_gui_patcher.py` script is located in the `apk_patcher` folder.
- **IP and Port Configuration:**
  - Modify the `server-config.json` file to adjust the IP and port settings, if needed.
- **Source code be uploaded soon.**
---

Follow these steps sequentially for proper setup. Enjoy!

---

## Building the project currently needs `vcpkg` 
- abseil libary
  ( vcpkg install abseil )
---



Follow these steps sequentially for proper setup. Enjoy!

---