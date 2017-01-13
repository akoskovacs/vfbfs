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

extern struct vfbfs_file_ops vfbfs_gen_file_oprs;
extern int vfbfs_gen_file_getattr(struct vfbfs *, struct vfbfs_file *, const char *, struct stat *);

int vfbfs_gen_dir_getattr(struct vfbfs *fs, struct vfbfs_dir *d, const char *path, struct stat *st)
{
    return vfbfs_gen_file_getattr(fs, d->vd_file, path, st);
}

int vfbfs_gen_dir_open(struct vfbfs *fs, struct vfbfs_dir *d, const char *path, struct fuse_file_info *fi)
{
    return 0;    
}

int vfbfs_gen_dir_read(struct vfbfs *fs, struct vfbfs_dir *dir, const char *path, void *buf, fuse_fill_dir_t filler, off_t off, struct fuse_file_info *fi)
{
    pthread_rwlock_rdlock(&dir->vd_rwlock);
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    struct vfbfs_file *f;
    //syslog(LOG_NOTICE, "reading dir: %s", dir->vd_file->vf_name);
    RB_FOREACH(f, VFBFS_FILE_TREE, &dir->vd_files) {
        //syslog(LOG_NOTICE, "entry: %s", f->vf_name);
        filler(buf, f->vf_name, NULL, 0);
    }
    pthread_rwlock_unlock(&dir->vd_rwlock);
    return 0;
}

struct vfbfs_dir *vfbfs_get_rootdir(struct vfbfs *fs)
{
    if (fs != NULL && fs->fs_superblock != NULL && fs->fs_superblock->sb_root != NULL) {
        return fs->fs_superblock->sb_root;
    }
    return NULL;
}

struct vfbfs_dir *vfbfs_dir_alloc(struct vfbfs *fs)
{
    (void) fs;
    struct vfbfs_dir *d = (struct vfbfs_dir *)malloc(sizeof(struct vfbfs_dir));
    if (d == NULL) {
        return NULL;
    }
    d->vd_def_oprs = NULL;
    d->vd_private  = NULL;
    d->vd_superblock = NULL;
    d->vd_file  = NULL;
    RB_INIT(&d->vd_files);
    pthread_rwlock_init(&d->vd_rwlock, NULL);
    return d;
}

struct vfbfs_dir *vfbfs_dir_new(struct vfbfs *fs, char *name)
{
    struct vfbfs_file *f = vfbfs_file_new(fs, name);
    struct vfbfs_dir *d  = vfbfs_dir_alloc(fs);
    if (f == NULL || d == NULL) {
        return NULL;
    }
    if (fs != NULL && fs->fs_superblock != NULL) {
        d->vd_def_oprs = fs->fs_superblock->sb_def_oprs;
        d->vd_oprs = (struct vfbfs_dir_ops *)malloc(sizeof(struct vfbfs_dir_ops));
        #if 0
        d->vd_oprs = &(struct vfbfs_dir_ops) {
            .d_create   = NULL,
            .d_open     = vfbfs_gen_dir_open,
            .d_close    = NULL,
            .d_read     = vfbfs_gen_dir_read,
            .d_getattr  = vfbfs_gen_dir_getattr
        };
        #endif
        d->vd_oprs->d_create = NULL;
        d->vd_oprs->d_open   = vfbfs_gen_dir_open;
        d->vd_oprs->d_close  = NULL;
        d->vd_oprs->d_read   = vfbfs_gen_dir_read;
        d->vd_oprs->d_getattr= vfbfs_gen_dir_getattr;
        d->vd_superblock = fs->fs_superblock;
    }
    f->vf_name    = name;
    f->vf_stat.st_mode |= S_IFDIR;
    f->vf_stat.st_size = 4096;
    f->vf_private = (void *)d;
    d->vd_file    = f;
    return d;
}

struct vfbfs_dir *
vfbfs_dir_add_to(struct vfbfs *fs, struct vfbfs_dir *parent, struct vfbfs_dir *d)
{
    struct vfbfs_file *f = NULL;

    if (fs != NULL) {
        f = d->vd_file;
        if (parent == NULL) {
            /* Add to '/' if the parent == NULL */
            parent = fs->fs_superblock->sb_root;
        }
        d->vd_superblock = parent->vd_superblock;
        f = vfbfs_file_add_to(fs, parent, f);
    }
    return (struct vfbfs_dir *)f->vf_private;
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
