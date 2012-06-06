.SUFFIXES:
.SUFFIXES: .o .c

DYNAMO_DIR=.
SPLOIT_DIR=cs155-exploits/sploits
TARGET_DIR=cs155-exploits/targets

TEST_BIN=$(SPLOIT_DIR)/sploit1

CC=gcc
CFLAGS=-Wall -fPIC -DLINUX -DX86_64 -I $(DYNAMO_DIR)/include

.c.o :
	$(CC) $(CFLAGS) -c $<

shady.so: shady.o shady.c
	$(CC) $(CFLAGS) -shared -Wl,-soname,shady.so -o shady.so $<

.PHONY: sploits targets
sploits:
	make -C $(SPLOIT_DIR) all

targets:
	make -C $(TARGET_DIR) all
	cp $(TARGET_DIR)/target[0-9] /tmp

all: shady.so simpletest

clean:
	rm -f *.o shady
	make -C $(TARGET_DIR) clean
	make -C $(SPLOIT_DIR) clean

run: shady.so
	$(DYNAMO_DIR)/bin64/drrun -client shady.so 0x1 "" $(TEST_BIN)

simpletest: simpletest.o
