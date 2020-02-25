QEMU_PATH = /home/chenzhuo/dev/projects/qemu

BUILD_DIR = ..
CXX = g++
SRCS := $(wildcard *.cc)
DEPFILES := $(SRCS:%.cc=%.d)

DEBUG ?= 0
ifeq ($(DEBUG), 1)
	CXXFLAGS += -DDEBUG -g -O0
	SONAME = libtrace.sod
	OBJECTS = $(patsubst %.cc,%.od,$(SRCS))
else
	CXXFLAGS += -O2
	SONAME = libtrace.so
	OBJECTS = $(patsubst %.cc,%.o,$(SRCS))
endif

CXXFLAGS += -std=c++17 -D__STDC_LIMIT_MACROS -Werror -DPIE -m64 -mcx16 -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -Wall -Wundef -Wwrite-strings -fno-strict-aliasing -fno-common -fwrapv -Wexpansion-to-defined -Wendif-labels -Wno-shift-negative-value -Wno-missing-include-dirs -Wempty-body -Wformat-security -Wformat-y2k -Winit-self -Wignored-qualifiers -Wtype-limits -fstack-protector-strong -fPIC
CXXFLAGS += -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include
CXXFLAGS += -I../
CXXFLAGS += -I/home/chenzhuo/dev/projects/sniper/sift
CXXFLAGS += -I/home/chenzhuo/dev/projects/sniper/frontend
LDFLAGS = -lglib-2.0
DEPFLAGS = -MT $@ -MMD -MP -MF $*.d

LIB_SIFT = /home/chenzhuo/dev/projects/sniper/sift/libsift.a

QEMU_PLUGIN_H := $(QEMU_PATH)/include/qemu/qemu-plugin.h
LN_QEMU_PLUGIN_H := ../qemu-plugin.h

all: $(LN_QEMU_PLUGIN_H) $(BUILD_DIR)/$(SONAME)

$(LN_QEMU_PLUGIN_H):
	ln -s $(QEMU_PLUGIN_H) $(LN_QEMU_PLUGIN_H)

$(BUILD_DIR)/$(SONAME): $(OBJECTS) $(LIB_SIFT)
	$(CXX) -shared -Wl,-soname,$(SONAME) -o $@ $^ $(LDFLAGS)

%.o: %.cc %.d
	$(CXX) -o $@ -c $< $(CXXFLAGS) $(DEPFLAGS)

%.od: %.cc %.d
	$(CXX) -o $@ -c $< $(CXXFLAGS) $(DEPFLAGS)

$(DEPFILES):
include $(wildcard $(DEPFILES))

clean:
	rm -f *.o *.od ../libtrace.sod ../libtrace.so *.d qemu-plugin.h

.PHONY: all clean


