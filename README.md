# Check Internet Up (chkinetup)

**Check Internet Up** (`chkinetup`) is a lightweight Internet connectivity monitor for Linux systems, including Raspberry Pi. It regularly checks if the device is online and logs outages, helping diagnose Wi-Fi drops, router failures, or ISP issues. Ideal for remote-access systems and home automation.

---

## ✨ Features
- Runs on Raspberry Pi and standard Linux systems
- Detects Internet connectivity loss quickly
- Logs failures with timestamps for diagnostics
- Lightweight — suitable for background service use
- No dependencies beyond common Linux tools

---

## 📦 Installation

Clone the repository:

```sh
git clone https://github.com/<yourname>/<yourrepo>.git
cd <yourrepo>

Make the script executable:
chmod +x chkinetup

Usage

Run manually:
./chkinetup 5 -d
or
./chkinetup -h
for help

Example systemd service setup:
sudo cp chkinetup.service /etc/systemd/system/
sudo systemctl enable --now chkinetup.service

but I do via cron
crontab -e
add in these lines and save (nano editor CTRL O, RETURN, CTRL X)

# checks internet up 
@reboot /home/pi/sbin/chkinetup 5 -l chkinetup.log

that will save a log you can do:
tail -f ~/log/chkinetup.log

Configuration

By default the script checks a reliable endpoints :-;
www.google.com
www.cloudflare.com
www.microsoft.com
www.amazon.com
or you can 8.8.8.8 or a well-known website).
You can change the target or interval at the top of the script to suit your network.

Requirements
	•	Linux OS
	•	Standard networking tools (ping, bash or python depending on your version)

Contributing

Issues and pull requests are welcome!
Share ideas or improvements that help monitor uptime more effectively.
