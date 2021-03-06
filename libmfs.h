#pragma once

#include <stddef.h>
#include <stdint.h>

#define MAX_LEN_DEVICENAME 255
typedef uint64_t sector_t;

int open_blockdevice(const char *device, int *fh);
int close_blockdevice(int fh);
int write_blockdevice(int fh,void *data,size_t datalen);
int read_blockdevice(int fh,void *data, size_t datalen);
uint64_t bytecount_blockdevice(int fh);
unsigned int sectorsize_blockdevice(int fh);
void print_bitmap(size_t const size, void const * const ptr);
