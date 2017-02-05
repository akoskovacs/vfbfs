/*
 * Virtual userspace filesystem for framebuffers
 *
 * Copyright (C) 2017 Akos Kovacs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <vfbfs.h>

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

extern struct vfbfs_file_ops vfbfs_gen_file_oprs;

static struct vfbfs_options {
    int show_help;
} vfbfs_options;

#define OPTION(t, p) \
    { t, offsetof(struct vfbfs_options, p), 1 }

static const struct fuse_opt option_spec[] = {
//    OPTION("--help", show_help),
//    OPTION("-h", show_help),
    FUSE_OPT_END
};

struct vfbfs *vfbfs_get_fs(void)
{
    struct fuse_context *ctx = fuse_get_context();
    return (struct vfbfs *)ctx->private_data;
}

static void *vfbfs_fo_init(struct fuse_conn_info *ci)
{
    (void) ci;
    return vfbfs_get_fs();
}

static void vfbfs_fo_destroy(void *p)
{
    (void)p;
}

static int vfbfs_fo_open(const char *path, struct fuse_file_info *fi)
{
    struct vfbfs *fs        = vfbfs_get_fs();
    struct vfbfs_file *file = vfbfs_lookup(fs, path);
    if (file != NULL) {
        fi->fh = (uint64_t)file;
        if (file->vf_oprs != NULL && file->vf_oprs->f_open) {
            return file->vf_oprs->f_open(fs, file, path, fi);
        }
        return 0;
    }
    return -ENOENT;
}

static int vfbfs_fo_read(const char *path, char *data, size_t size
    , off_t off, struct fuse_file_info *fi)
{
    struct vfbfs *fs        = vfbfs_get_fs();
    struct vfbfs_file *file = (struct vfbfs_file *)fi->fh;
    if (file != NULL) {
        if (vfbfs_file_is_dir(file)) {
            return -EISDIR;
        }
        if (file->vf_oprs != NULL && file->vf_oprs->f_read) {
            return file->vf_oprs->f_read(fs, file, path, data, size, off, fi);
        }
        return 0;
    }
    return -EBADF;
}

static int vfbfs_fo_write(const char *path, const char *data, size_t size
    , off_t off, struct fuse_file_info *fi)
{
    struct vfbfs *fs        = vfbfs_get_fs();
    struct vfbfs_file *file = (struct vfbfs_file *)fi->fh;
    if (file != NULL) {
        if (vfbfs_file_is_dir(file)) {
            return -EISDIR;
        }
        if (file->vf_oprs != NULL && file->vf_oprs->f_write != NULL) {
            return file->vf_oprs->f_write(fs, file, path, data, size, off, fi);
        }
        return 0;
    }
    return -EBADF;
}

static int vfbfs_fo_close(const char *path, struct fuse_file_info *fi)
{
    struct vfbfs *fs        = vfbfs_get_fs();
    struct vfbfs_file *file = (struct vfbfs_file *)fi->fh;
    if (file != NULL) {
        fi->fh = (uint64_t)file;
        if (file->vf_oprs != NULL && file->vf_oprs->f_close != NULL) {
            return file->vf_oprs->f_close(fs, file, path, fi);
        }
        return 0;
    }
    return -ENOENT;
}

static int vfbfs_fo_release(const char *path, struct fuse_file_info *fi)
{
    struct vfbfs *fs        = vfbfs_get_fs();
    struct vfbfs_file *file = (struct vfbfs_file *)fi->fh;
    if (file != NULL) {
        fi->fh = (uint64_t)file;
        if (file->vf_oprs != NULL && file->vf_oprs->f_release != NULL) {
            return file->vf_oprs->f_release(fs, file, path, fi);
        }
        return 0;
    }
    return -ENOENT;
}

static int vfbfs_fo_getattr(const char *path, struct stat *st)
{
    struct vfbfs *fs        = vfbfs_get_fs();
    if (fs == NULL) {
        return -ENOENT;
    }
    struct vfbfs_file *file = vfbfs_lookup(fs, path);
    if (file != NULL) {
        // fi->fh = (uint64_t)file;
        if (vfbfs_file_is_dir(file)) {
            struct vfbfs_dir *d = (struct vfbfs_dir *)file->vf_private;
            if (d != NULL && d->vd_oprs != NULL && d->vd_oprs->d_getattr != NULL) {
                return d->vd_oprs->d_getattr(fs, d, path, st);
            }
        } else {
            if (file->vf_oprs != NULL && file->vf_oprs->f_getattr != NULL) {
                return file->vf_oprs->f_getattr(fs, file, path, st);
            }
        }
        return 0;
    }
    return -ENOENT;
}

static int vfbfs_fo_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    return 0;
}

static int vfbfs_fo_opendir(const char *path, struct fuse_file_info *fi)
{
    struct vfbfs *fs        = vfbfs_get_fs();
    struct vfbfs_file *fd   = vfbfs_lookup(fs, path);
    struct vfbfs_dir  *dir;
    if (fd != NULL) {
        if (!vfbfs_file_is_dir(fd)) {
            return -ENOTDIR;
        }
        dir    = (struct vfbfs_dir *)fd->vf_private;
        fi->fh = (uint64_t)fd;
        if (dir->vd_oprs != NULL && dir->vd_oprs->d_open != NULL) {
            return dir->vd_oprs->d_open(fs, dir, path, fi);
        }
        return 0;
    }
    return -ENOENT;
}

static int vfbfs_fo_readdir(const char *path, void *buf, fuse_fill_dir_t filler
    , off_t off, struct fuse_file_info *fi)
{
    struct vfbfs *fs        = vfbfs_get_fs();
    struct vfbfs_file *fd   = (struct vfbfs_file *)fi->fh;
    struct vfbfs_dir *dir;
    if (vfbfs_file_is_dir(fd)) {
        dir = (struct vfbfs_dir *)fd->vf_private;
        if (dir->vd_oprs != NULL && dir->vd_oprs->d_read != NULL) {
            return dir->vd_oprs->d_read(fs, dir, path, buf, filler, off, fi);
        }
        return 0;
    }
    return -EBADF;
}

static int vfbfs_fo_releasedir(const char *path, struct fuse_file_info *fi)
{
    struct vfbfs *fs        = vfbfs_get_fs();
    struct vfbfs_file *fd   = (struct vfbfs_file *)fi->fh;
    struct vfbfs_dir *dir;
    if (vfbfs_file_is_dir(fd)) {
        dir = (struct vfbfs_dir *)fd->vf_private;
        if (dir->vd_oprs != NULL && dir->vd_oprs->d_release != NULL) {
            return dir->vd_oprs->d_release(fs, dir, path, fi);
        }
        return 0;
    }
    return -EBADF;
}

struct vfbfs_superblock *vfbfs_superblock_alloc(struct vfbfs *fs)
{
    (void) fs;
    struct vfbfs_superblock *sb = (struct vfbfs_superblock *)malloc(sizeof(struct vfbfs_superblock));
    sb->sb_mountpoint = NULL;
    sb->sb_file_count = 0;
    sb->sb_def_oprs    = &vfbfs_gen_file_oprs;
    //pthread_rwlockattr_init(&sb.w_lock);
    pthread_mutex_init(&sb->sb_wlock, NULL);
    sb->sb_fs_oprs  = (struct fuse_operations) {
        .init       = vfbfs_fo_init,
        .destroy    = vfbfs_fo_destroy,

        .open       = vfbfs_fo_open,
        .read       = vfbfs_fo_read,
        .write      = vfbfs_fo_write,
        .release    = NULL,
        //.close      = vfbfs_fo_close,
        .release    = vfbfs_fo_release,
        .getattr    = vfbfs_fo_getattr,

        .create     = vfbfs_fo_create,
        .opendir    = vfbfs_fo_opendir,
        .readdir    = vfbfs_fo_readdir,
        .releasedir = vfbfs_fo_releasedir
    };
    return sb;
}

struct vfbfs *vfbfs_init(struct vfbfs *fs)
{
    struct vfbfs_superblock *sb = vfbfs_superblock_alloc(fs);
    fs->fs_superblock = sb;
    sb->sb_fs = fs;
    sb->sb_root = vfbfs_dir_new(fs, strdup("/"));
    sb->sb_root->vd_superblock = sb;
    return fs;
}

struct vfbfs *vfbfs_fs_alloc(void)
{
    struct vfbfs *fs = (struct vfbfs *)malloc(sizeof(struct vfbfs));
    fs->fs_superblock = NULL;
    fs->fs_abs_path   = NULL;
    return fs;
}

struct vfbfs *vfbfs_new(void)
{
    struct vfbfs *fs = vfbfs_fs_alloc();
    return vfbfs_init(fs);
}

int vfbfs_main(struct vfbfs *fs, int argc, char *argv[])
{
    struct vfbfs_superblock *sb = fs->fs_superblock;
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    if (fuse_opt_parse(&args, &vfbfs_options, option_spec, NULL) == -1) {
        return 1;
    }
    fs->fs_abs_path = get_current_dir_name();
    return fuse_main(args.argc, args.argv, &sb->sb_fs_oprs, fs);
}

int main(int argc, char *argv[])
{
    struct vfbfs fs;
    struct vfbfs_dir *fb, *config;
    struct vfbfs_file *readme;
    char *msg = strdup("This is a readme file!\n");

    /* TODO --help */
    openlog("vfbfs", LOG_CONS|LOG_PID, LOG_USER);
    vfbfs_init(&fs);

    fb = vfbfs_dir_create_in(&fs, NULL, "fb");
    config = vfbfs_dir_create_in(&fs, NULL, "config");
    readme = vfbfs_file_create_in(&fs, config, "readme.txt");
    readme->vf_private = msg;
    readme->vf_stat.st_size = strlen(msg)+1;

    return vfbfs_main(&fs, argc, argv);
}
