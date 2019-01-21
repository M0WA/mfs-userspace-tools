#pragma once

#define MAX_LEN_DEVICENAME 255

int open_blockdevice(const char *device, int *fh);
int write_blockdevice(int fh,void *data,size_t datalen);