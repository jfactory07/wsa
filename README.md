# WSA (Window System Agent)
The Window System Agent (WSA) library is introduced to encapsulte details about the native window system. It provides some general interfaces for driver to interact with different window systems.
Driver could dynamically load WSA library according to the window system that application is running on.


## How To Build and Install
The instructions are verified on Ubuntu 16.04.3 (64-bit version).

Currently, only Wayland is supported. The build generates one library libamdgpu_wsa_wayland.so. 

```
sudo apt-get install libwayland-dev libwayland-dev:i386
cd ./wsa
mkdir ./build
cd ./build
```
### 64-bit Build
```
cmake ..
make
make install
```
### 32-bit Build
```
cmake .. -DCMAKE_C_FLAGS=-m32
make
make install
```


