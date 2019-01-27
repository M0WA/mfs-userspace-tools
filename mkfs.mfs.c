#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <inttypes.h>

#include <superblock.h>
#include <fs.h>

#include "libmfs.h"

#define MFS_DEFAULT_BLOCKSIZE   0
#define BITS_PER_BYTE           8
#define DIV_ROUND_UP(n,d)       (((n) + (d) - 1) / (d))
#define BITS_TO_LONGS(nr)       DIV_ROUND_UP(nr, BITS_PER_BYTE * sizeof(long))

static struct option long_options[] = {
    {"device"   , required_argument, 0, 'd'},
    {"blocksize", required_argument, 0, 'b'},
    {"verbose"  , no_argument      , 0, 'v'},
    {"help"     , no_argument      , 0, 'h'},
    {0, 0, 0, 0}
};

struct mfs_mkfs_config {
    int verbose;
    char device[MAX_LEN_DEVICENAME];
    uint32_t block_size;
};

static void show_usage(const char *executable) {
    printf(
"creates a mfs filesystem on a device\n\
%s -d <devicename> [-v]\n\
    -d <device>   : blockdevice name\n\
    -b <blocksize>: blocksize in bytes (default: use sectorsize of blockdevice)\n\
    -v            : verbose\n\
    -h            : help\n\
version: %lu.%lu\n\
",executable,MFS_GET_MAJOR_VERSION(MFS_VERSION),MFS_GET_MINOR_VERSION(MFS_VERSION));
}

static int parse_commandline(int argc,char ** argv, struct mfs_mkfs_config *config) 
{
    int c;
    int option_index = 0;
    while( (c = getopt_long(argc, argv, "b:d:hv",long_options, &option_index)) != -1 ) {
        switch(c) {
        case 'h':
            show_usage(argv[0]);
            exit(0);
        case 'v':
            config->verbose = 1;
            break;
        case 'd':
            if(!optarg) {
                fprintf(stderr,"no device found in -d <device>\n");
                return -EINVAL;
            }
            if(strlen(optarg) > (MAX_LEN_DEVICENAME - 1)) {
                fprintf(stderr,"device name too long in -d <device>\n");
                return -EINVAL;
            }
            config->device[0] = 0;
            strncat(config->device,optarg,MAX_LEN_DEVICENAME-1);
            break;
        case '?':
        default:
            fprintf(stderr,"unknown error while parsing command line arguments\n");
            return -EINVAL;
        }
    }

    if(!config->block_size) {
        config->block_size = MFS_DEFAULT_BLOCKSIZE;
    }
    if(!config->device[0]) {
        fprintf(stderr,"no device given, please specify -d <device>\n");
        return -EINVAL;
    }

    return 0;
}

static int create_superblock(const struct mfs_mkfs_config *conf,struct mfs_super_block *sb,uint64_t blocks) 
{
    sb->version = MFS_VERSION;
    sb->magic = MFS_MAGIC_NUMBER;
    sb->block_size = conf->block_size;
    sb->block_count = blocks;
    return 0;
}

static void set_bit_bitmap(unsigned long *bitmap,size_t bit) {
    size_t lpos = bit / sizeof(unsigned long);
    size_t bpos = bit % sizeof(unsigned long);
    size_t mask = 1 << bit;

    unsigned long *bits = &bitmap[lpos];
    (*bits) = (*bits) | mask;
}

static unsigned long *create_zero_bitmap(int fh,uint64_t bits) {

    unsigned long *bitmap = calloc(BITS_TO_LONGS(bits),sizeof(unsigned long));
    if(!bitmap) {
        return NULL;
    }
    return bitmap;
}

static int write_freemap(int fh,uint64_t bits,uint32_t block_size) {
    int err;
    unsigned long *bitmap = NULL;
    size_t used_bytes = sizeof(union mfs_padded_super_block) + (BITS_TO_LONGS(bits) * sizeof(unsigned long) * 2);
    size_t used_blocks = used_bytes / block_size;

    bitmap = create_zero_bitmap(fh,bits);
    if(!bitmap) {
        return -ENOMEM;
    }
    for(size_t b = 0; b < used_blocks; b++) {
        set_bit_bitmap(bitmap,b);
    }
    err = write_blockdevice(fh,bitmap,BITS_TO_LONGS(bits)*sizeof(unsigned long));
    free(bitmap);
    return err;
}

