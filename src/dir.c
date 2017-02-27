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

#include <stdlib.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <stdarg.h>

extern struct vfbfs_file_ops vfbfs_gen_file_oprs;

int vfbfs_gen_dir_open(struct vfbfs *fs, struct vfbfs_dir *d, const char *path, struct fuse_file_info *fi)
{
    return 0;    
}

int vfbfs_gen_dir_read(struct vfbfs *fs, struct vfbfs_dir *dir
    , const char *path, void *buf, fuse_fill_dir_t filler, off_t off, struct fuse_file_info *fi)
{
    pthread_rwlock_rdlock(&dir->d_rwlock);
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    struct vfbfs_entry *e;
    RB_FOREACH(e, VFBFS_ENTRY_TREE, &dir->d_entries) {
        filler(buf, e->e_name, NULL, 0);
    }
    pthread_rwlock_unlock(&dir->d_rwlock);
    return 0;
}

struct vfbfs_dir *vfbfs_get_rootdir(struct vfbfs *fs)
{
    if (fs != NULL && fs->fs_superblock != NULL && fs->fs_superblock->sb_root != NULL) {
        return fs->fs_superblock->sb_root;
    }
    return NULL;
}

struct vfbfs_dir *vfbfs_dir_init(struct vfbfs_dir *d)
{
    d->d_ddir_oprs   = NULL;
    d->d_dentry_oprs = NULL;
    d->d_dfile_oprs  = NULL;
    d->d_private     = NULL;
    d->d_superblock  = NULL;
    d->d_entry       = NULL;
    RB_INIT(&d->d_entries);
    pthread_rwlock_init(&d->d_rwlock, NULL);
    return d;
}

struct vfbfs_dir *vfbfs_dir_alloc(struct vfbfs *fs)
{
    (void) fs;
    struct vfbfs_dir *d = (struct vfbfs_dir *)malloc(sizeof(*d));
    if (d == NULL) {
        return NULL;
    }
    return vfbfs_dir_init(d);
}

static struct vfbfs_dir_ops vfbfs_dir_gen_oprs = {
    .d_read     = vfbfs_gen_dir_read,
    .d_open     = vfbfs_gen_dir_open,
    .d_close    = NULL,
    .d_getattr  = NULL,
    .d_create   = NULL,
};

struct vfbfs_dir_ops *vfbfs_dir_get_generic_ops(void)
{
    return &vfbfs_dir_gen_oprs;
}

struct vfbfs_dir *vfbfs_dir_new(struct vfbfs *fs, char *name)
{
    struct vfbfs_entry *e = vfbfs_entry_dir_alloc(fs);
    struct vfbfs_dir *d  = vfbfs_dir_alloc(fs);
    if (e == NULL || d == NULL) {
        free(e);
        free(d);
        return NULL;
    }
    e->e_name = name;
    e->e_elem.dir = d;
    d->d_entry    = e;

    if (fs != NULL && fs->fs_superblock != NULL) {
        d->d_dentry_oprs = fs->fs_superblock->sb_dentry_oprs;
        d->d_dfile_oprs  = fs->fs_superblock->sb_dfile_oprs;
        d->d_ddir_oprs   = fs->fs_superblock->sb_ddir_oprs;
        d->d_superblock  = fs->fs_superblock;
    }
    return d;
}

struct vfbfs_dir *
vfbfs_dir_add_to(struct vfbfs *fs, struct vfbfs_dir *parent, struct vfbfs_dir *d)
{
    struct vfbfs_entry *e;

    if (fs != NULL) {
        e = d->d_entry;
        if (parent == NULL) {
            /* Add to '/' if the parent == NULL */
            parent = fs->fs_superblock->sb_root;
        }
        if (vfbfs_entry_add_to(fs, parent, e) != 0) {
            return NULL;
        }
        /* Inherit everything from the parent, (overwriting superblock inheritance) */
        d->d_dentry_oprs = parent->d_dentry_oprs;
        d->d_dfile_oprs  = parent->d_dfile_oprs;
        d->d_ddir_oprs   = parent->d_ddir_oprs;
        d->d_superblock  = parent->d_superblock;
    }
    return d;
}

