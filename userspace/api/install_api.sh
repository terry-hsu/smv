export SYSLIB=/usr/lib
export SYSINC=/usr/include

# compile smv userspace API library
echo "================= Compiling user space library ================="
cmake .
make clean
make

# Copy library and header files to local machine
echo "================= Copying smv header files to: $SYSINC ================="
sudo cp smv_lib.h /usr/include
sudo cp memdom_lib.h /usr/include
sudo cp kernel_comm.h /usr/include

echo "================= Copying smv library to system folder: $SYSLIB ================="
sudo cp libsmv_lib.so /usr/lib
sudo cp libsmv_lib.so /usr/lib/x86_64-linux-gnu/
sudo cp libsmv_lib.so /lib/x86_64-linux-gnu/

echo "================= Installation copmleted ==============================="
