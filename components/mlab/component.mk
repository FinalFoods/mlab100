#
# Component Makefile
#

# We cannot just set an "initial" CFLAGS before including the main
# configuration, since it looks like that is used to overload any
# normal CFLAGS setting. So if we want extra compiler options/flags we
# use "CFLAGS+=" after including the standard world.

include ${TOPDIR_MLAB100}/mlab100/sdkconfig

# Provide ".su" files with stage usage information for our functions:
#CFLAGS += -fstack-usage -fdump-rtl-dfinish
CFLAGS += -fstack-usage

# If we have any other component dependencies we need to list them here:
#COMPONENT_DEPENDS := <component_name>

COMPONENT_ADD_INCLUDEDIRS := include
COMPONENT_SRCDIRS := src

# If we want to embed files as binary blobs:
#COMPONENT_EMBED_TXTFILES := <file1>
#COMPONENT_EMBED_TXTFILES += <file2>
