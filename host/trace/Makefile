QEMU_PATH = /home/chenzhuo/dev/projects/qemu
SNIPER_PATH = /home/chenzhuo/dev/projects/sniper_dev
SIFT_PATH = $(SNIPER_PATH)/sift

BUILD_DIR = ..
CXX = g++-7
SRCS := $(wildcard *.cc)
DEPFILES := $(SRCS:%.cc=%.d)

DEBUG ?= 0
ifeq ($(DEBUG), 1)
	CXXFLAGS += -DDEBUG -g -O0
	SONAME = libtrace.sod
	OBJECTS = $(patsubst %.cc,%.od,$(SRCS))
	LIB_SIFT = sift/libsift.ad
else
	CXXFLAGS += -O2
	SONAME = libtrace.so
	OBJECTS = $(patsubst %.cc,%.o,$(SRCS))
	LIB_SIFT = sift/libsift.a
endif

SIFT_DIR = sift
SIFT_FILE = $(patsubst $(SIFT_PATH)/%, $(SIFT_DIR)/%, $(wildcard $(SIFT_PATH)/*.h $(SIFT_PATH)/*.cc))

CXXFLAGS += -std=c++17 -DTARGET_X86_64 -D__STDC_LIMIT_MACROS -Werror -DPIE -m64 -mcx16 -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -Wall -Wundef -Wwrite-strings -fno-strict-aliasing -fno-common -fwrapv -Wexpansion-to-defined -Wendif-labels -Wno-shift-negative-value -Wno-missing-include-dirs -Wempty-body -Wformat-security -Wformat-y2k -Winit-self -Wignored-qualifiers -Wtype-limits -fstack-protector-strong -fPIC
CXXFLAGS += -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include
CXXFLAGS += -I../ -I$(SIFT_DIR) -I$(SNIPER_PATH)/include
LDFLAGS = -lglib-2.0
DEPFLAGS = -MT $@ -MMD -MP -MF $*.d

SNIPER_CXXFLAGS += -std=c++17 -fno-strict-aliasing -fPIC -DTARGET_INTEL64 -Wall -Wextra -Wcast-align -Wno-unused-parameter -Wno-unknown-pragmas -I$(SNIPER_PATH)/common/misc
export SNIPER_CXXFLAGS

QEMU_PLUGIN_H := $(QEMU_PATH)/include/qemu/qemu-plugin.h
LN_QEMU_PLUGIN_H := ../qemu-plugin.h

all: $(LN_QEMU_PLUGIN_H) $(BUILD_DIR)/$(SONAME)

$(LIB_SIFT): $(SIFT_DIR) $(SIFT_FILE)
	$(MAKE) -C ./sift

$(SIFT_DIR)/%:
	mkdir -p $(SIFT_DIR)
	ln -s $(SIFT_PATH)/$* $@

$(LN_QEMU_PLUGIN_H):
	ln -s $(QEMU_PLUGIN_H) $(LN_QEMU_PLUGIN_H)

$(BUILD_DIR)/$(SONAME): $(OBJECTS) $(LIB_SIFT)
	$(CXX) -shared -Wl,-soname,$(SONAME) -o $@ $^ $(LDFLAGS)

%.o %.od: %.cc %.d $(SIFT_FILE)
	$(CXX) -o $@ -c $< $(CXXFLAGS) $(DEPFLAGS)

$(DEPFILES):
include $(wildcard $(DEPFILES))

clean:
	rm -f *.o *.od ../libtrace.sod ../libtrace.so *.d qemu-plugin.h
	rm -f $(SIFT_DIR)/*.o $(SIFT_DIR)/*.h $(SIFT_DIR)/*.cc $(SIFT_DIR)/libsift.a $(SIFT_DIR)/*.od $(SIFT_DIR)/libsift.ad

.PHONY: all clean


