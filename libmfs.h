#pragma once

#include <stddef.h>
#include <stdint.h>

#define MAX_LEN_DEVICENAME 255

int open_blockdevice(const char *device, int *fh);
int write_blockdevice(int fh,void *data,size_t datalen);
uint64_t bytecount_blockdevice(int fh);
int sectorsize_blockdevice(int fh);