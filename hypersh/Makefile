QEMU_PATH = /home/chenzhuo/dev/projects/qemu

CXX := g++
CXXFLAGS := -O2 -std=c++14 -D__STDC_LIMIT_MACROS -Werror -pthread -DPIE -m64 -mcx16 -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -Wall -Wundef -Wwrite-strings -fno-strict-aliasing -fno-common -fwrapv -Wexpansion-to-defined -Wendif-labels -Wno-shift-negative-value -Wno-missing-include-dirs -Wempty-body -Wformat-security -Wformat-y2k -Winit-self -Wignored-qualifiers -Wtype-limits -fstack-protector-strong -fPIC
# CXXFLAGS += -isystem $(QEMU_PATH)/include/qemu
QEMU_PLUGIN_H := $(QEMU_PATH)/include/qemu/qemu-plugin.h
LN_QEMU_PLUGIN_H := qemu-plugin.h


OBJECTS := $(patsubst %.cc,%.o,$(wildcard *.cc))
SONAME := libhypersh.so

all: $(LN_QEMU_PLUGIN_H) $(SONAME)

$(LN_QEMU_PLUGIN_H):
	ln -s $(QEMU_PLUGIN_H) $(LN_QEMU_PLUGIN_H)

$(SONAME): $(OBJECTS)
	$(CXX) -shared -Wl,-soname,$@ -o $@ $^

%.o: %.cc %.h common.h
	$(CXX) -o $@ -c $< $(CXXFLAGS)

clean:
	rm -f *.o *.so qemu-plugin.h


