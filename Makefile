ifneq ($(wildcard /var/jb/bin/sh),)
SHELL := /var/jb/bin/sh
endif
THEOS ?= ../theos
SDK ?= $(THEOS)/sdks/iPhoneOS16.5.sdk
CC := clang
PYTHON ?= python3
FLAGS := -isysroot $(SDK) -arch arm64 -miphoneos-version-min=16.0 -O2 -Wall -Wextra -Wno-unused-parameter
OBJC := $(FLAGS) -fobjc-arc
UI := Sources/App/BDBController.m Sources/App/BDBBridge.m
.PHONY: all package check check-power-state clean assets
all: package
assets:
	$(PYTHON) scripts/assets.py
build/Profile.o: Sources/Helper/Profile20D67.c Sources/Helper/Profile20D67.h Sources/Helper/PowerState.h
	mkdir -p build
	$(CC) $(FLAGS) -c $< -o $@
build/basebandctl: Sources/Helper/main.m build/Profile.o Sources/Helper/PowerState.h
	$(CC) $(OBJC) Sources/Helper/main.m build/Profile.o -framework Foundation -framework IOKit -framework CoreFoundation -o $@
build/BasebandDisabler: Sources/App/main.m $(UI) Sources/App/BDBController.h Sources/App/BDBBridge.h
	mkdir -p build
	$(CC) $(OBJC) Sources/App/main.m $(UI) -framework UIKit -framework Foundation -o $@
package: assets build/basebandctl build/BasebandDisabler
	$(PYTHON) scripts/package.py
check-power-state:
	$(CC) $(FLAGS) -std=c++17 -Werror -fsyntax-only tests/power_state.cpp
check: check-power-state package
	$(PYTHON) tests/test_package.py
clean:
	$(PYTHON) -c 'import shutil; shutil.rmtree("build", ignore_errors=True)'
