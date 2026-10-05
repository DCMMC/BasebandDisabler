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
DEB := dist/com.dcmmc.basebanddisabler_0.1.0_iphoneos-arm64.deb
.PHONY: all package check clean assets
all: package
assets:
	$(PYTHON) scripts/assets.py
build/Profile.o: Sources/Helper/Profile20D67.c Sources/Helper/Profile20D67.h
	mkdir -p build
	$(CC) $(FLAGS) -c $< -o $@
build/basebandctl: Sources/Helper/main.m build/Profile.o
	$(CC) $(OBJC) $^ -framework Foundation -framework IOKit -framework CoreFoundation -o $@
build/BasebandDisabler: Sources/App/main.m $(UI) Sources/App/BDBController.h Sources/App/BDBBridge.h
	mkdir -p build
	$(CC) $(OBJC) Sources/App/main.m $(UI) -framework UIKit -framework Foundation -o $@
build/BasebandDisablerPrefs: Sources/Preferences/Preferences.m $(UI) Sources/App/BDBController.h Sources/App/BDBBridge.h
	mkdir -p build
	$(CC) $(OBJC) -bundle Sources/Preferences/Preferences.m $(UI) -F$(SDK)/System/Library/PrivateFrameworks -framework Preferences -framework UIKit -framework Foundation -o $@
package: assets build/basebandctl build/BasebandDisabler build/BasebandDisablerPrefs
	$(PYTHON) scripts/package.py
check: package
	$(PYTHON) tests/test_package.py
clean:
	$(PYTHON) -c 'import shutil; shutil.rmtree("build", ignore_errors=True)'
