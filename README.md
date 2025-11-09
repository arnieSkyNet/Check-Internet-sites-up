# 📡 Check Internet Up (chkinetup) ![Version](https://img.shields.io/badge/version-v0.09-blue) ![License](https://img.shields.io/badge/license-MIT-green)

**Check Internet Up** (`chkinetup`) is a lightweight Linux internet connectivity monitor, ideal for Raspberry Pi, home automation, and remote-access systems. It regularly checks connectivity and logs outages, helping diagnose Wi-Fi drops, router failures, or ISP issues.  

---

## ✨ Features
- Runs on Raspberry Pi and standard Linux systems  
- Detects Internet connectivity loss quickly  
- Logs failures and restorations with timestamps  
- Lightweight — suitable for background service use  
- No dependencies beyond standard Linux tools  

---

## 🛠️ Build & Install
Clone the repository:
```bash
git clone https://github.com/arnieSkyNet/Check-Internet-sites-up.git
cd Check-Internet-sites-up
```
Build the binary:
```bash
make
mv chkinetup ~/sbin/
```
Ensure `~/sbin` is in your PATH if you want to run it from anywhere:
```bash
export PATH="$HOME/sbin:$PATH"
```

---

## ▶️ Usage
Run manually:
```bash
chkinetup 5 -d
```
Show help:
```bash
chkinetup -h
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
[08:11:2025 22:02:30 chkinetup 5 v0.09] arnie - stopped
[08:11:2025 22:02:39 chkinetup 5 v0.09] arnie - started
[08:11:2025 22:02:40 chkinetup 5 v0.09] www.google.com - unreachable
[08:11:2025 22:02:40 chkinetup 5 v0.09] www.cloudflare.com - unreachable
[08:11:2025 22:02:40 chkinetup 5 v0.09] www.microsoft.com - unreachable
[08:11:2025 22:02:40 chkinetup 5 v0.09] www.amazon.com - unreachable
[08:11:2025 22:02:40 chkinetup 5 v0.09] arnie - All hosts unreachable
[08:11:2025 22:03:11 chkinetup 5 v0.09] www.google.com - connectivity restored
[08:11:2025 22:03:21 chkinetup 5 v0.09] www.google.com - Global connectivity restored
```

---

## 🧾 Requirements
- Linux OS  
- Standard networking tools (ping, bash)  
- GCC / make (for building from source)

---

## 📄 License
Licensed under the MIT License — see the `LICENSE` file for details. ![License](https://img.shields.io/badge/license-MIT-green)

---

## 🤝 Contributing
Issues and pull requests are welcome! Share ideas or improvements that help monitor uptime more effectively.
