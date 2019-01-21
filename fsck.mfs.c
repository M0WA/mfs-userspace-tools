#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <fs.h>

#include "libmfs.h"

static struct option long_options[] = {
    {"device"   , required_argument, 0, 'd'},
    {"verbose"  , no_argument      , 0, 'v'},
    {"help"     , no_argument      , 0, 'h'},
    {0, 0, 0, 0}
};

struct mfs_fsck_config {
    int verbose;
    char device[MAX_LEN_DEVICENAME];
};

static void show_usage(const char *executable) {
    printf(
"checks and repairs a mfs filesystem on a device\n\
%s -d <devicename> [-v]\n\
    -d <device>   : blockdevice name\n\
    -v            : verbose\n\
    -h            : help\n\
version: %lu.%lu\n\
",executable,MFS_GET_MAJOR_VERSION(MFS_VERSION),MFS_GET_MINOR_VERSION(MFS_VERSION));
}

static int parse_commandline(int argc,char ** argv, struct mfs_fsck_config *config) 
{
    int c;
    int option_index = 0;
    while( (c = getopt_long(argc, argv, "d:hv",long_options, &option_index)) != -1 ) {
        switch(c) {
        case 'h':
            show_usage(argv[0]);
            exit(0);
        case 'v':
            config->verbose = 1;
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

release:
    return 0;
}