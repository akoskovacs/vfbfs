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

#ifndef VFBFS_H
#define VFBFS_H

#define _GNU_SOURCE
#define FUSE_USE_VERSION 26
#include <fuse_opt.h>
#include <fuse.h>
#include <bsd/sys/tree.h>
#include <pthread.h>
#include <stdbool.h>
#include <syslog.h>

struct vfbfs;
struct vfbfs_file;
struct vfbfs_superblock;
struct vfbfs_dir;

struct vfbfs_dir_ops {
    int (*d_create)(struct vfbfs *, struct vfbfs_dir *, const char *, mode_t, struct fuse_file_info *);
    int (*d_open)(struct vfbfs *, struct vfbfs_dir *, const char *, struct fuse_file_info *);
    int (*d_close)(struct vfbfs *, struct vfbfs_dir *, const char *, struct fuse_file_info *);
    int (*d_read)(struct vfbfs *, struct vfbfs_dir *, const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info *);
    int (*d_getattr)(struct vfbfs *, struct vfbfs_dir *, const char *, struct stat *);
    int (*d_release)(struct vfbfs *, struct vfbfs_dir *, const char *, struct fuse_file_info *);
};

struct vfbfs_file_ops {
    int (*f_open)(struct vfbfs *, struct vfbfs_file *, const char *,  struct fuse_file_info *);
    int (*f_close)(struct vfbfs *, struct vfbfs_file *, const char *, struct fuse_file_info *);
    int (*f_read)(struct vfbfs *, struct vfbfs_file *, const char *, char *, size_t, off_t, struct fuse_file_info *);
    int (*f_write)(struct vfbfs *, struct vfbfs_file *, const char *, const char *, size_t, off_t, struct fuse_file_info *);
    int (*f_truncate)(struct vfbfs *, struct vfbfs_file *, const char *, off_t);
    int (*f_getattr)(struct vfbfs *, struct vfbfs_file *, const char *, struct stat *);
    int (*f_release)(struct vfbfs *, struct vfbfs_file *, const char *path, struct fuse_file_info *);
};

struct vfbfs_file {
    char                   *vf_name;        /* basename of the file or directory */
    struct stat             vf_stat;
    size_t                  vf_open_count;  /* currently open count */
    struct vfbfs_file_ops  *vf_oprs;        /* file operations on the file */
    struct vfbfs_dir       *vf_parent;      /* containing directory */
    RB_ENTRY(vfbfs_file)    vf_entry;       /* entry in the directory's Red-Black tree */
    pthread_mutex_t         vf_lock;        /* must held a lock to access file */
    void                   *vf_private;     /* private field (struct vfbfs_dir * if vf_mode & S_IFDIR) */
};

struct vfbfs_dir {
    struct vfbfs_file                   *vd_file;     /* file entry of this directory */
    struct vfbfs_dir_ops                *vd_oprs;     /* directory operations */
    pthread_rwlock_t                     vd_rwlock;   /* read-write lock for this structure */
    RB_HEAD(VFBFS_FILE_TREE, vfbfs_file) vd_files;    /* Red-Black tree of the entries of this directory */
    struct vfbfs_file_ops               *vd_def_oprs; /* default opreations for files in this dir */
    struct vfbfs_superblock             *vd_superblock; /* parent superblock */
    void                                *vd_private;  /* private data (if any) */
};

struct vfbfs_superblock {
    char                   *sb_mountpoint;  /* system moutpoint path */
    struct vfbfs_dir       *sb_root;        /* root directory */
    size_t                  sb_file_count;  /* full file count */
    pthread_mutex_t         sb_wlock;       /* write lock */
    struct vfbfs_file_ops  *sb_def_oprs;    /* default operations for new files */
    struct vfbfs           *sb_fs;          /* parent filesystem */
    struct fuse_operations  sb_fs_oprs;     /* FUSE basic operations */
};

struct vfbfs {
    struct vfbfs_superblock *fs_superblock; /* superblock of the filesystem */
    const char              *fs_abs_path;   /* absoulte working-directory path */
    pthread_key_t            fs_errno_key;  /* thread-local errno */
};

int vfbfs_file_cmp(struct vfbfs_file *, struct vfbfs_file *);

RB_PROTOTYPE(VFBFS_FILE_TREE, vfbfs_file, vf_entry, vfbfs_file_cmp);
struct vfbfs            *vfbfs_fs_alloc(void);
struct vfbfs            *vfbfs_init(struct vfbfs *fs);
struct vfbfs            *vfbfs_new(void);
int                      vfbfs_main(struct vfbfs *fs, int argc, char *argv[]);
struct vfbfs_file       *vfbfs_lookup(struct vfbfs *fs, const char *path);

struct vfbfs_dir        *vfbfs_get_rootdir(struct vfbfs *fs);
struct vfbfs            *vfbfs_get_fs(void);

struct vfbfs_superblock *vfbfs_superblock_alloc(struct vfbfs *fs);

bool                     vfbfs_file_is_dir(struct vfbfs_file *f);
struct vfbfs_file       *vfbfs_file_new(struct vfbfs *fs, char *name);
void                     vfbfs_file_init(struct vfbfs_file *f);
struct vfbfs_file       *vfbfs_file_alloc(struct vfbfs *fs);
struct vfbfs_file       *vfbfs_file_add_to(struct vfbfs *fs, struct vfbfs_dir *dir, struct vfbfs_file *f);
struct vfbfs_file       *vfbfs_file_create_in(struct vfbfs *fs, struct vfbfs_dir *parent, const char *fname);
void                     vfbfs_file_free(struct vfbfs *fs, struct vfbfs_file *f);

struct vfbfs_dir        *vfbfs_dir_new(struct vfbfs *fs, char *name);
struct vfbfs_dir        *vfbfs_dir_alloc(struct vfbfs *fs);
struct vfbfs_dir        *vfbfs_dir_add_to(struct vfbfs *fs, struct vfbfs_dir *parent, struct vfbfs_dir *d);
struct vfbfs_dir        *vfbfs_dir_create_in(struct vfbfs *fs, struct vfbfs_dir *parent, const char *dname);
#endif /* VFBFS_H */
