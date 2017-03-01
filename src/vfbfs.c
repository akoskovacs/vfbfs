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
    if (ctx != NULL) {
        return (struct vfbfs *)ctx->private_data;
    }
    return NULL;
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
    struct vfbfs *fs      = vfbfs_get_fs();
    struct vfbfs_entry *e = vfbfs_entry_lookup(fs, path);
    struct vfbfs_file  *f = vfbfs_entry_get_file(e);
    if (e == NULL) {
        return -ENOENT;
    } 
    if (f) {
        return vfbfs_file_call_operation(fs, f, VFBFS_F_OPEN, path, fi);
    }
    return -EISDIR;
}

static int vfbfs_fo_read(const char *path, char *data, size_t size
    , off_t off, struct fuse_file_info *fi)
{
    struct vfbfs *fs      = vfbfs_get_fs();
    struct vfbfs_entry *e = (struct vfbfs_entry *)fi->fh;
    struct vfbfs_file  *f = vfbfs_entry_get_file(e);
    if (e == NULL) {
        return -EBADF;
    }
    if (f) {
        return vfbfs_file_call_operation(fs, f, VFBFS_F_READ, path, data, size, off, fi);
    }
    return -EISDIR;
}

static int vfbfs_fo_write(const char *path, const char *data, size_t size
    , off_t off, struct fuse_file_info *fi)
{
    struct vfbfs *fs      = vfbfs_get_fs();
    struct vfbfs_entry *e = (struct vfbfs_entry *)fi->fh;
    struct vfbfs_file  *f = vfbfs_entry_get_file(e);
    if (e == NULL) {
        return -EBADF;
    } 
    if (f) {
        return vfbfs_file_call_operation(fs, f, VFBFS_F_WRITE, path, data, size, off, fi);
    }
    return -EISDIR;
}

static int vfbfs_fo_truncate(const char *path, off_t size)
{
    struct vfbfs *fs      = vfbfs_get_fs();
    struct vfbfs_entry *e = vfbfs_entry_lookup(fs, path);
    struct vfbfs_file  *f = vfbfs_entry_get_file(e);
    if (e == NULL) {
        return -EBADF;
    } 
    if (f) {
        return vfbfs_file_call_operation(fs, f, VFBFS_F_TRUNCATE, path, size);
    }
    return -EISDIR;
}

int vfbfs_fo_fsync(const char *path, int op, struct fuse_file_info *fi)
{
    (void)path;
    (void)op;
    (void)fi;
    return 0;
}

static int vfbfs_fo_close(const char *path, struct fuse_file_info *fi)
{
    struct vfbfs *fs      = vfbfs_get_fs();
    struct vfbfs_entry *e = (struct vfbfs_entry *)fi->fh;
    struct vfbfs_file  *f = vfbfs_entry_get_file(e);
    if (e == NULL) {
        return -EBADF;
    } 
    if (f) {
        return vfbfs_file_call_operation(fs, f, VFBFS_F_CLOSE, path, fi);
    }
    return -EISDIR;
}

static int vfbfs_fo_release(const char *path, struct fuse_file_info *fi)
{
    struct vfbfs *fs      = vfbfs_get_fs();
    struct vfbfs_entry *e = (struct vfbfs_entry *)fi->fh;
    struct vfbfs_file  *f = vfbfs_entry_get_file(e);
    if (e == NULL) {
        return -EBADF;
    } 
    if (f) {
        return vfbfs_file_call_operation(fs, f, VFBFS_F_RELEASE, path, fi);
    }
    return -ENOENT;
}

static int vfbfs_fo_getattr(const char *path, struct stat *st)
{
    struct vfbfs *fs        = vfbfs_get_fs();
    struct vfbfs_entry *e   = vfbfs_entry_lookup(fs, path);
    if (e != NULL) {
        if (vfbfs_entry_is_dir(e)) {
            return vfbfs_dir_call_operation(fs, e->e_elem.dir, VFBFS_D_GETATTR, path, st);
        } else {
            return vfbfs_file_call_operation(fs, e->e_elem.file, VFBFS_F_GETATTR, path, st);
        }
    }
    return -ENOENT;
}

static int vfbfs_fo_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    struct vfbfs *fs        = vfbfs_get_fs();
    char         *dirname   = strdup(path);
    const char *fname       = strrchr(path, '/')+1;
    struct vfbfs_entry *e;
    if (dirname == NULL || fname == NULL) {
        return -ENOSPC;
    }
    dirname[fname-path] = '\0';
    e = vfbfs_entry_lookup(fs, dirname);
    if (e == NULL) {
        return -ENOENT;
    }
    free(dirname);
    if (vfbfs_entry_is_dir(e)) {
        return vfbfs_dir_call_operation(fs, e->e_elem.dir, VFBFS_D_CREATE, path, fname, mode, fi);
    }

    return -ENOTDIR;
}

