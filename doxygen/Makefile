# Copyright (c) 2016 Comcast Cable Communications Management, LLC.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

#
# Build the documentation
#

# Default target
.PHONY: all
all: doxygen

# These should be passed in by the root Makefile
WS_ROOT ?= $(CURDIR)/..
BR ?= $(WS_ROOT)/build-root

# We support MacOS for docs generation
ifeq ($(shell uname),Darwin)
OS_ID = darwin
endif

# Work out the OS if we haven't already
OS_ID ?= $(shell grep '^ID=' /etc/os-release | cut -f2- -d= | sed -e 's/\"//g')

# Package dependencies
DOC_DEB_DEPENDS = doxygen graphviz python-pyparsing python-jinja2
DOC_RPM_DEPENDS = doxygen graphviz pyparsing python-jinja2
DOC_MAC_BIN_DEPENDS = doxygen dot git
DOC_MAC_PY_DEPENDS = pyparsing jinja2

# Doxygen configuration and our utility scripts
DOXY_DIR ?= $(WS_ROOT)/doxygen

# Primary source directories
DOXY_SRC_DIRECTORIES = \
	vppinfra \
	svm \
	vlib \
	vlib-api \
	vnet \
	vpp \
	vpp-api

# Input directories and files
DOXY_INPUT ?= \
	$(wildcard $(WS_ROOT)/*.md) \
	$(wildcard $(DOXY_DIR)/*.md) \
	$(DOXY_SRC_DIRECTORIES) \
	plugins

# Strip leading workspace path from input names
DOXY_INPUT := $(subst $(WS_ROOT)/,,$(DOXY_INPUT))

# Files to exclude, from pre-Doxygen steps, eg because they're
# selectively compiled.
# Examples would be to exclude non-DPDK related sources when
# there's a DPDK equivalent that conflicts.
# These must be left-anchored paths for the regexp below to work.
DOXY_EXCLUDE ?= \
	vlib/vlib/buffer.c \
	vlib/example \
	plugins/sample-plugin

# Generate a regexp for filenames to exclude
DOXY_EXCLUDE_REGEXP = ($(subst .,\.,$(shell echo '$(strip $(DOXY_EXCLUDE))' | sed -e 's/ /|/g')))

# Include all the normal source directories in the include file path
DOXY_INCLUDE_PATH = $(DOXY_SRC_DIRECTORIES)

# Also include any plugin directories that exist
DOXY_INCLUDE_PATH += \
	$(shell find $(WS_ROOT)/plugins -maxdepth 1 -type d | sed -e 's@^$(WS_ROOT)/*@@')

# Find API header directories and include them in the header path.
# This is only useful if VPP and plugins are already built; nothing
# here depends on those targets. We don't build documentation for these
# header files, they're just added to the INCLUDE search path for Doxygen.
_vpp_br = $(shell find "$(BR)" -maxdepth 1 -type d \
	'(' -name build-vpp_debug-native -o -name build-vpp-native ')' -print \
	| sed -e 's@^$(WS_ROOT)/*@@' -e 1q)
ifneq ($(strip $(_vpp_br)),)
DOXY_INCLUDE_PATH += \
	$(_vpp_br)/vlib-api \
	$(_vpp_br)/vpp
# Also include any plugin directories that exist
DOXY_INCLUDE_PATH += \
	$(shell find $(WS_ROOT)/$(_vpp_br)/plugins -maxdepth 1 -type d | sed -e 's@^$(WS_ROOT)/*@@')
endif

# Discover if we have CPP available
_cpp = $(shell which cpp)
ifneq ($(strip $(_cpp)),)
# Add whatever directories CPP normally includes to the header path
DOXY_INCLUDE_PATH += $(shell set -e; $(_cpp) -v </dev/null 2>&1 | awk 'f&&/^ /{print $$1} /^\#include/{f=1}')
endif

# Target directory for doxygen output
DOXY_OUTPUT ?= $(BR)/docs

# Siphoned fragments end up in here
SIPHON_INPUT ?= $(DOXY_OUTPUT)/siphon_fragments

# Siphoned fragements are processed into here
SIPHON_OUTPUT ?= $(DOXY_OUTPUT)/siphon_docs

# Extra document inputs that are processed in addition to DOXY_INPUT
EXTRA_DOXY_INPUT += $(SIPHON_OUTPUT)

# All the siphon types we know about
SIPHONS ?= clicmd syscfg

SIPHON_FILES = $(addprefix $(SIPHON_INPUT)/,$(addsuffix .siphon,$(SIPHONS)))
SIPHON_DOCS = $(addprefix $(SIPHON_OUTPUT)/,$(addsuffix .md,$(SIPHONS)))

$(BR)/.doxygen-bootstrap.ok: Makefile
	@echo "Checking whether dependencies for Doxygen are installed..."
ifeq ($(OS_ID),ubuntu)
	@set -e; inst=; \
		for i in $(DOC_DEB_DEPENDS); do \
			dpkg-query --show $$i >/dev/null 2>&1 || inst="$$inst $$i"; \
		done; \
		if [ "$$inst" ]; then \
			sudo apt-get update; \
			sudo apt-get $(CONFIRM) $(FORCE) install $$inst; \
		fi
	@if [ ! -s /usr/lib/graphviz/config6a ]; then \
		echo "Rebuidlding system Graphviz configuration."; \
		sudo dot -c; \
	fi
else ifneq ("$(wildcard /etc/redhat-release)","")
	@sudo yum install $(CONFIRM) $(DOC_RPM_DEPENDS)
else ifeq ($(OS_ID),darwin)
	@set -e; \
	for bin in $(DOC_MAC_BIN_DEPENDS); do \
		which -s $${bin} || (\
			echo "Program '$${bin}' not found, please install it."; \
			false; \
		); \
	done
	@set -e; \
	for py in $(DOC_MAC_PY_DEPENDS); do \
		python -c "import $${py}" >/dev/null 2>&1 || (\
			echo "Python package '$${py}' not found, please install it."; \
			false; \
		); \
	done
else
	$(error "Building documentation currently works only on Ubuntu, CentOS or MacOS systems.")
endif
	@touch $@

.PHONY: bootstrap-doxygen
bootstrap-doxygen: $(BR)/.doxygen-bootstrap.ok

.DELETE_ON_ERROR: $(BR)/.doxygen-siphon.dep
$(BR)/.doxygen-siphon.dep: Makefile
	@echo "Building siphon dependencies..."
	@set -e; rm -f "$@"; for input in $(DOXY_INPUT); do \
		find "$(WS_ROOT)/$$input" -type f \
			\( -name '*.[ch]' -or -name '*.dox' \) -print \
			| grep -v -E '^$(WS_ROOT)/$(DOXY_EXCLUDE_REGEXP)' \
			| sed -e "s/^/\$$(SIPHON_FILES): /" \
			>> $@; \
	done

# Include the source -> siphon dependencies
-include $(BR)/.doxygen-siphon.dep

# Generate .siphon files that contain fragments of source file that
# relate to the siphons we support.
.NOTPARALLEL: $(SIPHON_FILES)
$(SIPHON_FILES): $(BR)/.doxygen-bootstrap.ok \
		$(DOXY_DIR)/siphon-generate \
		$(wildcard $(DOXY_DIR)/siphon/*.py)
	@rm -rf "$(SIPHON_INPUT)" "$(SIPHON_OUTPUT)"
	@mkdir -p "$(SIPHON_INPUT)" "$(SIPHON_OUTPUT)"
	@touch $(SIPHON_INPUT)/files
	@echo "Collating source file list for siphoning..."
	@for input in $(DOXY_INPUT); do \
		cd "$(WS_ROOT)"; \
		find "$$input" -type f \
			\( -name '*.[ch]' -or -name '*.dox' \) -print \
			| grep -v -E '^$(DOXY_EXCLUDE_REGEXP)' \
			>> $(SIPHON_INPUT)/files; \
	done
	@echo "Generating siphons..."
	@set -e; \
	cd "$(WS_ROOT)"; \
	$(DOXY_DIR)/siphon-generate \
		--output="$(SIPHON_INPUT)" \
		"@$(SIPHON_INPUT)/files"

# Process the .siphon source fragments and render them into doxygen flavored
# markdown documentation
.DELETE_ON_ERROR: $(SIPHON_DOCS)
$(SIPHON_OUTPUT)/%.md: $(SIPHON_INPUT)/%.siphon \
		$(DOXY_DIR)/siphon-process \
		$(wildcard $(DOXY_DIR)/siphon/*.py) \
		$(wildcard $(DOXY_DIR)/siphon_templates/*/*.md)
	@echo "Processing siphon from $(notdir $<)..."
	@set -e; \
	cd "$(WS_ROOT)"; \
	$(DOXY_DIR)/siphon-process \
		--type=$(basename $(notdir $<)) \
		--output="$@" \
		"$<"

# This target can be used just to generate the siphoned docs
.PHONY: doxygen-siphon
doxygen-siphon: $(SIPHON_DOCS)

# Generate the doxygen docs
.PHONY: doxygen
doxygen: $(SIPHON_DOCS)
	@mkdir -p "$(DOXY_OUTPUT)"
	@echo "Running Doxygen..."
	set -e; cd "$(WS_ROOT)"; \
	    ROOT="$(WS_ROOT)" \
	    BUILD_ROOT="$(BR)" \
	    INPUT="$(addprefix $(WS_ROOT)/,$(DOXY_INPUT)) $(EXTRA_DOXY_INPUT)" \
	    INCLUDE_PATH="$(DOXY_INCLUDE_PATH)" \
	    EXCLUDE="$(DOXY_EXCLUDE)" \
	    HTML=YES \
	    VERSION="`git describe --tags --dirty`" \
	    doxygen $(DOXY_DIR)/doxygen.cfg

.PHONY: wipe-doxygen
wipe-doxygen:
	rm -rf "$(BR)/docs" "$(BR)/.doxygen-siphon.d"

.PHONY: clean
clean: wipe-doxygen
