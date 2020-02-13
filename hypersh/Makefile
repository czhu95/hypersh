QEMU_PATH = /home/chenzhuo/dev/projects/qemu

CXX = g++
CXXFLAGS = -O2 -D__STDC_LIMIT_MACROS -Werror -pthread -DPIE -m64 -mcx16 -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -Wall -Wundef -Wwrite-strings -fno-strict-aliasing -fno-common -fwrapv -std=gnu++98 -Wexpansion-to-defined -Wendif-labels -Wno-shift-negative-value -Wno-missing-include-dirs -Wempty-body -Wformat-security -Wformat-y2k -Winit-self -Wignored-qualifiers -Wtype-limits -fstack-protector-strong -fPIC
CXXFLAGS += -I$(QEMU_PATH)/include/qemu -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I/usr/include/p11-kit-1 -I/usr/include/libpng16 -I/usr/include/spice-server -I/usr/include/spice-1 -I/usr/include/capstone

all:
	$(CXX) -o hypersh.o -c hypersh.cc $(CXXFLAGS)
	$(CXX) -shared -Wl,-soname,libhypersh.so -o libhypersh.so hypersh.o

clean:
	rm -f *.o *.so


