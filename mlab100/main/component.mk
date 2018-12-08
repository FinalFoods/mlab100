#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

CFLAGS += -DBUILDTIMESTAMP=${BUILDTIMESTAMP} -DMLAB_VERSION=${GITTAG}
