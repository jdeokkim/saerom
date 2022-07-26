#
# Copyright (c) 2022 jdeokkim
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#

.PHONY: all clean

_COLOR_BEGIN := $(shell tput setaf 13)
_COLOR_END := $(shell tput sgr0)

PROJECT_NAME := saerom
PROJECT_FULL_NAME := jdeokkim/saerom

PROJECT_PREFIX := $(_COLOR_BEGIN)$(PROJECT_FULL_NAME):$(_COLOR_END)

BINARY_PATH := bin
INCLUDE_PATH := include
RESOURCE_PATH := res
SOURCE_PATH := src

INCLUDE_PATH += $(SOURCE_PATH)/external

SOURCES := \
	$(SOURCE_PATH)/bot.c    \
	$(SOURCE_PATH)/config.c \
	$(SOURCE_PATH)/info.c   \
	$(SOURCE_PATH)/json.c   \
	$(SOURCE_PATH)/krdict.c \
	$(SOURCE_PATH)/owner.c  \
	$(SOURCE_PATH)/papago.c \
	$(SOURCE_PATH)/utils.c  \
	$(SOURCE_PATH)/yxml.c   \
	$(SOURCE_PATH)/main.c

OBJECTS := $(SOURCES:.c=.o)

TARGETS := $(BINARY_PATH)/$(PROJECT_NAME)

CC := gcc
CFLAGS := -D_DEFAULT_SOURCE -g $(INCLUDE_PATH:%=-I%) -O2 -std=gnu99
LDLIBS := -ldl -lcurl -ldiscord -lpthread -lsigar

all: pre-build build post-build

pre-build:
	@echo "$(PROJECT_PREFIX) Using: '$(CC)' to build this project."
    
build: $(TARGETS)

$(SOURCE_PATH)/%.o: $(SOURCE_PATH)/%.c
	@echo "$(PROJECT_PREFIX) Compiling: $@ (from $<)"
	@$(CC) -c $< -o $@ $(CFLAGS)
    
$(TARGETS): $(OBJECTS)
	@mkdir -p $(BINARY_PATH)
	@echo "$(PROJECT_PREFIX) Linking: $(TARGETS)"
	@$(CC) $(OBJECTS) -o $(TARGETS) $(CFLAGS) $(LDFLAGS) $(LDLIBS) $(WEBFLAGS)
    
post-build:
	@echo "$(PROJECT_PREFIX) Build complete."

clean:
	@echo "$(PROJECT_PREFIX) Cleaning up."
	@rm -rf $(BINARY_PATH)/*
	@rm -rf $(SOURCE_PATH)/*.o