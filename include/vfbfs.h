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
struct vfbfs_entry;
struct vfbfs_file;
struct vfbfs_superblock;
struct vfbfs_dir;

struct vfbfs_entry_ops {
    int (*e_getattr)(struct vfbfs *, struct vfbfs_entry *, const char *, struct stat *);
    int (*e_release)(struct vfbfs *, struct vfbfs_entry *, const char *, struct fuse_file_info *);
    int (*e_is_capable)(struct vfbfs *, struct vfbfs_entry *, const char *, uint32_t flags);
};

enum VfbfsDirOperation {
      VFBFS_D_CREATE, VFBFS_D_OPEN, VFBFS_D_CLOSE, VFBFS_D_READ, VFBFS_D_GETATTR, VFBFS_D_RELEASE
};

struct vfbfs_dir_ops {
    int (*d_create)(struct vfbfs *, struct vfbfs_dir *, const char *, mode_t, struct fuse_file_info *);
    int (*d_open)(struct vfbfs *, struct vfbfs_dir *, const char *, struct fuse_file_info *);
    int (*d_close)(struct vfbfs *, struct vfbfs_dir *, const char *, struct fuse_file_info *);
    int (*d_read)(struct vfbfs *, struct vfbfs_dir *, const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info *);
    int (*d_getattr)(struct vfbfs *, struct vfbfs_dir *, const char *, struct stat *);
    int (*d_release)(struct vfbfs *, struct vfbfs_dir *, const char *, struct fuse_file_info *);
};

enum VfbfsFileOperation {
      VFBFS_F_OPEN, VFBFS_F_CLOSE, VFBFS_F_READ, VFBFS_F_WRITE
    , VFBFS_F_TRUNCATE, VFBFS_F_GETATTR, VFBFS_F_RELEASE
};

struct vfbfs_file_ops {
    int (*f_open)(struct vfbfs *, struct vfbfs_file *, const char *,  struct fuse_file_info *);
    int (*f_close)(struct vfbfs *, struct vfbfs_file *, const char *, struct fuse_file_info *);
    int (*f_read)(struct vfbfs *, struct vfbfs_file *, const char *, char *, size_t, off_t, struct fuse_file_info *);
    int (*f_write)(struct vfbfs *, struct vfbfs_file *, const char *, const char *, size_t, off_t, struct fuse_file_info *);
    int (*f_truncate)(struct vfbfs *, struct vfbfs_file *, const char *, off_t);
    int (*f_getattr)(struct vfbfs *, struct vfbfs_file *, const char *, struct stat *);
    int (*f_release)(struct vfbfs *, struct vfbfs_file *, const char *, struct fuse_file_info *);
};

/*
 * The vfbfs_entry structure holds the general information (size, permisson, etc)
 * about the file or directory entry in the parent directory.
 * If the entry is a directory e_stat.st_mode has the S_IFDIR bit set, it's a file otherwise.
 * To modify this structure e_wlock has to be acquired.
*/
struct vfbfs_entry {
    char                   *e_name;
    struct vfbfs_entry_ops *e_oprs;
    struct stat             e_stat;
    RB_ENTRY(vfbfs_entry)   e_entry;       /* entry in the directory's Red-Black tree */
    union {
        struct vfbfs_file  *file;          /* this is a file if e_stat.st_mode & S_IFREG */ 
        struct vfbfs_dir   *dir;           /* or e_stat.st_mode & S_IFDIR */
    } e_elem;
    pthread_mutex_t         e_wlock;       /* must held a lock to modify this entry */
    struct vfbfs_dir       *e_parent;      /* containing directory */
    void                   *e_private;     /* user-defined stuff   */
};

void vfbfs_entry_init(struct vfbfs_entry *ent);
void vfbfs_entry_init_generic(struct vfbfs_entry *ent);
int  vfbfs_entry_add_to(struct vfbfs *fs, struct vfbfs_dir *parent, struct vfbfs_entry *entry);
struct vfbfs_entry       *vfbfs_entry_alloc(struct vfbfs *fs);
struct vfbfs_entry       *vfbfs_entry_file_alloc(struct vfbfs *fs);
struct vfbfs_entry       *vfbfs_entry_dir_alloc(struct vfbfs *fs);
struct vfbfs_entry       *vfbfs_entry_lookup(struct vfbfs *fs, const char *path);
struct vfbfs_entry_ops   *vfbfs_entry_get_mem_ops(void);
struct vfbfs_file        *vfbfs_entry_get_file(struct vfbfs_entry *e);
struct vfbfs_dir         *vfbfs_entry_get_dir(struct vfbfs_entry *e);

/*
 * Holds information about regular files including it's contents (f_private)
 * and operations (f_oprs).
*/
struct vfbfs_file {
    struct vfbfs_entry     *f_entry;
    struct vfbfs_file_ops  *f_oprs;        /* file operations on the file */
    size_t                  f_open_count;  /* currently open count */
    pthread_mutex_t         f_lock;        /* must held a lock to access file */
    char                   *f_content;
    void                   *f_private;
};

