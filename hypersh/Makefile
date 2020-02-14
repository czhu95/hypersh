QEMU_PATH = /home/chenzhuo/dev/projects/qemu

CXX := g++
CXXFLAGS := -O2 -std=c++14 -D__STDC_LIMIT_MACROS -Werror -pthread -DPIE -m64 -mcx16 -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -Wall -Wundef -Wwrite-strings -fno-strict-aliasing -fno-common -fwrapv -Wexpansion-to-defined -Wendif-labels -Wno-shift-negative-value -Wno-missing-include-dirs -Wempty-body -Wformat-security -Wformat-y2k -Winit-self -Wignored-qualifiers -Wtype-limits -fstack-protector-strong -fPIC
DEPFLAGS = -MT $@ -MMD -MP -MF $*.d

QEMU_PLUGIN_H := $(QEMU_PATH)/include/qemu/qemu-plugin.h
LN_QEMU_PLUGIN_H := qemu-plugin.h

SRCS := $(wildcard *.cc)
OBJECTS := $(patsubst %.cc,%.o,$(SRCS))
DEPFILES := $(SRCS:%.cc=%.d)
SONAME := libhypersh.so

all: $(LN_QEMU_PLUGIN_H) $(SONAME)

$(LN_QEMU_PLUGIN_H):
	ln -s $(QEMU_PLUGIN_H) $(LN_QEMU_PLUGIN_H)

$(SONAME): $(OBJECTS)
	$(CXX) -shared -Wl,-soname,$@ -o $@ $^

%.o: %.cc %.d
	$(CXX) -o $@ -c $< $(CXXFLAGS) $(DEPFLAGS)

$(DEPFILES):
include $(wildcard $(DEPFILES))

clean:
	rm -f *.o *.so *.d qemu-plugin.h


