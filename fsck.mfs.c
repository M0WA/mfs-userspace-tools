#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <inttypes.h>
#include <alloca.h>

#include <fs.h>
#include <superblock.h>

#include "libmfs.h"

#define BITS_PER_BYTE           8
#define DIV_ROUND_UP(n,d)       (((n) + (d) - 1) / (d))
#define BITS_TO_LONGS(nr)       DIV_ROUND_UP(nr, BITS_PER_BYTE * sizeof(long))

static struct option long_options[] = {
    {"device"   , required_argument, 0, 'd'},
    {"force"    , no_argument      , 0, 'f'},
    {"verbose"  , no_argument      , 0, 'v'},
    {"help"     , no_argument      , 0, 'h'},
    {0, 0, 0, 0}
};

struct mfs_fsck_config {
    int verbose;
    int force;
    char device[MAX_LEN_DEVICENAME];
};

static void show_usage(const char *executable) {
    printf(
"checks and repairs a mfs filesystem on a device\n\
%s -d <devicename> [-v]\n\
    -d <device>   : blockdevice name\n\
    -f            : force check\n\
    -v            : verbose, use twice for debug\n\
    -h            : help\n\
version: %lu.%lu\n\
",executable,MFS_GET_MAJOR_VERSION(MFS_VERSION),MFS_GET_MINOR_VERSION(MFS_VERSION));
}

static void dump_inode()
{
}

static void dump_superblock_bitmap(int fh,uint64_t size_bytes)
{
    void* buf;
    int err;

    buf = alloca(size_bytes);

    err = read_blockdevice(fh,buf,size_bytes);
    if(err) {
        //TODO: print error
        return;
    }

    print_bitmap(size_bytes, buf);
}

static void dump_superblock(const struct mfs_super_block *sb)
{
    fprintf(stderr,"\
superblock: \n\
    sb->version        : %lu.%lu \n\
    sb->magic          : 0x%" PRIx64 " \n\
    sb->block_size     : %" PRIu32 " \n\
    sb->block_count    : %" PRIu64 " \n\
    sb->freemap_block  : %" PRIu64 " \n\
    sb->rootinode_block: %" PRIu64 " \n\
    sb->next_ino       : %" PRIu64 " \n\
    sb->mounted        : %u\n",
    MFS_GET_MAJOR_VERSION(sb->version),MFS_GET_MINOR_VERSION(sb->version),
    sb->magic,sb->block_size,sb->block_count,sb->freemap_block,
    sb->rootinode_block,sb->next_ino,sb->mounted);
}

static int read_superblock(int fh, struct mfs_super_block *sb) 
{
    int err;

    memset(sb,0,sizeof(struct mfs_super_block));

    if( lseek(fh,MFS_SUPERBLOCK_BLOCK,SEEK_SET) == -1 ) {
        fprintf(stderr,"cannot seek to superblock for reading %s\n",strerror(errno));
    }

    err = read_blockdevice(fh,sb,sizeof(struct mfs_super_block));
    if( err != 0 ) {
        return err;
    }
    return 0;
}

static int verify_magic_number(const struct mfs_super_block *sb)
{
    if( sb->magic != MFS_MAGIC_NUMBER ) {
        fprintf(stderr,"wrong magic number for fs\n");
        return 1;
    }
    return 0;
}

static int verify_version(const struct mfs_super_block *sb)
{
    if( MFS_GET_MAJOR_VERSION(sb->version) != MFS_GET_MAJOR_VERSION(MFS_VERSION) ||
        MFS_GET_MINOR_VERSION(sb->version) != MFS_GET_MINOR_VERSION(MFS_VERSION) ) {
        fprintf(stderr,"fs and userspace tools differ in version, please use up-to-date tools\n");
        return 1;
    }    
    return 0;
}

static int verify_filesystem(const struct mfs_fsck_config *conf) 
{
    int fh, err;
    uint64_t bitmap_bytes;
    uint64_t bytes,blocks;
    struct mfs_super_block sb;
    
    if(conf->verbose) {
        fprintf(stderr,"opening block device %s\n",conf->device); }
    err = open_blockdevice(conf->device, &fh);
    if( err != 0 ) {
        goto release; }
    if(conf->verbose) {
        fprintf(stderr,"block device %s is open\n",conf->device); }

    if(conf->verbose) {
        fprintf(stderr,"reading superblock from device %s\n",conf->device); }
    err = read_superblock(fh, &sb);
    if( err != 0 ) {
        goto release; }
    if(conf->verbose) {
        fprintf(stderr,"read superblock from device %s\n",conf->device); }

    if(conf->verbose) {
        fprintf(stderr,"checking filesystem version\n"); }
    if(verify_version(&sb) && !conf->force) {
        err = EINVAL;
        goto release; }
    if(conf->verbose) {
        fprintf(stderr,"filesystem version checked\n"); }

    if(conf->verbose) {
        fprintf(stderr,"checking magic number\n"); }
    if( verify_magic_number(&sb) && !conf->force ) {
        err = EINVAL;
        goto release; }
    if(conf->verbose) {
        fprintf(stderr,"magic number checked\n"); }

    if(sb.mounted && !conf->force) {
        err = EINVAL;
        fprintf(stderr,"cannot operate on mounted filesystem, use -f to enforce");
        goto release;
    }

    bytes = bytecount_blockdevice(fh);
    blocks = bytes / sb.block_size;
    bitmap_bytes = BITS_TO_LONGS(blocks) * sizeof(unsigned long);

    if(conf->verbose) {    
        dump_superblock(&sb);
        
        if( conf->verbose > 1 ) {
            fprintf(stderr,"freemap:\n");
            lseek(fh,sb.freemap_block * sb.block_size,SEEK_SET);
            dump_superblock_bitmap(fh,bitmap_bytes);
        }
    }

release:
    if(fh) {
        if(conf->verbose) {
            fprintf(stderr,"closing blockdevice %s\n",conf->device); }
        err = close_blockdevice(fh);
        if(conf->verbose) {
            fprintf(stderr,"blockdevice %s closed\n",conf->device); }
    }
    return err;
}

static int parse_commandline(int argc,char ** argv, struct mfs_fsck_config *config) 
{
    int c;
    int option_index = 0;
    while( (c = getopt_long(argc, argv, "d:fhv",long_options, &option_index)) != -1 ) {
        switch(c) {
        case 'h':
            show_usage(argv[0]);
            exit(0);
        case 'v':
            config->verbose++;
            break;
        case 'f':
            config->force = 1;
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
            return 1;
        }
    }

    if(!config->device[0]) {
        fprintf(stderr,"no device given, please specify -d <device>\n");
        return 1;
    }

    return 0;
}

int main(int argc,char ** argv) 
{
    struct mfs_fsck_config conf;
    int err = 0;
    memset(&conf,0,sizeof(struct mfs_fsck_config));

    err = parse_commandline(argc,argv,&conf);
    if( err != 0 ) {
        goto release; }

    err = verify_filesystem(&conf);

release:
    return err;
}
