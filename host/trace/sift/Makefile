SOURCES=$(filter-out siftdump.cc,$(wildcard *.cc))

DEBUG ?= 0
ifeq ($(DEBUG), 1)
	SNIPER_CXXFLAGS += -DDEBUG -g -O0
	OBJECTS = $(patsubst %.cc,%.od,$(SOURCES))
	TARGET=libsift.ad
else
	SNIPER_CXXFLAGS += -O2
	OBJECTS = $(patsubst %.cc,%.o,$(SOURCES))
	TARGET=libsift.a
endif

CXX = g++
AR = ar

SNIPER_TARGET_ARCH ?= intel64


all : $(TARGET)

SNIPER_CXXFLAGS += -fPIC $(SNIPER_CXXFLAGS_ARCH)

%.o %.od : %.cc $(wildcard *.h) Makefile
	$(CXX) -c -o $@ $< $(SNIPER_CXXFLAGS)

$(TARGET) : $(OBJECTS)
	$(AR) rcs $@ $^
