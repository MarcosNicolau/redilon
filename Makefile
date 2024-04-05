CC := gcc
CFLAGS := -Wall -Werror -O2

# Library name and version
LIBRARY_NAME := redilon
LIBRARY_VERSION := 1.0

# Source files
SRCS := $(wildcard src/*.c)
OBJS := $(SRCS:.c=.o)

# Installation directories
PREFIX := /usr/local
LIBDIR := $(PREFIX)/lib
INCLUDEDIR := $(PREFIX)/include/

# Targets
TARGET := lib$(LIBRARY_NAME).so.$(LIBRARY_VERSION)

.PHONY: all install uninstall clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -shared -o $@ $^

install: $(TARGET)
	install -D -m 644 $(TARGET) $(LIBDIR)/$(TARGET)
	install -D -m 644 $(TARGET) $(LIBDIR)/$(TARGET:.so.$(LIBRARY_VERSION)=.so)
	install -d $(INCLUDEDIR)
	install -m 644 ./src/redilon.h $(INCLUDEDIR)/$(LIBRARY_NAME).h

uninstall:
	rm -f $(LIBDIR)/$(TARGET)
	rm -f $(LIBDIR)/$(TARGET:.so.$(LIBRARY_VERSION)=.so)
	rm -rf $(INCLUDEDIR)

clean:
	rm -f $(TARGET) $(OBJS)
