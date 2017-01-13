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
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

inline int vfbfs_file_cmp(struct vfbfs_file *f1, struct vfbfs_file *f2)
{
    return strcmp(f1->vf_name, f2->vf_name);
}

RB_GENERATE(VFBFS_FILE_TREE, vfbfs_file, vf_entry, vfbfs_file_cmp);

int vfbfs_gen_file_getattr(struct vfbfs *fs, struct vfbfs_file *f, const char *path, struct stat *st)
{
    *st = f->vf_stat;
    return 0;
}

int vfbfs_gen_file_open(struct vfbfs *fs, struct vfbfs_file *f, const char *path, struct fuse_file_info *in)
{
    (void) in;
    pthread_mutex_lock(&f->vf_lock);
    f->vf_open_count++;
    pthread_mutex_unlock(&f->vf_lock);
    return 0;
}

int vfbfs_gen_file_close(struct vfbfs *fs, struct vfbfs_file *f, const char *path, struct fuse_file_info *in)
{
    (void) in;
    pthread_mutex_lock(&f->vf_lock);
    f->vf_open_count--;
    pthread_mutex_unlock(&f->vf_lock);
    return 0;
}

int vfbfs_gen_file_read(struct vfbfs *fs, struct vfbfs_file *file, const char *path
    , char *data, size_t size, off_t off, struct fuse_file_info *fi)
{
    pthread_mutex_lock(&file->vf_lock);
    off_t fsize = file->vf_stat.st_size;
    if (file->vf_private != NULL) {
        if (off + size >= fsize) {
            size = fsize;
        }
        memcpy(data, file->vf_private+off, size);
        return size;
    } else {
        size = 0;
    }
    pthread_mutex_unlock(&file->vf_lock);
    return size;
}

int vfbfs_gen_file_write(struct vfbfs *fs, struct vfbfs_file *file, const char *path
    , const char *data, size_t size, off_t off, struct fuse_file_info *fi)
{
    pthread_mutex_lock(&file->vf_lock);
    off_t fsize = file->vf_stat.st_size;
    if (file->vf_private == NULL) {
        file->vf_private = calloc(size+off, sizeof(char));
        fsize = off + size;
    } else {
        if (off + size >= fsize) {
            file->vf_private = realloc(file->vf_private, off+size);
            fsize = off+size;
        }
    }
    memcpy(file->vf_private + off, data, size);
    file->vf_stat.st_size = fsize;
    pthread_mutex_unlock(&file->vf_lock);
    return 0;
}

int vfbfs_gen_file_release(struct vfbfs *fs, struct vfbfs_file *file
    , const char *path, struct fuse_file_info *fi)
{
    //pthread_mutex_lock(&file->vf_lock);
    //vfbfs_file_free(fs, file);
    /* No unlock, the mutex is already destroyed by now */
    return 0;
}

struct vfbfs_file_ops vfbfs_gen_file_oprs = {
    .f_open       = vfbfs_gen_file_open,
    .f_close      = vfbfs_gen_file_close,
    .f_read       = vfbfs_gen_file_read,
    .f_write      = vfbfs_gen_file_write,
    .f_getattr    = vfbfs_gen_file_getattr,
    .f_release    = vfbfs_gen_file_release,
};

void vfbfs_file_init(struct vfbfs_file *f)
{
    /* vf_oprs will be set later, by the parent directory (vd_def_oprs field) */
    // f->vf_oprs = &vfbfs_gen_file_oprs;
    pthread_mutex_init(&f->vf_lock, NULL);
    f->vf_private = f->vf_name = NULL;
    f->vf_open_count = 0;
    f->vf_parent = NULL;
    memset(&f->vf_stat, 0, sizeof(struct stat));
    f->vf_stat.st_mode = 0444;
    f->vf_stat.st_uid  = getuid();
    f->vf_stat.st_gid  = getgid();
    f->vf_stat.st_ctime = f->vf_stat.st_atime = f->vf_stat.st_mtime = time(NULL);
}

struct vfbfs_file *vfbfs_file_alloc(struct vfbfs *fs)
{
    (void) fs;
    struct vfbfs_file *f = (struct vfbfs_file *)malloc(sizeof(struct vfbfs_file));
    vfbfs_file_init(f);
    return f;
}

void vfbfs_file_free(struct vfbfs *fs, struct vfbfs_file *file)
{

}

struct vfbfs_file *vfbfs_file_new(struct vfbfs *fs, char *name)
{
    (void) fs;
    struct vfbfs_file *f = vfbfs_file_alloc(fs);
    if (f == NULL) {
        return NULL;
    }
    f->vf_name = name;
    return f;
}

struct vfbfs_file *vfbfs_file_add_to(struct vfbfs *fs, struct vfbfs_dir *d, struct vfbfs_file *f)
{
    struct vfbfs_superblock *sb = fs->fs_superblock;
    if (d == NULL) {
        d = fs->fs_superblock->sb_root;
    }
    pthread_rwlock_wrlock(&d->vd_rwlock);
    RB_INSERT(VFBFS_FILE_TREE, &d->vd_files, f);
    pthread_rwlock_unlock(&d->vd_rwlock);

    pthread_mutex_lock(&f->vf_lock);
    f->vf_parent = d;
    /* Use the directory's default file operations if none is set */
    if (f->vf_oprs == NULL) {
        f->vf_oprs   = d->vd_def_oprs;
    }
    pthread_mutex_unlock(&f->vf_lock);

    pthread_mutex_lock(&sb->sb_wlock);
    sb->sb_file_count++;
    pthread_mutex_unlock(&sb->sb_wlock);
    return f;
}

struct vfbfs_file *vfbfs_file_find_in(struct vfbfs *fs, struct vfbfs_dir *dir, const char *name)
{
    (void) fs;
    struct vfbfs_file f, *file;
    f.vf_name = (char *) name;

    pthread_rwlock_rdlock(&dir->vd_rwlock);
    file = RB_FIND(VFBFS_FILE_TREE, &dir->vd_files, &f);
    pthread_rwlock_unlock(&dir->vd_rwlock);
    return file;
}

bool vfbfs_file_is_dir(struct vfbfs_file *f)
{
    return f && (f->vf_stat.st_mode & S_IFDIR);
}

struct vfbfs_file *
vfbfs_file_create_in(struct vfbfs *fs, struct vfbfs_dir *parent, const char *fname)
{
    if (fs == NULL || fname == NULL) {
        return NULL;
    }
    struct vfbfs_file *f = vfbfs_file_new(fs, strdup(fname));
    f->vf_stat.st_mode |= S_IFREG;
    return vfbfs_file_add_to(fs, parent, f);
}

struct vfbfs_file *vfbfs_lookup(struct vfbfs *fs, const char *path)
{
    struct vfbfs_superblock *sb  = fs->fs_superblock;
    struct vfbfs_dir        *dir = sb->sb_root;

    if (path[0] == '/' && path[1] == '\0') {
        return dir->vd_file;
    }

    char *pptr = strdup(path);
    char *svptr;
    char *entry;
    struct vfbfs_file *file = NULL;
    while ((entry = strtok_r(pptr, "/", &svptr))) {
        file = vfbfs_file_find_in(fs, dir, entry);
        if (file == NULL) {
            return NULL;
        } else {
            if (vfbfs_file_is_dir(file)) {
                dir = (struct vfbfs_dir *)file->vf_private;
            } else {
                return file;
            }
        }
        pptr = NULL;
    }
    // todo: free pptr
    return file;
}