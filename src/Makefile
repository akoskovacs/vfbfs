# Virtual userspace filesystem for framebuffers
#
# Copyright (C) 2017 Akos Kovacs
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

include devices/Makefile.devs

CC		    := gcc
RM		    := rm
PKG_FUSE    := fuse
PKG_CONFIG  := pkg-config
TARGET		:= vfbfs
# TODO: fix this (autodetect fuse)
SO_FUSE 	:= /lib/x86_64-linux-gnu/libfuse.so.2.9.4

OBJS    := vfbfs.o file.o dir.o $(obj-y) 
CFLAGS  := $(shell $(PKG_CONFIG) --cflags $(PKG_FUSE)) -I ../include -ggdb -Wall
LDFLAGS := $(shell $(PKG_CONFIG) --libs $(PKG_FUSE))
#LDFLAGS := $(SO_FUSE)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o ../$(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $^

.PHONY: clean
clean:
	$(RM) $(OBJS)
