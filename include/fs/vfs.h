#ifndef ARCLINE_VFS_H
#define ARCLINE_VFS_H

#include <kernel/types.h>
#include <stddef.h>

struct file;
struct inode;

// File operations structure
struct file_operations {
    ssize_t (*read)(struct file *file, char *buf, size_t count, loff_t *offset);
    ssize_t (*write)(struct file *file, const char *buf, size_t count, loff_t *offset);
    int (*open)(struct inode *inode, struct file *file);
    int (*close)(struct inode *inode, struct file *file);
    // Add other operations as needed (e.g., ioctl, lseek, etc.)
};

// Inode structure
struct inode {
    unsigned long i_ino;            // Inode number
    mode_t i_mode;                  // File type and permissions
    struct file_operations *i_fop;  // File operations for this inode
    void *i_private;                // Private data for the filesystem/device
    // Add other inode fields as needed (e.g., i_rdev for device files)
};

// File structure (open file description)
struct file {
    loff_t f_pos;                   // Current file offset
    struct inode *f_inode;          // Pointer to the inode
    struct file_operations *f_op;   // Pointer to the file operations
    void *private_data;             // Private data for the opened file
    // Add other file fields as needed (e.g., flags, etc.)
};

// Function to initialize the VFS
void vfs_init(void);

// Function to register a character device
int register_chrdev(const char *name, struct file_operations *fops);

// Function to open a file/device
struct file *vfs_open(const char *path, int flags, mode_t mode);

// Function to read from a file/device
ssize_t vfs_read(struct file *file, char *buf, size_t count, loff_t *offset);

// Function to close a file/device
int vfs_close(struct file *file);

// Constants for file flags and modes (simplified for now)
#define O_RDONLY    0x0001
#define O_WRONLY    0x0002
#define O_RDWR      0x0003
#define O_CREAT     0x0100
#define O_TRUNC     0x0200
#define O_APPEND    0x0400

// File types (simplified)
#define S_IFREG     0x8000  // Regular file
#define S_IFDIR     0x4000  // Directory
#define S_IFCHR     0x2000  // Character device

typedef uint32_t mode_t;
typedef int32_t loff_t;
typedef int32_t ssize_t;


#endif // ARCLINE_VFS_H