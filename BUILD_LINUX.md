# =à Linux / Raspberry Pi Build & Install

## Clone Repository
git clone https://github.com/arnieSkyNet/Check-Internet-sites-up.git
cd Check-Internet-sites-up

## Dependencies / Libraries
Before building, ensure the required packages are installed:

sudo apt update
sudo apt install -y build-essential

This provides:
- GCC compiler
- make
- Standard C libraries (libc, libpthread, etc.)

## Build Binary
make
mv chkinetup ~/sbin/

Ensure ~/sbin is in your PATH to run it from anywhere:
export PATH="$HOME/sbin:$PATH"

## Optional: Run with Debug
chkinetup 5 -d

## Log Files
- Default location: ~/log/chkinetup.log
- Tail live: tail -f ~/log/chkinetup.log
- Custom log file: chkinetup -l /path/to/custom.log