bool                     vfbfs_file_is_dir(struct vfbfs_file *f);
struct vfbfs_file       *vfbfs_file_new(struct vfbfs *fs, char *name);
void                     vfbfs_file_init(struct vfbfs_file *f);
struct vfbfs_file       *vfbfs_file_alloc(struct vfbfs *fs);
off_t                    vfbfs_file_get_size(struct vfbfs_file *f);
off_t                    vfbfs_file_set_size(struct vfbfs_file *f, off_t new_size);
struct vfbfs_file       *vfbfs_file_add_to(struct vfbfs *fs, struct vfbfs_dir *dir, struct vfbfs_file *f);
struct vfbfs_file       *vfbfs_file_create_in(struct vfbfs *fs, struct vfbfs_dir *parent, const char *fname);
void                     vfbfs_file_free(struct vfbfs *fs, struct vfbfs_file *f);
struct vfbfs_file_ops   *vfbfs_file_get_mem_ops(void);
int                      vfbfs_file_call_operation_with(struct vfbfs *, struct vfbfs_file *, struct vfbfs_file_ops *, enum VfbfsFileOperation op, ...);
int                      vfbfs_file_call_operation(struct vfbfs *, struct vfbfs_file *, enum VfbfsFileOperation op, ...);

/*
 * Describes the directories in the filesystem. The entries are stored in a
 * Red-Black tree.
 * The newly added entries will inherit the appropriate default operations (d_dentry_oprs, d_dfile_oprs, d_ddir_oprs).
 * To read the 
*/
struct vfbfs_dir {
    struct vfbfs_entry                    *d_entry;       /* entry of this directory */
    struct vfbfs_dir_ops                  *d_oprs;        /* directory operations */
    pthread_rwlock_t                       d_rwlock;      /* read-write lock for this structure */
    RB_HEAD(VFBFS_ENTRY_TREE, vfbfs_entry) d_entries;     /* Red-Black tree of the entries of this directory */
    struct vfbfs_superblock               *d_superblock;  /* parent superblock */
    struct vfbfs_entry_ops                *d_dentry_oprs; /* default entry operation in this directory */ 
    struct vfbfs_file_ops                 *d_dfile_oprs;  /* default operations for files in this dir */
    struct vfbfs_dir_ops                  *d_ddir_oprs;   /* default operations for directories in this dir */
    void                                  *d_private;     /* private data (if any) */
};

struct vfbfs_dir        *vfbfs_dir_new(struct vfbfs *fs, char *name);
struct vfbfs_dir        *vfbfs_dir_alloc(struct vfbfs *fs);
struct vfbfs_dir        *vfbfs_dir_add_to(struct vfbfs *fs, struct vfbfs_dir *parent, struct vfbfs_dir *d);
struct vfbfs_dir        *vfbfs_dir_create_in(struct vfbfs *fs, struct vfbfs_dir *parent, const char *dname);
struct vfbfs_dir_ops    *vfbfs_dir_get_generic_ops(void);
int                      vfbfs_dir_call_operation_with(struct vfbfs *, struct vfbfs_dir *
                                    , struct vfbfs_dir_ops *, enum VfbfsDirOperation op, ...);
int                      vfbfs_dir_call_operation(struct vfbfs *, struct vfbfs_dir *, enum VfbfsDirOperation op, ...);

struct vfbfs_superblock {
    char                   *sb_mountpoint;  /* system moutpoint path */
    struct vfbfs_dir       *sb_root;        /* root directory */
    size_t                  sb_file_count;  /* full file count */
    pthread_mutex_t         sb_wlock;       /* write lock */
    struct vfbfs_entry_ops *sb_dentry_oprs; /* default operations for entries in this filesystem */
    struct vfbfs_file_ops  *sb_dfile_oprs;  /* default operations for files in this filesystem */
    struct vfbfs_dir_ops   *sb_ddir_oprs;   /* default operations for directories in this filesystem */
    struct vfbfs           *sb_fs;          /* parent filesystem */
    struct fuse_operations  sb_fs_oprs;     /* FUSE basic operations */
};

struct vfbfs {
    struct vfbfs_superblock *fs_superblock; /* superblock of the filesystem */
    const char              *fs_abs_path;   /* absoulte working-directory path */
    pthread_key_t            fs_errno_key;  /* thread-local errno */
};

int vfbfs_entry_cmp(struct vfbfs_entry *, struct vfbfs_entry *);

RB_PROTOTYPE(VFBFS_ENTRY_TREE, vfbfs_entry, e_entry, vfbfs_entry_cmp);
struct vfbfs            *vfbfs_fs_alloc(void);
struct vfbfs            *vfbfs_init(struct vfbfs *fs);
struct vfbfs            *vfbfs_new(void);
int                      vfbfs_main(struct vfbfs *fs, int argc, char *argv[]);
struct vfbfs_file       *vfbfs_lookup(struct vfbfs *fs, const char *path);

struct vfbfs_dir        *vfbfs_get_rootdir(struct vfbfs *fs);
struct vfbfs            *vfbfs_get_fs(void);

struct vfbfs_superblock *vfbfs_superblock_alloc(struct vfbfs *fs);
#endif /* VFBFS_H */
