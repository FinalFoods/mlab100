#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

include sdkconfig

HOSTPLATFORM:=$(shell uname -s)

# Seconds since epoch used to identify particular build:
export BUILDTIMESTAMP=${shell date +%s}

# Following will return tag if in git tree (e.g. "v1.0-4") or use
# "Unknown" for non-zero response (when the git information is not
# available):
export GITTAG = ${shell git describe --tags 2> /dev/null; if [ $$? -ne 0 ]; then echo Unknown; fi}
# When the repo is at a specific tag point then GITTAG will be of the form:
#  v0.1
# When the repo is at an arbitrary commit point then GITTAG will be of the form:
#  v0.1-1-g0efaa4f
# where the short commit identifier tag is appended to the last
# specific "release" tag, along with the count of the changesets since
# that tag point.

# Example for adding build host specific rules or definitions if needed:
ifeq "${HOSTPLATFORM}" "Linux"
# export LD_LIBRARY_PATH= ${TOPDIR_MLAB100}/3rd_party/tools/lib
else ifeq "${HOSTPLATFORM}" "Darwin"
# export DYLD_LIBRARY_PATH= ${TOPDIR_MLAB100}/3rd_party/tools/lib
else # all other build hosts
$(error "Build host not supported")
endif # all other build hosts

PROJECT_NAME := mlab100

# Reference extra components (outside the esp-idf tree):
EXTRA_COMPONENT_DIRS := \
	${TOPDIR_MLAB100}/components/mlab \
	${TOPDIR_MLAB100}/3rd_party/components/onewire

include $(IDF_PATH)/make/project.mk

