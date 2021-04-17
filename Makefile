CC = cc
CXX = c++
CXXSTD = 14
CMAKE_FLAGS = 
STRIP = strip

PROJECT = elementumorg
NAME = lt2http
GIT = git

DOCKER = docker
DOCKER_IMAGE = elementumorg/lt2http
CMAKE = cmake

LOCAL_PLATFORM=linux-x64
LOCAL_CROSS_TRIPLE=x86_64-linux-gnu
LOCAL_CROSS_ROOT=$(shell pwd)/local-env/

UPX = upx
GIT_VERSION = $(shell $(GIT) describe --tags)
PARALLEL = $(shell cat /proc/cpuinfo | grep processor | wc -l)
OUTPUT_NAME = $(NAME)$(EXT)
BUILD_PATH = build/$(TARGET_OS)-$(TARGET_ARCH)
PLATFORMS = \
	android-arm \
	android-arm64 \
	android-x64 \
	android-x86 \
	linux-armv6 \
	linux-armv7 \
	linux-arm64 \
	linux-x64 \
	linux-x86 \
	windows-x64 \
	windows-x86 \
	darwin-x64

include platform_host.mk

ifneq ($(CROSS_TRIPLE),)
	CC := $(CROSS_TRIPLE)-$(CC)
	CXX := $(CROSS_TRIPLE)-$(CXX)
	STRIP := $(CROSS_TRIPLE)-strip
endif

include platform_target.mk

include scripts/deps

CMAKE_FLAGS = -DOPENSSL_ROOT_DIR=$(CROSS_ROOT)/ \
	-DCROSS_TRIPLE=$(CROSS_TRIPLE) -DCROSS_ROOT=$(CROSS_ROOT)

ifeq ($(TARGET_OS), windows)
	EXT = .exe

	CMAKE_FLAGS += -DCMAKE_TOOLCHAIN_FILE=scripts/toolchains/mingw.cmake -DMINGW=ON
	CC := $(CROSS_TRIPLE)-gcc
	CXX := $(CROSS_TRIPLE)-g++
	CFLAGS += -DWIN32 -D_WIN32_WINNT=0x0600 -DWIN32_LEAN_AND_MEAN
	CXXFLAGS += -DWIN32 -D_WIN32_WINNT=0x0600 -DWIN32_LEAN_AND_MEAN
else ifeq ($(TARGET_OS), darwin)
	EXT =
	CMAKE_FLAGS += -DCMAKE_TOOLCHAIN_FILE=scripts/toolchains/darwin.cmake
	CC := $(CROSS_TRIPLE)-gcc
	CXX := $(CROSS_TRIPLE)-g++
else ifeq ($(TARGET_OS), linux)
	EXT =
else ifeq ($(TARGET_OS), android)
	EXT =
	CMAKE_FLAGS += -DIFADDRS=ON
	CC := $(CROSS_ROOT)/bin/$(CROSS_TRIPLE)-clang
	CXX := $(CROSS_ROOT)/bin/$(CROSS_TRIPLE)-clang++
endif


.PHONY: $(PLATFORMS) local-env

all:
	for i in $(PLATFORMS); do \
		$(MAKE) $$i; \
	done

$(PLATFORMS):
	$(MAKE) build TARGET_OS=$(firstword $(subst -, ,$@)) TARGET_ARCH=$(word 2, $(subst -, ,$@))

force:
	@true

$(BUILD_PATH):
	mkdir -p $(BUILD_PATH)

$(BUILD_PATH)/$(OUTPUT_NAME): $(BUILD_PATH) force
	rm -rf $(BUILD_PATH)
	CC="$(CC)" CXX="$(CXX)" CROSS_TRIPLE="$(CROSS_TRIPLE)" \
	$(CMAKE) -B $(BUILD_PATH) \
		-DUSE_MIMALLOC=OFF \
		-DUSE_STATIC=$(USE_STATIC) \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_VERBOSE_MAKEFILE=ON \
		-DCMAKE_CXX_STANDARD=$(CXXSTD) \
		-DCMAKE_C_FLAGS="$(CFLAGS)" \
		-DCMAKE_CXX_FLAGS="$(CXXFLAGS)" \
		-DCMAKE_PREFIX_PATH="$(CROSS_ROOT)/lib/pkgconfig" \
		-DCMAKE_MODULE_PATH="$(CROSS_ROOT)/lib/pkgconfig" \
		$(CMAKE_FLAGS)
	cmake --build $(BUILD_PATH) --parallel $(PARALLEL)
	chmod -R 777 $(BUILD_PATH)

vendor_darwin vendor_linux:

vendor_windows:
	#find $(shell go env GOPATH)/pkg/$(GOOS)_$(GOARCH) -name *.dll -exec cp -f {} $(BUILD_PATH) \;

vendor_android:
	cp $(CROSS_ROOT)/sysroot/usr/lib/$(CROSS_TRIPLE)/libc++_shared.so $(BUILD_PATH)/out/
	chmod +rx $(BUILD_PATH)/out/libc++_shared.so
	# cp $(CROSS_ROOT)/$(CROSS_TRIPLE)/lib/libgnustl_shared.so $(BUILD_PATH)
	# chmod +rx $(BUILD_PATH)/libgnustl_shared.so

