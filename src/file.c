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
#include <errno.h>
#include <stdarg.h>

void vfbfs_entry_init(struct vfbfs_entry *e)
{
    e->e_name      = NULL;
    e->e_oprs      = NULL;
    e->e_parent    = NULL;
    e->e_private   = NULL;
    e->e_elem.file = NULL;
    /* e_entry will be initialized on insert */
    pthread_mutex_init(&e->e_wlock, NULL);
    memset(&e->e_stat, 0, sizeof(struct stat));
}

void vfbfs_entry_init_generic(struct vfbfs_entry *e)
{
    e->e_stat.st_mode = 0444;
    e->e_stat.st_uid  = getuid();
    e->e_stat.st_gid  = getgid();
    e->e_stat.st_nlink = 1;
    e->e_stat.st_ctime = e->e_stat.st_atime 
            = e->e_stat.st_mtime = time(NULL);
}

struct vfbfs_entry *vfbfs_entry_alloc(struct vfbfs *fs)
{
    (void) fs;
    struct vfbfs_entry *e = (struct vfbfs_entry *)malloc(sizeof(*e));
    if (e != NULL) {
        vfbfs_entry_init(e);
    }
    return e;
}

struct vfbfs_entry *vfbfs_entry_file_alloc(struct vfbfs *fs)
{
    struct vfbfs_entry *e = vfbfs_entry_alloc(fs);
    if (e == NULL) {
        return NULL;
    }
    vfbfs_entry_init_generic(e);
    e->e_stat.st_mode |= S_IFREG;
    return e;
}

struct vfbfs_entry *vfbfs_entry_dir_alloc(struct vfbfs *fs)
{
    struct vfbfs_entry *e = vfbfs_entry_alloc(fs);
    if (e == NULL) {
        return NULL;
    }
    vfbfs_entry_init_generic(e);
    e->e_stat.st_mode |= S_IFDIR;
    e->e_stat.st_size = 4096;
    return e;
}

inline int vfbfs_entry_cmp(struct vfbfs_entry *e1, struct vfbfs_entry *e2)
{
    if (e1 != NULL && e2 != NULL) {
        return strcmp(e1->e_name, e2->e_name);
    } else {
        return e1 < e2;
    }
}

RB_GENERATE(VFBFS_ENTRY_TREE, vfbfs_entry, e_entry, vfbfs_entry_cmp);

off_t vfbfs_file_get_size(struct vfbfs_file *f)
{
    struct vfbfs_entry *fe = (f != NULL) ? f->f_entry : NULL;
    return (fe != NULL) ? fe->e_stat.st_size : 0;
}

off_t vfbfs_file_set_size(struct vfbfs_file *f, off_t new_size)
{
    struct vfbfs_entry *fe;
    if (f == NULL) {
        return 0;
    }
    fe = f->f_entry;
    off_t old_size = fe->e_stat.st_size;
    pthread_mutex_lock(&fe->e_wlock);
    fe->e_stat.st_size = new_size;
    pthread_mutex_unlock(&fe->e_wlock);
    return old_size;
}

bool vfbfs_entry_is_file(struct vfbfs_entry *e)
{
    if (e != NULL) {
        return (e->e_stat.st_mode & S_IFREG) && (e->e_elem.file != NULL);
    }
    return false;
}

bool vfbfs_entry_is_dir(struct vfbfs_entry *e)
{
    if (e != NULL) {
        return (e->e_stat.st_mode & S_IFDIR) && (e->e_elem.dir != NULL);
    }
    return false;
}

struct vfbfs_file *vfbfs_entry_get_file(struct vfbfs_entry *e)
{
    return vfbfs_entry_is_file(e) ? e->e_elem.file : NULL;
}

struct vfbfs_dir *vfbfs_entry_get_dir(struct vfbfs_entry *e)
{
    return vfbfs_entry_is_dir(e) ? e->e_elem.dir : NULL;
}

/*
 * The getattr() system call is used the same way on both files and directories,
 * altrough vfbfs_dir and vfbfs_file could have its own getattr().
*/
 int vfbfs_mem_entry_getattr(struct vfbfs *fs, struct vfbfs_entry *e, const char *path, struct stat *st)
{
    #if 0
    struct vfbfs_file *f;
    struct vfbfs_dir *d;
    if (vfbfs_entry_is_file(e)) {
        f = e->e_elem.file;
        if (f != NULL && f->f_oprs != NULL && f->f_oprs->f_getattr != NULL) {
            return f->f_oprs->f_getattr(fs, f, path, st);
        }
    } else {
        d = e->e_elem.dir;
        if (d != NULL && d->d_oprs != NULL && d->d_oprs->d_getattr != NULL) {
            return d->d_oprs->d_getattr(fs, d, path, st);
        }
    }
    #endif
    *st = e->e_stat;
    return 0;
}

