QEMU_PATH = /home/chenzhuo/dev/projects/qemu

CXX = g++
SRCS := $(wildcard *.cc)
DEPFILES := $(SRCS:%.cc=%.d)

DEBUG ?= 0
ifeq ($(DEBUG), 1)
	CXXFLAGS += -DDEBUG -g -O0
	SONAME = libhypersh.sod
	OBJECTS = $(patsubst %.cc,%.od,$(SRCS))
else
	CXXFLAGS += -O2
	SONAME = libhypersh.so
	OBJECTS = $(patsubst %.cc,%.o,$(SRCS))
endif

CXXFLAGS += -std=c++17 -D__STDC_LIMIT_MACROS -Werror -DPIE -m64 -mcx16 -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -Wall -Wundef -Wwrite-strings -fno-strict-aliasing -fno-common -fwrapv -Wexpansion-to-defined -Wendif-labels -Wno-shift-negative-value -Wno-missing-include-dirs -Wempty-body -Wformat-security -Wformat-y2k -Winit-self -Wignored-qualifiers -Wtype-limits -fstack-protector-strong -fPIC
CXXFLAGS += -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include
LDFLAGS = -lglib-2.0
DEPFLAGS = -MT $@ -MMD -MP -MF $*.d

QEMU_PLUGIN_H := $(QEMU_PATH)/include/qemu/qemu-plugin.h
LN_QEMU_PLUGIN_H := qemu-plugin.h

all: $(LN_QEMU_PLUGIN_H) $(SONAME)

$(LN_QEMU_PLUGIN_H):
	ln -s $(QEMU_PLUGIN_H) $(LN_QEMU_PLUGIN_H)

$(SONAME): $(OBJECTS)
	$(CXX) -shared -Wl,-soname,$@ -o $@ $^ $(LDFLAGS)

%.o: %.cc %.d
	$(CXX) -o $@ -c $< $(CXXFLAGS) $(DEPFLAGS)

%.od: %.cc %.d
	$(CXX) -o $@ -c $< $(CXXFLAGS) $(DEPFLAGS)

$(DEPFILES):
include $(wildcard $(DEPFILES))

clean:
	rm -f *.o *.od *.sod *.so *.d qemu-plugin.h

.PHONY: all clean


