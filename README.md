# NT Games

Trivial games for NTOS. With source code and binaries for all platforms. Alpha, AXP64, ARM, AARCH64, MIPS, PowerPC, Itanium, etc.

## RISC building instructions

### ARM

Only available in MSVC (2012-2015) not SDK, add `/D_ARM_WINAPI_PARTITION_DESKTOP_SDK_AVAILABLE`

### IA64

Win7 SDK only (not Visual Studio), use `setenv /ia64 /2008 /release`

### AXP64

Microsoft Platform SDK for Windows 2000 – RC2, aka psdk99, `setwin64`, use `cl.exe -D_AXP64_=1 -D_ALPHA64_=1 -DALPHA=1 -DWIN64 -D_WIN64 -DWIN32 -D_WIN32  -Wp64 -W4 -Ap64`

## License
All code is public domain.