int vfbfs_mem_entry_is_capable(struct vfbfs *fs, struct vfbfs_entry *ent, const char *path, uint32_t flags)
{
    #if 0
    uid_t ui = getuid();
    gid_t oi = getgid();
    if (flags & O_RDWR) {
    } else if (flags & O_CREAT) {
    } else if (flags & O_RDWR) {
    }
    #endif
    return 0;
}

int vfbfs_mem_file_open(struct vfbfs *fs, struct vfbfs_file *f, const char *path, struct fuse_file_info *fi)
{
    int cap;
    struct vfbfs_entry *e = f->f_entry;
    if (e != NULL && e->e_oprs != NULL && e->e_oprs->e_is_capable != NULL) {
        cap = e->e_oprs->e_is_capable(fs, e, path, fi->flags);
        if (cap != 0) {
            return cap;
        }
    }
    pthread_mutex_lock(&f->f_lock);
    f->f_open_count++;
    pthread_mutex_unlock(&f->f_lock);
    return 0;
}

int vfbfs_mem_file_close(struct vfbfs *fs, struct vfbfs_file *f, const char *path, struct fuse_file_info *in)
{
    (void) in;
    pthread_mutex_lock(&f->f_lock);
    f->f_open_count--;
    pthread_mutex_unlock(&f->f_lock);
    return 0;
}

int vfbfs_mem_file_read(struct vfbfs *fs, struct vfbfs_file *file, const char *path
    , char *data, size_t size, off_t off, struct fuse_file_info *fi)
{
    pthread_mutex_lock(&file->f_lock);
    off_t fsize = vfbfs_file_get_size(file);
    if (file->f_content != NULL) {
        if (off + size >= fsize) {
            size = fsize;
        }
        memcpy(data, file->f_content+off, size);
        return size;
    } else {
        size = 0;
    }
    pthread_mutex_unlock(&file->f_lock);
    return size;
}

int vfbfs_mem_file_truncate(struct vfbfs *fs, struct vfbfs_file *file, const char *path, off_t size)
{
    pthread_mutex_lock(&file->f_lock);
    if (file->f_content != NULL) {
        free(file->f_content);
    }
    file->f_content = calloc(size, sizeof(char));
    pthread_mutex_unlock(&file->f_lock);
    if (file->f_content == NULL) {
        vfbfs_file_set_size(file, 0);
        return -ENOSPC;
    }
    // this function locks file->f_lock by itself
    vfbfs_file_set_size(file, size);
    return 0;
}

int vfbfs_mem_file_write(struct vfbfs *fs, struct vfbfs_file *file, const char *path
    , const char *data, size_t size, off_t off, struct fuse_file_info *fi)
{
    pthread_mutex_lock(&file->f_lock);
    off_t fsize = vfbfs_file_get_size(file);
    off_t nsize = fsize;
    void *nptr = NULL;
    if (file->f_content != NULL) {
        if (off + size >= fsize) {
            nptr = realloc(file->f_content, off+size);
            if (nptr == NULL) {
                return -ENOSPC;
            }
            file->f_content = nptr;
            nsize = off+size;
        }
    }
    memcpy(file->f_content + off, data, size);
    pthread_mutex_unlock(&file->f_lock);
    if (fsize != nsize) {
        vfbfs_file_set_size(file, nsize);
    }
    return 0;
}

int vfbfs_mem_file_release(struct vfbfs *fs, struct vfbfs_file *file
    , const char *path, struct fuse_file_info *fi)
{
    //pthread_mutex_lock(&file->vf_lock);
    //vfbfs_file_free(fs, file);
    /* No unlock, the mutex is already destroyed by now */
    return 0;
}

static struct vfbfs_file_ops vfbfs_mem_file_oprs = {
    .f_open       = vfbfs_mem_file_open,
    .f_close      = vfbfs_mem_file_close,
    .f_read       = vfbfs_mem_file_read,
    .f_write      = vfbfs_mem_file_write,
    .f_truncate   = vfbfs_mem_file_truncate,
    .f_getattr    = NULL,
    .f_release    = vfbfs_mem_file_release,
};

struct vfbfs_file_ops *vfbfs_file_get_mem_ops(void)
{
    return &vfbfs_mem_file_oprs;
}

