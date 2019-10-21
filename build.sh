mkdir build
cd build
cmake .. -DCOMGR_INC_PATH=/opt/rocm/include/ -DCOMGR_LIB_PATH=/opt/rocm/lib/
make
