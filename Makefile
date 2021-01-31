#CROSS_COMPILE=../../../../../crossgcc/armv5-eabi--glibc--stable/bin/armv5-glibc-linux-
CROSS_COMPILE=../../../../../crossgcc/arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
CC = $(CROSS_COMPILE)gcc

OUT := sp_ipc_test

all: sp_ipc_test.o
	$(CC) -O -o $(OUT) $^

%.o: %.c
	$(CC) -O -c -o $@ $<

clean:
	@rm -f *.o $(OUT)

