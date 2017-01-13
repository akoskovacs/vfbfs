#include "vfbfs.h"
#define FUSE_USE_VERSION 30
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static struct vfbfs_options {
    int show_help;
};

#define OPTION(t, p) \
    { t, offsetof(struct vfbfs_options, p), 1 }

static const struct fuse_opt option_spec[] = {
    OPTION("--help", show_help),
    OPTION("-h", show_help),
    FUSE_OPT_END
};

static void *
vfbfs_init(struct fuse_conn_info *ci, struct fuse_config *cfg)
{
    (void) conn;
    cfg->kernel_cache = 0;
    return NULL;
}

static int vfbfs_open(const char *path, struct fuse_file_info *f)
{
    return 0;
}

static struct fuse_operations vfbfs_oprs = {
    .init       = vfbfs_init,
    .open       = vfbfs_open,
    .read       = vfbfs_read,
    .write      = vfbfs_write,
};

int main(int argc, const char *argv[])
{
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    if (fuse_opt_parse(&args, &vfbfs_options, option_spec, NULL) == -1) {
        return 1;
    }

    /* TODO --help */

    return fuse_main(args.argc, args.argv, &vfbfs_oprs, NULL);
}