struct vfbfs_entry_ops vfbfs_mem_entry_oprs = {
    .e_getattr    = vfbfs_mem_entry_getattr,
    .e_release    = NULL,
    .e_is_capable = vfbfs_mem_entry_is_capable,
};

struct vfbfs_entry_ops *vfbfs_entry_get_mem_ops(void)
{
    return &vfbfs_mem_entry_oprs;
}

void vfbfs_file_init(struct vfbfs_file *f)
{
    f->f_entry = NULL;
    f->f_oprs  = NULL;
    f->f_open_count  = 0;
    pthread_mutex_init(&f->f_lock, NULL);
    f->f_content = NULL;
    f->f_private = NULL;
}

struct vfbfs_file *vfbfs_file_alloc(struct vfbfs *fs)
{
    (void) fs;
    struct vfbfs_file *f = (struct vfbfs_file *)malloc(sizeof(*f));
    if (f != NULL) {
        vfbfs_file_init(f);
    }
    return f;
}

void vfbfs_file_free(struct vfbfs *fs, struct vfbfs_file *file)
{

}

struct vfbfs_file *vfbfs_file_new(struct vfbfs *fs, char *name)
{
    (void) fs;
    struct vfbfs_file *f = vfbfs_file_alloc(fs);
    struct vfbfs_entry *e = vfbfs_entry_file_alloc(fs);
    if (f == NULL || e == NULL) {
        free(e);
        free(f);
        return NULL;
    }
    e->e_name = name;

    e->e_elem.file = f;
    f->f_entry = e;
    return f;
}

void vfbfs_file_inherit(struct vfbfs_dir *d, struct vfbfs_file *f)
{
    struct vfbfs_entry *fe = f->f_entry;
    pthread_mutex_lock(&fe->e_wlock);
    fe->e_parent = d;
    /* Use the directory's default file operations if none is set */
    if (fe->e_oprs == NULL) {
        fe->e_oprs   = d->d_dentry_oprs;
    }
    pthread_mutex_unlock(&fe->e_wlock);

    pthread_mutex_lock(&f->f_lock);
    if (f->f_oprs == NULL) {
        f->f_oprs = d->d_dfile_oprs;
    }
    pthread_mutex_unlock(&f->f_lock);
}

int vfbfs_entry_add_to(struct vfbfs *fs, struct vfbfs_dir *parent, struct vfbfs_entry *e)
{
    if (parent == NULL || e == NULL) {
        return -ENOENT;
    }
    pthread_rwlock_wrlock(&parent->d_rwlock);
    RB_INSERT(VFBFS_ENTRY_TREE, &parent->d_entries, e);
    pthread_rwlock_unlock(&parent->d_rwlock);
    return 0;
}

struct vfbfs_file *vfbfs_file_add_to(struct vfbfs *fs, struct vfbfs_dir *d, struct vfbfs_file *f)
{
    struct vfbfs_superblock *sb = (fs != NULL) ? fs->fs_superblock : NULL;
    struct vfbfs_entry *fe = (f != NULL) ? f->f_entry : NULL;
    if (sb == NULL || fe == NULL) {
        return NULL;
    }

    if (d == NULL) {
        d = fs->fs_superblock->sb_root;
    }

    if (vfbfs_entry_add_to(fs, d, fe) != 0) {
        return NULL;
    }

    vfbfs_file_inherit(d, f);

    pthread_mutex_lock(&sb->sb_wlock);
    sb->sb_file_count++;
    pthread_mutex_unlock(&sb->sb_wlock);
    return f;
}

struct vfbfs_entry *vfbfs_entry_find_in(struct vfbfs *fs, struct vfbfs_dir *d, const char *name)
{
    struct vfbfs_entry ent, *re; 
    ent.e_name = (char *)name;
    pthread_rwlock_rdlock(&d->d_rwlock);
    re = RB_FIND(VFBFS_ENTRY_TREE, &d->d_entries, &ent);
    pthread_rwlock_unlock(&d->d_rwlock);
    return re;
}

struct vfbfs_file *vfbfs_file_find_in(struct vfbfs *fs, struct vfbfs_dir *dir, const char *name)
{
    struct vfbfs_entry *e = vfbfs_entry_find_in(fs, dir, name);
    return (e != NULL) ? e->e_elem.file : NULL;
}

struct vfbfs_file *
vfbfs_file_create_in(struct vfbfs *fs, struct vfbfs_dir *parent, const char *fname)
{
    if (fs == NULL || fname == NULL) {
        return NULL;
    }
    struct vfbfs_file *f = vfbfs_file_new(fs, strdup(fname));
    return vfbfs_file_add_to(fs, parent, f);
}

