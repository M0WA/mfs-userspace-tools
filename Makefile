FSNAME=mfs
GCC=gcc

CFLAGS := -I../kernel-module/

all: 
	$(MAKE) clean
	$(MAKE) lib$(FSNAME) 
	$(MAKE) mkfs.$(FSNAME) 
	$(MAKE) fsck.$(FSNAME)

lib$(FSNAME):
	$(GCC) $(CFLAGS) -c lib$(FSNAME).c -o lib$(FSNAME).o

clean_lib$(FSNAME):
	rm -f lib$(FSNAME).o

mkfs.$(FSNAME):
	$(GCC) $(CFLAGS) mkfs.$(FSNAME).c lib$(FSNAME).c -o mkfs.$(FSNAME)

clean_mkfs:
	rm -f mkfs.$(FSNAME).o lib$(FSNAME).o mkfs.$(FSNAME)

fsck.$(FSNAME):
	$(GCC) $(CFLAGS) fsck.$(FSNAME).c lib$(FSNAME).c -o fsck.$(FSNAME)

clean_fsck:
	rm -f fsck.$(FSNAME).o fsck.$(FSNAME)

clean: clean_lib$(FSNAME) clean_fsck clean_mkfs

.PHONY: all clean clean_fsck clean_mkfs clean_lib$(FSNAME)