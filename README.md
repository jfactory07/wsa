# WSA (Window System Agent)
The Window System Agent (WSA) library is introduced to encapsulte details about the native window system. It provides some general interfaces for driver to interact with different window systems.
Driver could dynamically load WSA library according to the window system that application is running on.


## How To Build and Install
The instructions are verified on Ubuntu 16.04.3 (64-bit version).

Currently, only Wayland is supported. The build generates one library libamdgpu_wsa_wayland.so.

```
sudo apt-get install libwayland-dev libwayland-dev:i386
cd <root of wsa>
```
### 64-bit Build
```
cmake -H. -Bbuilds/Release64
cd builds/Release64
make
```
### 32-bit Build
```
cmake -H. -Bbuilds/Release -DCMAKE_C_FLAGS=-m32
cd builds/Release
make
```
### Install WSA
```
sudo cp <root of wsa>/builds/Release64/wayland/libamdgpu_wsa_wayland.so /usr/lib/x86_64-linux-gnu/
sudo cp <root of wsa>/builds/Release/wayland/libamdgpu_wsa_wayland.so /usr/lib/i386-linux-gnu/
```

