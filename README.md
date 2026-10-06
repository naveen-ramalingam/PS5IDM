# PS5 Internet Download Manager (PS5IDM)

A high-performance, native download manager and archive extractor designed specifically for the PlayStation 5 homebrew environment. It features a built-in lightweight HTTP/WebSocket server providing a responsive, modern Web UI that can be accessed remotely from any browser (Mac, Windows, iOS, Android, etc.).

## 🚀 Features

- **High-Speed Downloads**: Multi-threaded, chunk-based download engine designed for maximum throughput.
- **Remote Web Interface**: Modern, glassmorphic UI to manage your downloads from your phone or PC.
- **Smart Link Grabber**: Paste a file host or index page URL, and the server will scan it to extract direct download links (supports `.pkg`, `.zip`, `.rar`, `.7z`, etc.). Includes a browser fallback for captcha-protected sites.
- **Automated Archive Extraction**: Automatically detects and extracts multipart archives (`.part1.rar`, `.001`, etc.) once downloads complete.
- **Password-Protected Archives**: Built-in support to decrypt and extract password-protected `.zip`, `.rar`, and `.7z` files directly from the Web UI.
- **SQLite Database**: Persistent queue and settings management.

## 🛠 Prerequisites

- **CMake** (v3.15+)
- **GCC / Clang** (Targeting PS5 / POSIX environments)
- **libcurl** (for the download engine and link grabber)
- **libarchive** (for advanced extraction features)
- **sqlite3** (for local database storage)
- **mongoose** (for the embedded web server)

## 🏗 Building for PS5

Generate the `.elf` executable targeting the PS5 hardware:

```bash
mkdir build && cd build
cmake -DPS5_TARGET=ON ..
make -j$(sysctl -n hw.ncpu)
```

The resulting executable will be available at `build/ps5-download-manager.elf`.

## 🎮 Usage

Run the executable on your target environment:

```bash
./ps5-download-manager.elf --debug
```

Once running, access the Web UI from any device on your local network:
`http://<PS5-IP-ADDRESS>:8080`

### Link Grabber
1. Navigate to the **Downloads** tab.
2. Click **🔗 Link Grabber**.
3. Paste a directory or file host URL.
4. The server will scan and return all downloadable links. Select the ones you want and hit **Download Selected**.

### Password-Protected Archives
1. Go to the **Archives** tab in the Web UI.
2. If an archive requires a password, click **Extract**.
3. A modal will prompt you for the password. Enter it, and the backend will handle decryption and extraction securely.

## 📝 License
This project is open-source. Please ensure compliance with PS5 SDK agreements if compiling with official tools.