struct vfbfs_dir *
vfbfs_dir_create_in(struct vfbfs *fs, struct vfbfs_dir *parent, const char *dname)
{
    if (fs == NULL || dname == NULL) {
        return NULL;
    }
    struct vfbfs_dir *d = vfbfs_dir_new(fs, strdup(dname));
    return vfbfs_dir_add_to(fs, parent, d);
}

int vfbfs_dir_call_operation_va_with(struct vfbfs *fs, struct vfbfs_dir *dir
                    , struct vfbfs_dir_ops *oprs, enum VfbfsDirOperation op, va_list ap)
{
    struct fuse_file_info *fi;
    struct vfbfs_entry *e;
    const char *path;
    if (dir == NULL) {
        return -ENOENT;
    }
    if (oprs == NULL) {
        if (dir->d_oprs == NULL) {
            return -ENOSYS;
        }
        oprs = dir->d_oprs;
    }
    e    = dir->d_entry;
    path = va_arg(ap, const char *);
    switch (op) {
        case VFBFS_D_CREATE:
        if (oprs->d_create != NULL) {
            return oprs->d_create(fs, dir, path, va_arg(ap, mode_t)
                , va_arg(ap, struct fuse_file_info *));
        }
        break;

        case VFBFS_D_OPEN:
        if (oprs->d_open != NULL) {
            fi = va_arg(ap, struct fuse_file_info *);
            if (fi != NULL) {
                fi->fh = (uint64_t)e;
            }
            return oprs->d_open(fs, dir, path, fi);
        }
        break;

        case VFBFS_D_CLOSE:
        if (oprs->d_close != NULL) {
            return oprs->d_close(fs, dir, path, va_arg(ap, struct fuse_file_info *));
        }
        break;

        case VFBFS_D_READ:
        if (oprs->d_read != NULL) {
            return oprs->d_read(fs, dir, path, va_arg(ap, void *), va_arg(ap, fuse_fill_dir_t)
                , va_arg(ap, off_t), va_arg(ap, struct fuse_file_info *));
        }
        break;

        case VFBFS_D_GETATTR:
        if (oprs->d_getattr != NULL) {
            return oprs->d_getattr(fs, dir, path, va_arg(ap, struct stat *));
        } else {
            if (e != NULL && e->e_oprs != NULL && e->e_oprs->e_getattr != NULL) {
                return e->e_oprs->e_getattr(fs, e, path, va_arg(ap, struct stat *));
            }
        }
        break;

        case VFBFS_D_RELEASE:
        /* FIXME: ??? */
        if (oprs->d_release != NULL) {
            return oprs->d_release(fs, dir, path, va_arg(ap, struct fuse_file_info *));
        } else {
            if (e != NULL && e->e_oprs != NULL && e->e_oprs->e_release != NULL) {
                return e->e_oprs->e_release(fs, e, path, va_arg(ap, struct fuse_file_info *));
            }
        }
        break;
    }
    return 0;
}

int vfbfs_dir_call_operation_with(struct vfbfs *fs, struct vfbfs_dir *dir
                    , struct vfbfs_dir_ops *oprs, enum VfbfsDirOperation op, ...)
{
    int r;
    va_list ap;
    va_start(ap, op);
    r = vfbfs_file_call_operation_va_with(fs, dir, oprs, op, ap);
    va_end(ap);
    return r;
}

int vfbfs_dir_call_operation(struct vfbfs *fs, struct vfbfs_dir *dir
                    , enum VfbfsDirOperation op, ...)
{
    int r;
    va_list ap;
    va_start(ap, op);
    r = vfbfs_dir_call_operation_va_with(fs, dir, NULL, op, ap);
    va_end(ap);
    return r;
}