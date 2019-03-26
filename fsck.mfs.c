#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <inttypes.h>
#include <alloca.h>

#include "libmfs.h"

#include <fs.h>
#include <superblock.h>
#include <inode.h>

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
    printf("\
usage: %s -d <devicename> [-v]\n\n\
checks and repairs a mfs filesystem on a device\n\
version %lu.%lu\n\
    -d <device>   : blockdevice name\n\
    -f            : force check\n\
    -v            : verbose, use twice for debug\n\
    -h            : help\n\
",executable,MFS_GET_MAJOR_VERSION(MFS_VERSION),MFS_GET_MINOR_VERSION(MFS_VERSION));
}

static void dump_superblock(const struct mfs_super_block *sb)
{
    uint64_t capacity_mb,freemap_size,metadata_mb;

    freemap_size = BITS_TO_LONGS(sb->block_count) * sizeof(unsigned long);
    capacity_mb = (sb->block_size*sb->block_count) / ( 1024 * 1024 );
    metadata_mb = (MFS_SUPERBLOCK_SIZE + freemap_size + sizeof(struct mfs_inode)) / ( 1024 * 1024 );

    fprintf(stderr,"\
superblock:\n\
    version         : %lu.%lu\n\
    magic           : 0x%" PRIx64 "\n\
    block_size      : %" PRIu32 "\n\
    block_count     : %" PRIu64 "\n\
    freemap_block   : %" PRIu64 "\n\
    rootinode_block : %" PRIu64 "\n\
    next_ino        : %" PRIu64 "\n\
    mounted         : %u\n\
    # mounts        : %" PRIu64 "\n\
filesystem:\n\
    capacity        : %" PRIu64 "MB\n\
    metadata        : %" PRIu64 "MB\n\
        freemap size: %" PRIu64 "B\n\
",  MFS_GET_MAJOR_VERSION(sb->version),MFS_GET_MINOR_VERSION(sb->version),
    sb->magic,sb->block_size,sb->block_count,sb->freemap_block,
    sb->rootinode_block,sb->next_ino,sb->mounted,sb->mount_cnt,
    capacity_mb,metadata_mb,freemap_size);
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
    void *freemap;
    
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

    if(sb.mounted) {
        if( !conf->force ) {
            fprintf(stderr,"cannot operate on mounted filesystem, use -f to force");
            err = EINVAL;
            goto release;
        } else {
            fprintf(stderr,"warn: operating on mounted filesystem");
        }
    }

    bitmap_bytes = BITS_TO_LONGS(sb.block_count) * sizeof(unsigned long);
    freemap = alloca(bitmap_bytes);
    
    err = lseek(fh,sb.freemap_block * sb.block_size,SEEK_SET);
    if( err == (off_t)-1) {
        fprintf(stderr,"cannot find freemap");
        err = EINVAL;
        goto release;
    }

    err = read_blockdevice(fh,freemap,bitmap_bytes);
    if(err) {
        fprintf(stderr,"cannot read freemap");
        err = EINVAL;
        goto release;
    }

    if(conf->verbose) {    
        dump_superblock(&sb);
        
        if( conf->verbose > 1 ) {
            fprintf(stderr,"freemap:\n");
            print_bitmap(bitmap_bytes, freemap);
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