struct vfbfs_entry *vfbfs_entry_lookup(struct vfbfs *fs, const char *path)
{
    struct vfbfs_superblock *sb  = fs->fs_superblock;
    struct vfbfs_dir        *dir = sb->sb_root;

    if (path[0] == '/' && path[1] == '\0') {
        return dir->d_entry;
    }

    char *apath = strdup(path);
    char *pptr = apath;
    char *svptr, *entry;
    struct vfbfs_entry *e;

    if (apath == NULL) {
        return NULL;
    }

    while ((entry = strtok_r(pptr, "/", &svptr))) {
        e = vfbfs_entry_find_in(fs, dir, entry);
        if (e == NULL) {
            return NULL;
        } else {
            if (vfbfs_entry_is_dir(e)) {
                dir = e->e_elem.dir;
            }
        }
        pptr = NULL;
    }
    /* FIXME: should check the last entry's name? */
    free(apath);
    return e;
}

struct vfbfs_file *vfbfs_file_lookup(struct vfbfs *fs, const char *path)
{
    struct vfbfs_entry *e = vfbfs_entry_lookup(fs, path);
    return vfbfs_entry_is_dir(e) ? NULL : e->e_elem.file;
}

int vfbfs_file_call_operation_va_with(struct vfbfs *fs, struct vfbfs_file *file
                    , struct vfbfs_file_ops *oprs, enum VfbfsFileOperation op, va_list ap)
{
    struct fuse_file_info *fi;
    struct vfbfs_entry *e;
    const char *path;
    if (file == NULL) {
        return -ENOENT;
    }
    if (oprs == NULL) {
        if (file->f_oprs == NULL) {
            return -ENOSYS;
        }
        oprs = file->f_oprs;
    }
    e    = file->f_entry;
    path = va_arg(ap, const char *);
    switch (op) {
        case VFBFS_F_OPEN:
        if (oprs->f_open != NULL) {
            fi = va_arg(ap, struct fuse_file_info *);
            if (fi != NULL) {
                fi->fh = (uint64_t)e;
            }
            return oprs->f_open(fs, file, path, fi);
        }
        break;

        case VFBFS_F_CLOSE:
        if (oprs->f_close != NULL) {
            return oprs->f_close(fs, file, path, va_arg(ap, struct fuse_file_info *));
        }
        break;

        case VFBFS_F_READ:
        if (oprs->f_read != NULL) {
            return oprs->f_read(fs, file, path, va_arg(ap, char *)
                , va_arg(ap, size_t), va_arg(ap, off_t), va_arg(ap, struct fuse_file_info *));
        }
        break;

        case VFBFS_F_WRITE:
        if (oprs->f_write != NULL) {
            return oprs->f_write(fs, file, path, va_arg(ap, const char *)
                , va_arg(ap, size_t), va_arg(ap, off_t), va_arg(ap, struct fuse_file_info *));
        }
        break;

        case VFBFS_F_TRUNCATE:
        if (oprs->f_truncate != NULL) {
            return oprs->f_truncate(fs, file, path, va_arg(ap, off_t));
        }
        break;

        case VFBFS_F_GETATTR:
        if (oprs->f_getattr != NULL) {
            return oprs->f_getattr(fs, file, path, va_arg(ap, struct stat *));
        } else {
            if (e != NULL && e->e_oprs != NULL && e->e_oprs->e_getattr) {
                return e->e_oprs->e_getattr(fs, e, path, va_arg(ap, struct stat *));
            }
        }
        break;

        case VFBFS_F_RELEASE:
        if (oprs->f_release != NULL) {
            return oprs->f_release(fs, file, path, va_arg(ap, struct fuse_file_info *));
        } else {
            if (e != NULL && e->e_oprs != NULL && e->e_oprs->e_release) {
                return e->e_oprs->e_release(fs, e, path, va_arg(ap, struct fuse_file_info *));
            }
        }
        break;
    }
    return 0;
}

int vfbfs_file_call_operation_with(struct vfbfs *fs, struct vfbfs_file *file
                    , struct vfbfs_file_ops *oprs, enum VfbfsFileOperation op, ...)
{
    int r;
    va_list ap;
    va_start(ap, op);
    r = vfbfs_file_call_operation_va_with(fs, file, oprs, op, ap);
    va_end(ap);
    return r;
}

int vfbfs_file_call_operation(struct vfbfs *fs, struct vfbfs_file *file, enum VfbfsFileOperation op, ...)
{
    int r;
    va_list ap;
    va_start(ap, op);
    r = vfbfs_file_call_operation_va_with(fs, file, NULL, op, ap);
    va_end(ap);
    return r;
}
