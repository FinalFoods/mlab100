#> Makefile
#==============================================================================

# The TARGETS definitions is the list of esp-idf projects to be built
# by default:
TARGETS = mlab100

#------------------------------------------------------------------------------

all: tools ${TARGETS}


#------------------------------------------------------------------------------

TOOLPREFIX = xtensa-esp32-elf-
CC = ${TOOLPREFIX}gcc
LD = ${TOOLPREFIX}ld
OBJCOPY = ${TOOLPREFIX}objcopy

#------------------------------------------------------------------------------

export TOPDIR_MLAB100=$(shell pwd)

# Build host
HOSTPLATFORM:=$(shell uname -s)
# Linux: Linux
# Mac OS X: Darwin

#------------------------------------------------------------------------------

DIRPKGS=$(shell cd 3rd_party;pwd)/packages
DIRTOOLS=$(shell cd 3rd_party;pwd)/tools

ifeq "${HOSTPLATFORM}" "Linux"

HOSTARCH:=$(shell uname -i)

ESPRESSIF_DOWNLOAD = https://dl.espressif.com/dl/

ifeq "${HOSTARCH}" "x86_64" # 64-bit
TOOLSUBDIR = linux64
PKGTOOLCHAIN = xtensa-esp32-elf-linux64-1.22.0-80-g6c4433a-5.2.0.tar.gz
export LD_LIBRARY_PATH=${TOOLTREE}/lib
else # assume 32-bit
TOOLSUBDIR = linux32
PKGTOOLCHAIN = xtensa-esp32-elf-linux32-1.22.0-80-g6c4433a-5.2.0.tar.gz
export LD_LIBRARY_PATH=${TOOLTREE}/lib
endif # assume 32-bit
else ifeq "${HOSTPLATFORM}" "Darwin"
TOOLSUBDIR = osx
PKGTOOLCHAIN = xtensa-esp32-elf-osx-1.22.0-80-g6c4433a-5.2.0.tar.gz
export DYLD_LIBRARY_PATH=${TOOLTREE}/lib
else # all other build hosts
$(error "Build host not supported")
endif # all other build hosts

TOOLPATH = ${DIRTOOLS}/${TOOLSUBDIR}/xtensa-esp32-elf/bin
EXTRATOOLPATH = ${DIRTOOLS}/bin

${DIRPKGS}:
	mkdir -p ${DIRPKGS}

${DIRPKGS}/${PKGTOOLCHAIN}: ${DIRPKGS}
	(cd ${DIRPKGS}; curl -o $@ -s ${ESPRESSIF_DOWNLOAD}${PKGTOOLCHAIN})

$(DIRTOOLS)/$(TOOLSUBDIR): ${DIRPKGS}/${PKGTOOLCHAIN}
	@mkdir -p $@
	@echo "====> Extracting toolchain"
	@(cd $@; tar xzf $<)
	@echo "Installed toolchain"

# NOTE: It is critical that the hosted mkspiffs uses the same
# configuration as the esp-idf based firmware. If the format is
# changed then it invalidates all in-field images:
${DIRTOOLS}/bin/mkspiffs:
	@(cd 3rd_party/mkspiffs; make CPPFLAGS="-DSPIFFS_OBJ_META_LEN=4")
	@mkdir -p ${EXTRATOOLPATH}
	@cp 3rd_party/mkspiffs/mkspiffs $@
	@(cd 3rd_party/mkspiffs/spiffs/src; rm -f *.o)
	@echo "Installed mkspiffs"

.PHONY: tools
tools: $(DIRTOOLS)/$(TOOLSUBDIR) ${DIRTOOLS}/bin/mkspiffs 

#------------------------------------------------------------------------------

export PATH := $(PATH):$(TOOLPATH):$(EXTRATOOLPATH)
export IDF_PATH = ${TOPDIR_MLAB100}/3rd_party/esp-idf

#------------------------------------------------------------------------------

.PHONY: ${TARGETS} all

# NOTE: We do a hardwired remove of the main application object (and
# its dependency file) to ensure that every "make" operation will use
# a new BUILDTIMESTAMP to be recorded in the produced binary.

${TARGETS}:
	@rm -f $@/build/main/mlab100.o $@/build/main/mlab100.d
	@${MAKE} -w -C $@ ${MAKECMDGOALS}

#------------------------------------------------------------------------------
# This is just a collection of PHONY targets for some of the targets
# supported by the normal esp-idf Make framework in the sub-project
# tree:

.PHONY: clean app-clean bootloader-clean menuconfig help monitor all_targets \
	size size-components size-symbols list-components \
	app bootloader \
	flash app-flash bootloader-flash \
	erase_flash erase_otadata

clean: ${TARGETS}

menuconfig: ${TARGETS}

app: ${TARGETS}

bootloader: ${TARGETS}

help: ${TARGETS}

size: ${TARGETS}

size-components: ${TARGETS}

size-symbols: ${TARGETS}

flash: ${TARGETS}

app-flash: ${TARGETS}

bootloader-flash: ${TARGETS}

app-clean: ${TARGETS}

bootloader-clean: ${TARGETS}

erase_flash: ${TARGETS}

erase_otadata: ${TARGETS}

monitor: ${TARGETS}

list-components: ${TARGETS}

#------------------------------------------------------------------------------

.PHONY: distclean
distclean: clean
	@rm -rf ${DIRTOOLS}

#==============================================================================
#> EOF Makefile