vendor_libs_windows:

vendor_libs_android:
	$(CROSS_ROOT)/sysroot/usr/lib/$(CROSS_TRIPLE)/libc++_shared.so
	# $(CROSS_ROOT)/$(CROSS_TRIPLE)/lib/libgnustl_shared.so

lt2http: $(BUILD_PATH)/$(OUTPUT_NAME)

re: clean build

clean:
	rm -rf $(BUILD_PATH)

distclean:
	rm -rf build

build: force
ifeq ($(TARGET_OS), windows)
endif	
	$(DOCKER) run --rm -v $(shell pwd):/lt2http --ulimit memlock=67108864 -w /lt2http $(DOCKER_IMAGE):$(TARGET_OS)-$(TARGET_ARCH) make dist TARGET_OS=$(TARGET_OS) TARGET_ARCH=$(TARGET_ARCH) GIT_VERSION=$(GIT_VERSION)

docker: force
	$(DOCKER) run --rm -v $(shell pwd):/lt2http --ulimit memlock=67108864 -w /lt2http $(DOCKER_IMAGE):$(TARGET_OS)-$(TARGET_ARCH)

strip: force
	# Temporary disable strip
	# @find $(BUILD_PATH) -type f ! -name "*.exe" -exec $(STRIP) {} \;

upx-all:
	@find build/ -type f ! -name "*.exe" -a ! -name "*.so" -exec $(UPX) --lzma {} \;

upx: force
# Do not .exe files, as upx doesn't really work with 8l/6l linked files.
# It's fine for other platforms, because we link with an external linker, namely
# GCC or Clang. However, on Windows this feature is not yet supported.
	@find $(BUILD_PATH) -type f ! -name "*.exe" -a ! -name "*.so" -exec $(UPX) --lzma {} \;

checksum: $(BUILD_PATH)/$(OUTPUT_NAME)
	shasum -b $(BUILD_PATH)/out/$(OUTPUT_NAME) | cut -d' ' -f1 >> $(BUILD_PATH)/out/$(OUTPUT_NAME)

ifeq ($(TARGET_ARCH), arm)
dist: lt2http vendor_$(TARGET_OS) strip checksum
else ifeq ($(TARGET_ARCH), armv6)
dist: lt2http vendor_$(TARGET_OS) strip checksum
else ifeq ($(TARGET_ARCH), armv7)
dist: lt2http vendor_$(TARGET_OS) strip checksum
else ifeq ($(TARGET_ARCH), arm64)
dist: lt2http vendor_$(TARGET_OS) strip checksum
else ifeq ($(TARGET_OS), darwin)
dist: lt2http vendor_$(TARGET_OS) strip checksum
else
dist: lt2http vendor_$(TARGET_OS) strip checksum
endif

binaries:
	rm -rf build/binaries
	git config --global push.default simple
	git clone --depth=1 https://${GH_TOKEN}@github.com/ElementumOrg/lt2http-binaries build/binaries
	for i in $(PLATFORMS); do \
		mkdir -p build/binaries/$$i; \
		cp -Rf build/$$i/out/* build/binaries/$$i/; \
	done
	cd build/binaries && git add * && git commit -m "Update to ${GIT_VERSION}"

pull-all:
	for i in $(PLATFORMS); do \
		docker pull $(DOCKER_IMAGE):$$i; \
	done

pull:
	docker pull $(DOCKER_IMAGE):$(PLATFORM)

prepare-all:
	for i in $(PLATFORMS); do \
		$(MAKE) prepare PLATFORM=$(PLATFORM); \
	done

local-env:
	mkdir -p $(LOCAL_CROSS_ROOT)
	cd $(LOCAL_CROSS_ROOT)
	CROSS_ROOT=$(LOCAL_CROSS_ROOT) \
	CROSS_TRIPLE=$(LOCAL_CROSS_TRIPLE) \
	scripts/build-local.sh

env:
	$(DOCKER) build \
		--build-arg BOOST_VERSION=$(BOOST_VERSION) \
		--build-arg BOOST_SHA256=$(BOOST_SHA256) \
		--build-arg OPENSSL_VERSION=$(OPENSSL_VERSION) \
		--build-arg OPENSSL_SHA256=$(OPENSSL_SHA256) \
		--build-arg LIBTORRENT_VERSION=$(LIBTORRENT_VERSION) \
		--build-arg OATPP_VERSION=$(OATPP_VERSION) \
		--build-arg OATPP_SWAGGER_VERSION=$(OATPP_SWAGGER_VERSION) \
		--build-arg MIMALLOC_VERSION=$(MIMALLOC_VERSION) \
		--build-arg CMAKE_VERSION=$(CMAKE_VERSION) \
		--build-arg BT_VERSION=$(BT_VERSION) \
		-t $(DOCKER_IMAGE):$(PLATFORM) \
		-f docker/$(PLATFORM).Dockerfile scripts/

envs:
	for i in $(PLATFORMS); do \
		$(MAKE) env PLATFORM=$$i; \
	done