static int vfbfs_fo_opendir(const char *path, struct fuse_file_info *fi)
{
    struct vfbfs *fs        = vfbfs_get_fs();
    struct vfbfs_entry *e   = vfbfs_entry_lookup(fs, path);
    struct vfbfs_dir  *dir  = vfbfs_entry_get_dir(e);
    if (e != NULL) {
        if (dir == NULL) {
            return -ENOTDIR;
        }
        return vfbfs_dir_call_operation(fs, dir, VFBFS_D_OPEN, path, fi);
    }
    return -ENOENT;
}

static int vfbfs_fo_readdir(const char *path, void *buf, fuse_fill_dir_t filler
    , off_t off, struct fuse_file_info *fi)
{
    struct vfbfs *fs        = vfbfs_get_fs();
    struct vfbfs_entry *e   = vfbfs_entry_lookup(fs, path);
    struct vfbfs_dir  *dir  = vfbfs_entry_get_dir(e);
    if (dir) {
        return vfbfs_dir_call_operation(fs, dir, VFBFS_D_READ, path, buf, filler, off, fi);
    }
    return -EBADF;
}

static int vfbfs_fo_releasedir(const char *path, struct fuse_file_info *fi)
{
    struct vfbfs *fs        = vfbfs_get_fs();
    struct vfbfs_entry *e   = vfbfs_entry_lookup(fs, path);
    struct vfbfs_dir  *dir  = vfbfs_entry_get_dir(e);
    if (dir) {
        return vfbfs_dir_call_operation(fs, dir, VFBFS_D_RELEASE, fi);
    }
    return -EBADF;
}

struct vfbfs_superblock *vfbfs_superblock_alloc(struct vfbfs *fs)
{
    (void) fs;
    struct vfbfs_superblock *sb = (struct vfbfs_superblock *)malloc(sizeof(struct vfbfs_superblock));
    sb->sb_mountpoint = NULL;
    sb->sb_file_count = 0;
    sb->sb_dfile_oprs  = vfbfs_file_get_mem_ops();
    sb->sb_dentry_oprs = vfbfs_entry_get_mem_ops();
    sb->sb_ddir_oprs   = vfbfs_dir_get_generic_ops();
    //pthread_rwlockattr_init(&sb.w_lock);
    pthread_mutex_init(&sb->sb_wlock, NULL);
    /* Set the "global" FUSE operation table up */
    sb->sb_fs_oprs  = (struct fuse_operations) {
        .init       = vfbfs_fo_init,
        .destroy    = vfbfs_fo_destroy,

        .open       = vfbfs_fo_open,
        .read       = vfbfs_fo_read,
        .write      = vfbfs_fo_write,
        .truncate   = vfbfs_fo_truncate,
        .fsync      = vfbfs_fo_fsync,
        .release    = NULL,
        //.close      = vfbfs_fo_close, // noexist
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
    struct vfbfs_dir *root = vfbfs_dir_new(fs, strdup("/"));
    if (sb == NULL || root == NULL) {
        free(sb);
        free(root);
        return NULL;
    }
    fs->fs_superblock = sb;
    sb->sb_fs         = fs;
    sb->sb_root       = root;

    root->d_oprs          = sb->sb_ddir_oprs;
    root->d_dentry_oprs   = sb->sb_dentry_oprs;
    root->d_dfile_oprs    = sb->sb_dfile_oprs;
    root->d_ddir_oprs     = root->d_oprs;
    root->d_entry->e_oprs = sb->sb_dentry_oprs;
    root->d_superblock    = sb;
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
    struct vfbfs_file *readme, *empty;
    char *msg = strdup("This is a readme file!\n");

    /* TODO --help */
    openlog("vfbfs", LOG_CONS|LOG_PID, LOG_USER);
    vfbfs_init(&fs);

    fb = vfbfs_dir_create_in(&fs, NULL, "fb");
    config = vfbfs_dir_create_in(&fs, NULL, "config");
    readme = vfbfs_file_create_in(&fs, config, "readme.txt");
    empty  = vfbfs_file_create_in(&fs, config, "empty.txt");
    readme->f_content = msg;
    vfbfs_file_set_size(readme, strlen(msg));

    return vfbfs_main(&fs, argc, argv);
}
