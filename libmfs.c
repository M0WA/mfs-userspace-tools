#include "libmfs.h"

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <linux/fs.h>

int open_blockdevice(const char *device, int *fh) 
{
    *fh = open(device,O_RDWR);
    if(*fh <= 0) {
        fprintf(stderr,"could not open device %s for r/w: %s\n",device,strerror(errno));
        return errno;
    }
    return 0;
}

int write_blockdevice(int fh,void *data,size_t datalen)
{
    ssize_t written = write(fh,data,datalen);
    if(written == -1) {
        fprintf(stderr,"could not write to device: %s\n",strerror(errno));
        return errno;
    }
    if(((size_t)written) != datalen) {
        fprintf(stderr,"incomplete write to device: %s\n",strerror(errno));
        return errno;
    }
    if(fsync(fh) != 0) {
        fprintf(stderr,"could not fsync to device: %s\n",strerror(errno));
        return errno;
    }
    return 0;
}

uint64_t bytecount_blockdevice(int fh) {
    uint64_t size;
    if ( ioctl(fh,BLKGETSIZE64,&size) == -1) {
        return 0;
    }
    return size;
}

unsigned int sectorsize_blockdevice(int fh) {
    unsigned int  size;
    if ( ioctl(fh,BLKSSZGET,&size) == -1) {
        return -1;
    }
    return size;
    
}