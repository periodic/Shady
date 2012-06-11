.SUFFIXES:
.SUFFIXES: .o .c

# needs to provide:
# ARCH (32 or 64)
# DR_DIR (path to DynamoRIO root)
-include LOCAL_VARS

SPLOIT_DIR=cs155-exploits/sploits
TARGET_DIR=cs155-exploits/targets

TEST_BIN=./simpletest

CC=gcc
CFLAGS=-Wall -fPIC -DLINUX -DX86_$(ARCH) $(DEBUG) -I $(DR_DIR)/include -I $(DR_DIR)/ext/include

DR_LIBS=$(DR_DIR)/lib$(ARCH)/release/libdynamorio.so.3.2 \
 $(DR_DIR)/ext/lib$(ARCH)/release/libdrwrap.so \
 $(DR_DIR)/ext/lib$(ARCH)/release/libdrmgr.so \
 $(DR_DIR)/ext/lib$(ARCH)/release/libdrsyms.so \
 $(DR_DIR)/ext/lib$(ARCH)/release/libdrcontainers.a

default: all

.c.o :
	$(CC) $(CFLAGS) -c $<

shady.so: shady.o shady_util.o inst_malloc.o inst_readwrite.o
	$(CC) $(CFLAGS) -shared -Wl,-soname,-shady.so \
	 -o shady.so $^ $(DR_LIBS)

test_malloc.so: test_malloc.o inst_malloc.o inst_stack.o shady_util.o
	$(CC) $(CFLAGS) -shared -Wl,-soname,-test_malloc.so \
	 -o test_malloc.so $^ $(DR_LIBS)

.PHONY: sploits targets
sploits:
	make -C $(SPLOIT_DIR) all

targets:
	make -C $(TARGET_DIR) all
	cp $(TARGET_DIR)/target[0-9] /tmp

all: shady.so test_malloc.so simpletest

clean:
	rm -f *.o shady
	make -C $(TARGET_DIR) clean
	make -C $(SPLOIT_DIR) clean

run: shady.so
	$(DR_DIR)/bin$(ARCH)/drrun -dr_home $(DR_DIR) -client shady.so 0x1 "" $(TEST_BIN)

simpletest: simpletest.o