static int write_inodemap(int fh,uint64_t bits) {
    int err;
    unsigned long *bitmap = create_zero_bitmap(fh,bits);
    if(!bitmap) {
        return -ENOMEM;
    }
    err = write_blockdevice(fh,bitmap,BITS_TO_LONGS(bits)*sizeof(unsigned long));
    free(bitmap);
    return err;
}

int main(int argc,char ** argv) 
{
    struct mfs_mkfs_config conf;
    union mfs_padded_super_block sb;
    int fh = -1;
    int err = 0;
    uint64_t bytes = 0;
    uint64_t blocks = 0;
    unsigned int sectorsize = -1;
    memset(&conf,0,sizeof(struct mfs_mkfs_config));
    memset(&sb,0,sizeof(union mfs_padded_super_block));

    err = parse_commandline(argc,argv,&conf);
    if( err != 0 ) {
        goto release; }

    if(conf.verbose) {
        fprintf(stderr,"opening block device %s\n",conf.device); }
    err = open_blockdevice(conf.device, &fh);
    if( err != 0 ) {
        goto release; }
    if(conf.verbose) {
        fprintf(stderr,"block device %s is open\n",conf.device); }

    sectorsize = sectorsize_blockdevice(fh);
    if(conf.block_size == 0) {
        conf.block_size = sectorsize;
    } else {
        fprintf(stderr,"warn: blocksize(%u) does not match sectorsize(%d)\n",conf.block_size,sectorsize);
        if(sectorsize > conf.block_size ) {
            fprintf(stderr,"blocksize(%u) is smaller than sectorsize(%d)\n",conf.block_size,sectorsize);
            err = -EINVAL;
            goto release;
        }
        if( (sectorsize % conf.block_size) != 0 ) {
            fprintf(stderr,"blocksize(%u) is not a multiple of sectorsize(%d)\n",conf.block_size,sectorsize);
            err = -EINVAL;
            goto release;
        }
    }
    if(conf.verbose) {
        fprintf(stderr,"blocksize: %u, sectorsize: %u\n",conf.block_size,sectorsize); }

    bytes = bytecount_blockdevice(fh);
    blocks = bytes / conf.block_size;
    if(!blocks) {
        fprintf(stderr,"block device %s has no free space\n",conf.device);
        goto release; }
    if(conf.verbose) {
        fprintf(stderr,"device has %lu MB free space in %lu blocks\n", ( (blocks*conf.block_size)/1024/1024), blocks ); }

    if(conf.verbose) {
        fprintf(stderr,"creating superblock\n"); }
    err = create_superblock(&conf,&sb.sb,blocks);
    if( err != 0 ) {
        goto release; }
    if(conf.verbose) {
        fprintf(stderr,"superblock created, version %lu.%lu\n",MFS_GET_MAJOR_VERSION(sb.sb.version),MFS_GET_MINOR_VERSION(sb.sb.version)); }

    if(conf.verbose) {
        fprintf(stderr,"writing superblock\n"); }
    err = write_blockdevice(fh,&sb,sizeof(union mfs_padded_super_block));
    if( err != 0 ) {
        goto release; }
    if(conf.verbose) {
        fprintf(stderr,"superblock written\n"); }

    if(conf.verbose) {
        fprintf(stderr,"writing free blocks bitmap (mapsize: %lu KB)\n",(blocks/8/1024)); }
    err = write_freemap(fh,blocks,conf.block_size);
    if( err != 0 ) {
        goto release; }
    if(conf.verbose) {
        fprintf(stderr,"free blocks bitmap written\n"); }

    if(conf.verbose) {
        fprintf(stderr,"writing inode bitmap (mapsize: %lu KB)\n",(blocks/8/1024)); }
    err = write_inodemap(fh,blocks);
    if( err != 0 ) {
        goto release; }
    if(conf.verbose) {
        fprintf(stderr,"inode bitmap written\n"); }

release:
    if(fh > 0) {
        if(conf.verbose) {
            fprintf(stderr,"closing blockdevice\n"); }
        if( close(fh) != 0 ) {
            fprintf(stderr,"error closing blockdevice: %s\n",strerror(errno)); }
        if(conf.verbose) {
            fprintf(stderr,"blockdevice closed\n"); }
    }
    return err;
}