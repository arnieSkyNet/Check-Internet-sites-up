# 📡 Check Internet Up (chkinetup) ![Version](https://img.shields.io/badge/version-v0.09-blue) ![License](https://img.shields.io/badge/license-MIT-green)

**Check Internet Up** (`chkinetup`) is a lightweight Linux internet connectivity monitor, ideal for Raspberry Pi, home automation, and remote-access systems. It regularly checks connectivity and logs outages, helping diagnose Wi-Fi drops, router failures, or ISP issues.  

---
## ✨ Features
- Runs on Raspberry Pi, Linux, and Windows
- Detects Internet connectivity loss quickly
- Logs failures and restorations with timestamps
- Lightweight — suitable for background service use
- No dependencies beyond standard tools
- Signal-safe termination (handles SIGINT and SIGTERM)
- Monitoring of host file if used and changed
---

## 🔍 How Connectivity Is Checked (No Ping Required)

`chkinetup` does **not** use ICMP ping.  
Instead, it performs a **real TCP connection attempt** to each configured host on **port 443** (HTTPS). This method checks more meaningful, real-world connectivity.

### ✅ Why TCP Port 443?
- HTTPS (443) is almost always allowed through firewalls
- Confirms **DNS resolution + route availability + service responsiveness**
- Detects real connectivity outages that a ping may **miss**

### ⚙️ How It Works (Simplified)
For each host, every `N` seconds:
1. Resolve its IP using `getaddrinfo()`
2. Attempt a **non-blocking** TCP socket connection
3. Wait briefly using `select()` (timeout)
4. Treat it as:
   - ✅ *Up* if the TCP handshake succeeds
   - ❌ *Unreachable* if it fails or times out

### 🧠 Why This Matters
Unlike ping:
- Doesn’t rely on ICMP (often blocked)
- Tests real internet usability
- More accurate for detecting failures that impact web browsing, streaming, VPN, etc.

> In short: If `chkinetup` says your internet is down…  
> your apps will **definitely** feel it too. ✅

## 🛠️ MS Windows Release v0.10.03

**Program:** Check-Internet-sites-up  
**Version:** v0.10.03  
**Platform:** Microsoft Windows  
**Build type:** Ready-built binary  

You can download the release here: [Download v0.10.03](https://github.com/arnieSkyNet/Check-Internet-sites-up/releases/tag/v0.10.03)

### Changelog
- Initial Windows GUI and console combined build  
- Display host connectivity status in real-time  
- GUI log window with keyboard shortcuts  
- Built-in host list for quick checks  
- Configurable delay between checks  
- Creation of host file
- Monitoring of host file changes

## 🛠️ Build & Install
Platform-specific build instructions are in separate files:

- [Linux / Raspberry Pi Build](BUILD_LINUX.md)  
- [Windows Build](BUILD_WINDOWS.md)

---

## ▶️ Linux Usage
Run manually:
```bash
chkinetup 5 -d
```
Show help:
```bash
chkinetup -h

chkinetup v0.09.01 - Internet connectivity checker

Usage: chkinetup [delay] [options]

Positional args:
  delay                   Interval in seconds between checks (default: 5)

Options:
  -h, --help              Show this help message and exit
  -d, --debug             Enable debug output to screen
  -l, --logfile <name>    Set logfile name (default: <hostname>.log)
  -L, --logdir <path>     Set logfile directory (default: $HOME/log)
  -c, --checkfile <file>  File containing list of hosts to check.
                          If the file doesn't exist, it will be created
                          and populated with the built-in hosts.
                          Lines starting with '#' are ignored.
  -C, --clearfile          Ignore existing host file if exists
  -H, --builtin-hosts      Use built-in host list
  -v, --version           Show program version

Written by ChatGPT vGPT-5-mini via guidance and design, with massive corrections by ArnieSkyNet
```
### Log Files
- Default log location:
```bash
~/log/chkinetup.log
```
- Watch logs live:
```bash
tail -f ~/log/chkinetup.log
```
- Optional: specify custom log file:
```bash
chkinetup -l /path/to/custom.log
```

---

## ⏱️ Scheduling
Run automatically via cron (example):
```bash
crontab -e
```
Add:
```cron
# Check internet at reboot
@reboot /home/pi/sbin/chkinetup 5 -l ~/log/chkinetup.log
```

---

## ⚙️ Configuration
Default hosts monitored:
- www.google.com  
- www.cloudflare.com  
- www.microsoft.com  
- www.amazon.com  
You can change the target hosts or interval at the top of the script to suit your network.  

---

## 📋 Log Output Example
```text
[08:11:2025 22:02:30 chkinetup 5 v0.09] <HOST> - stopped
[08:11:2025 22:02:39 chkinetup 5 v0.09] <HOST> - started
[08:11:2025 22:02:40 chkinetup 5 v0.09] www.google.com - unreachable
[08:11:2025 22:02:40 chkinetup 5 v0.09] www.cloudflare.com - unreachable
[08:11:2025 22:02:40 chkinetup 5 v0.09] www.microsoft.com - unreachable
[08:11:2025 22:02:40 chkinetup 5 v0.09] www.amazon.com - unreachable
[08:11:2025 22:02:40 chkinetup 5 v0.09] <HOST> - All hosts unreachable
[08:11:2025 22:03:11 chkinetup 5 v0.09] www.google.com - connectivity restored
[08:11:2025 22:03:21 chkinetup 5 v0.09] www.google.com - Global connectivity restored
```

---

## 🧾 Requirements
- Linux OS 
- GCC / make (for building from source)

---

## 📄 License
Licensed under the MIT License — see the `LICENSE` file for details. ![License](https://img.shields.io/badge/license-MIT-green)

---

## 🤝 Contributing
Issues and pull requests are welcome! Share ideas or improvements that help monitor uptime more effectively.
