#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>

#include <superblock.h>
#include <fs.h>

#define MAX_LEN_DEVICENAME    255
#define MFS_DEFAULT_BLOCKSIZE (uint64_t)512

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
    uint64_t block_size;
};

static void show_usage(const char *executable) {
    printf(
"creates a mfs filesystem on a device\n\
%s -d <devicename> [-v]\n\
    -d <device>   : blockdevice name\n\
    -b <blocksize>: blocksize in bytes (default: %" PRIu64 ")\n\
    -v            : verbose\n\
    -h            : help\n\
",executable,MFS_DEFAULT_BLOCKSIZE);
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
                return 1;
            }
            if(strlen(optarg) > (MAX_LEN_DEVICENAME - 1)) {
                fprintf(stderr,"device name too long in -d <device>\n");
                return 1;
            }
            config->device[0] = 0;
            strncat(config->device,optarg,MAX_LEN_DEVICENAME-1);
            break;
        case '?':
            break;
        default:
            fprintf(stderr,"unknown error while parsing command line arguments\n");
            return 1;
        }
    }

    if(!config->block_size) {
        config->block_size = MFS_DEFAULT_BLOCKSIZE;
    }
    if(!config->device[0]) {
        fprintf(stderr,"no device given, please specify -d <device>\n");
        return 1;
    }

    return 0;
}

static int open_blockdevice(const struct mfs_mkfs_config *conf, int *fh) {
    *fh = open(conf->device,O_RDWR);
    if(*fh <= 0) {
        fprintf(stderr,"could not open device %s for r/w: %s\n",conf->device,strerror(errno));
        return errno;
    }
    return 0;
}

static int write_blockdevice(int fh,void *data,size_t datalen) {
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

static int create_superblock(const struct mfs_mkfs_config *conf,struct mfs_super_block *sb) 
{
    sb->version = MFS_VERSION;
    sb->magic = MFS_MAGIC_NUMBER;
    sb->block_size = conf->block_size;
    return 0;
}

int main(int argc,char ** argv) 
{
    struct mfs_mkfs_config conf;
    union mfs_padded_super_block sb;
    int fh = -1;
    int err = 0;
    memset(&conf,0,sizeof(struct mfs_mkfs_config));
    memset(&sb,0,sizeof(union mfs_padded_super_block));

    err = parse_commandline(argc,argv,&conf);
    if( err != 0 ) {
        goto release; }

    if(conf.verbose) {
        fprintf(stderr,"opening block device %s\n",conf.device); }
    err = open_blockdevice(&conf, &fh);
    if( err != 0 ) {
        goto release; }
    if(conf.verbose) {
        fprintf(stderr,"block device %s is open\n",conf.device); }

    if(conf.verbose) {
        fprintf(stderr,"creating superblock\n"); }
    err = create_superblock(&conf,&sb.sb);
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

release:
    if(fh > 0) {
        if(conf.verbose) {
            fprintf(stderr,"closing blockdevice\n"); }
        if( close(fh) != 0 ) {
            fprintf(stderr,"error closing blockdevice: %s\n",strerror(errno)); }
        if(conf.verbose) {
            fprintf(stderr,"blockdevice closed\n"); }
    }
    return 0;
}