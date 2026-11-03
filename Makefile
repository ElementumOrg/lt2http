NAME = lt2http
GIT = git
GIT_VERSION = $(shell $(GIT) describe --tags --always)
DOCKER = docker
DOCKER_IMAGE = elementumorg/lt2http
BUILD_DIR = build
BUILD_TYPE ?= Release
LOCAL_PREFIX = $(CURDIR)/local-env

# Same platforms as Elementum. <platform> builds the lt2http binary, <platform>-shared the lt2http library,
# both end up in build/<platform>/ (the layout of lt2http-binaries and of the add-on's resources/bin).
ANDROID_PLATFORMS = android-arm android-arm64 android-x64 android-x86
LINUX_PLATFORMS = linux-armv6 linux-armv7 linux-armv7_softfp linux-arm64 linux-x64 linux-x86
WINDOWS_PLATFORMS = windows-x64 windows-x86
DARWIN_PLATFORMS = darwin-x64 darwin-arm64
PLATFORMS = $(ANDROID_PLATFORMS) $(LINUX_PLATFORMS) $(WINDOWS_PLATFORMS) $(DARWIN_PLATFORMS)
SHARED_PLATFORMS = $(addsuffix -shared,$(PLATFORMS))

# Run platform builds in parallel (`make JOBS=1 ...` for one at a time), sharing the CPUs (and memory) between them
JOBS ?= 4
BUILD_JOBS ?= $(shell nproc)

.PHONY: all help $(PLATFORMS) $(SHARED_PLATFORMS) $(addprefix env-,$(PLATFORMS)) env envs pull pull-all push push-all \
	local-env local binaries clean distclean

all:
	$(MAKE) -j$(JOBS) --output-sync=target BUILD_JOBS=$$(( $$(nproc) / $(JOBS) + 1 )) $(PLATFORMS) $(SHARED_PLATFORMS)

help:
	@echo "Usage: make [target] [VAR=value ...]"
	@echo
	@echo "Build (in Docker images $(DOCKER_IMAGE):<platform>):"
	@echo "  all                  Build binaries and libraries for all platforms (JOBS=$(JOBS) in parallel)"
	@echo "  <platform>           Build the binary, e.g. 'make linux-x64'"
	@echo "  <platform>-shared    Build the shared library, e.g. 'make windows-x64-shared'"
	@echo "  local-env, local     Build dependencies into ./local-env, then lt2http with the host compiler"
	@echo
	@echo "Images:"
	@echo "  env PLATFORM=<p>     Build the dependencies image for a platform (envs: all of them)"
	@echo "  pull, push           Pull/push the image for PLATFORM (pull-all, push-all: all of them)"
	@echo
	@echo "Release:"
	@echo "  binaries             Push build/<platform>/ to lt2http-binaries (needs GH_TOKEN)"
	@echo
	@echo "Variables: BUILD_TYPE=$(BUILD_TYPE), CMAKE_FLAGS, JOBS (parallel platforms), BUILD_JOBS (compile jobs per platform)"
	@echo
	@echo "Platforms:"
	@printf '  %s\n' $(PLATFORMS)

$(PLATFORMS): PLATFORM = $@
$(PLATFORMS): CMAKE_BUILD = $(BUILD_DIR)/cmake/$@
$(SHARED_PLATFORMS): PLATFORM = $(@:-shared=)
$(SHARED_PLATFORMS): CMAKE_BUILD = $(BUILD_DIR)/cmake/$@
$(SHARED_PLATFORMS): CMAKE_FLAGS += -Dbuild_library=ON

# Output file name, by kind (binary/library) and target OS
output_name = $(NAME)$(if $(filter %-shared,$1),$(if $(filter windows-%,$1),.dll,.so),$(if $(filter windows-%,$1),.exe))

$(PLATFORMS) $(SHARED_PLATFORMS):
	mkdir -p "$(BUILD_DIR)/$(PLATFORM)"
	$(DOCKER) run --rm -u "$$(id -u):$$(id -g)" -v "$(CURDIR):/$(NAME)" -w "/$(NAME)" $(DOCKER_IMAGE):$(PLATFORM) bash -c '\
		cmake -B "$(CMAKE_BUILD)" $${CMAKE_TOOLCHAIN_FILE:+-DCMAKE_TOOLCHAIN_FILE=$$CMAKE_TOOLCHAIN_FILE} \
			-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -Dlt2http_version=$(GIT_VERSION) $(CMAKE_FLAGS) && \
		cmake --build "$(CMAKE_BUILD)" -j$(BUILD_JOBS)'
	cp "$(CMAKE_BUILD)/$(call output_name,$@)" "$(BUILD_DIR)/$(PLATFORM)/"
	@# The add-on compares the sha1 appended to the file to decide if it has to copy a new version
	sha1sum "$(BUILD_DIR)/$(PLATFORM)/$(call output_name,$@)" | cut -c1-40 >> "$(BUILD_DIR)/$(PLATFORM)/$(call output_name,$@)"

env:
	$(DOCKER) build -t $(DOCKER_IMAGE):$(PLATFORM) -f docker/$(PLATFORM).Dockerfile .

$(addprefix env-,$(PLATFORMS)):
	$(MAKE) env PLATFORM=$(@:env-%=%)

envs:
	$(MAKE) -j$(JOBS) --output-sync=target $(addprefix env-,$(PLATFORMS))

pull:
	$(DOCKER) pull $(DOCKER_IMAGE):$(PLATFORM)

pull-all:
	for i in $(PLATFORMS); do $(DOCKER) pull $(DOCKER_IMAGE):$$i; done

push:
	$(DOCKER) push $(DOCKER_IMAGE):$(PLATFORM)

push-all:
	for i in $(PLATFORMS); do $(DOCKER) push $(DOCKER_IMAGE):$$i; done

# Native build for development, with dependencies built for the host into ./local-env
local-env:
	PREFIX=$(LOCAL_PREFIX) scripts/install_dependencies.sh backtrace boost openssl oatpp oatpp-swagger libtorrent

local:
	cmake -B $(BUILD_DIR)/cmake/local -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_PREFIX_PATH=$(LOCAL_PREFIX) \
		-Dlt2http_version=$(GIT_VERSION) $(CMAKE_FLAGS)
	cmake --build $(BUILD_DIR)/cmake/local -j$(BUILD_JOBS)

binaries:
	rm -rf $(BUILD_DIR)/binaries
	$(GIT) clone --depth=1 https://$(GH_TOKEN)@github.com/ElementumOrg/lt2http-binaries $(BUILD_DIR)/binaries
	for i in $(PLATFORMS); do \
		mkdir -p $(BUILD_DIR)/binaries/$$i; \
		cp -Rf $(BUILD_DIR)/$$i/* $(BUILD_DIR)/binaries/$$i/; \
	done
	cd $(BUILD_DIR)/binaries && \
		$(GIT) add -A && \
		$(GIT) commit -m "Update to $(GIT_VERSION)" && \
		$(GIT) tag -f $(GIT_VERSION) && \
		$(GIT) push origin master --tags

clean:
	rm -rf $(addprefix $(BUILD_DIR)/,$(PLATFORMS)) $(BUILD_DIR)/cmake

distclean:
	rm -rf $(BUILD_DIR)
