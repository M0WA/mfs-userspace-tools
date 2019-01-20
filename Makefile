BINARY=mkfs.mfs
GCC=gcc

CFLAGS := -I../kernel-module/

all: clean $(BINARY)

$(BINARY):
	$(GCC) $(CFLAGS) mkfs.mfs.c -o $(BINARY)

clean:
	rm -f *.o $(BINARY)

.PHONY: all clean