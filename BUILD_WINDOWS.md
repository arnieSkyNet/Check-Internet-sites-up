# 🛠️ Linux / Raspberry Pi Build & Install

## Clone Repository
git clone https://github.com/arnieSkyNet/Check-Internet-sites-up.git
cd Check-Internet-sites-up

## Open Visual Studio Solution
Open the solution file in Visual Studio:
chkinetup_win\chkinetup_win.sln


## Build the Solution
- Choose **Debug** or **Release** configuration
- Build the solution
- The executable will be located in:
  - chkinetup_win\x64\Debug\chkinetup_win.exe
  - or
  - chkinetup_win\x64\Release\chkinetup_win.exe

## Build Options

### Option 1: Build with static linking (Recommended)
- This creates an executable that **does not require extra DLLs** on another system
- Steps:
  1. In Visual Studio, open **Project Properties  C/C++  Code Generation  Runtime Library**
  2. Choose **Multi-threaded (/MT)** for static linking
  3. Build the solution (Debug or Release)
- The resulting EXE will be in:
  - `chkinetup_win\x64\Debug\chkinetup_win.exe`
  - or
  - `chkinetup_win\x64\Release\chkinetup_win.exe`

### Option 2: Build with standard Visual Studio settings
- This will produce an EXE that **requires Microsoft runtime DLLs**:
  - `MSVCP140.dll`, `VCRUNTIME140.dll`, `ucrtbase.dll`
  - Optionally: `MSVCP140_1.dll`, `VCRUNTIME140_1.dll`
- Users must install the **Microsoft Visual C++ Redistributable**:
  [https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist)



## Run the Program
- Double-click the EXE to open GUI
- Or run from a Command Prompt to attach a console:
```cmd
chkinetup_win.exe -d

## Optional: Run from Command Prompt
- Attach a console window: chkinetup_win.exe -d
- Run GUI directly: double-click chkinetup_win.exe

## Optional Command-Line Arguments
- `-d` : attach console window
- `-N` : console-only mode
- `-G` : force GUI window
- `-q` : quit immediately
- `delay` : seconds between host checks

## Log Files
- Default location: `%USERPROFILE%\log\`
- Use `-l <filename>` to set logfile name
- Use `-L <path>` to set logfile directory

## Dependencies / Libraries
- Visual Studio (2019 or later recommended)
- C++ standard libraries included with Visual Studio
- Optional: Microsoft Visual C++ Redistributable (if not building statically)

