// fs.cpp: File System

#include "sfs/fs.h"

#include <algorithm>

#include <assert.h>
#include <stdio.h>
#include <string.h>

// Debug file system -----------------------------------------------------------

void FileSystem::debug(Disk *disk) {
    Block superBlock;

    // Read Superblock
    disk->read(0, superBlock.Data);

    printf("SuperBlock:\n");
    printf("    magic number is %s\n", 
        superBlock.Super.MagicNumber == MAGIC_NUMBER ? "valid" : "invalid");
    printf("    %u blocks\n"         , superBlock.Super.Blocks);
    printf("    %u inode blocks\n"   , superBlock.Super.InodeBlocks);
    printf("    %u inodes\n"         , superBlock.Super.Inodes);

    // Read Inode blocks
    Block inodeBlock;
    // unsigned int inodeCount = 1;
    // loop over inode blocks
    for (unsigned int i = 0; i < superBlock.Super.InodeBlocks; i++) {
        // read in one inode block
        disk->read(i + 1, inodeBlock.Data);

        // loop over inodes in the block
        for (unsigned int j = 0; j < INODES_PER_BLOCK; j++) {
            Inode inode = inodeBlock.Inodes[j];

            // skip invalid inode
            if (!inode.Valid)
                continue;
            
            // loop over direct blocks
            std::string directBlocks = "";
            readArray(inode.Direct, POINTERS_PER_INODE, &directBlocks);
            printf("Inode %u:\n", j);
            printf("    size: %u bytes\n", inode.Size);
            printf("    direct blocks:%s\n", directBlocks.c_str());

            // skip invalid indirect node
            if (!inode.Indirect)
                continue;
            
            // loop over indirect blocks
            Block indirect;
            disk->read(inode.Indirect, indirect.Data);
            std::string indirectBlocks = "";
            readArray(indirect.Pointers, POINTERS_PER_BLOCK, &indirectBlocks);
            printf("    indirect block: %u\n", inode.Indirect);
            printf("    indirect data blocks:%s\n", indirectBlocks.c_str());
        }
    }
}

// Format file system ----------------------------------------------------------

bool FileSystem::format(Disk *disk) {
    // return false if mounted
    if (disk->mounted())
        return false;

    // Write superblock
    Block superBlock;
    superBlock.Super.MagicNumber = MAGIC_NUMBER;
    superBlock.Super.Blocks = disk->size();
    superBlock.Super.InodeBlocks = 0.1 * disk->size() + 0.5; // 0.5 for rounding
    superBlock.Super.Inodes = superBlock.Super.InodeBlocks * INODES_PER_BLOCK;
    disk->write(0, superBlock.Data);

    // Clear all other blocks
    Block empty;
    memset(empty.Data, 0, disk->BLOCK_SIZE);
    for (unsigned int i = 1; i < superBlock.Super.Blocks; i++) {
        disk->write(i, empty.Data);
    }

    return true;
}

// Mount file system -----------------------------------------------------------

bool FileSystem::mount(Disk *disk) {
    // Read superblock

    // Set device and mount

    // Copy metadata

    // Allocate free block bitmap

    return true;
}

// Create inode ----------------------------------------------------------------

ssize_t FileSystem::create() {
    // Locate free inode in inode table

    // Record inode if found
    return 0;
}

// Remove inode ----------------------------------------------------------------

bool FileSystem::remove(size_t inumber) {
    // Load inode information

    // Free direct blocks

    // Free indirect blocks

    // Clear inode in inode table
    return true;
}

// Inode stat ------------------------------------------------------------------

ssize_t FileSystem::stat(size_t inumber) {
    // Load inode information
    return 0;
}

// Read from inode -------------------------------------------------------------

ssize_t FileSystem::read(size_t inumber, char *data, size_t length, size_t offset) {
    // Load inode information

    // Adjust length

    // Read block and copy to data
    return 0;
}

// Write to inode --------------------------------------------------------------

ssize_t FileSystem::write(size_t inumber, char *data, size_t length, size_t offset) {
    // Load inode
    
    // Write block and copy to data
    return 0;
}

// Helper functions 
void FileSystem::readArray(uint32_t array[], size_t size, std::string* string) {
    for (unsigned int i = 0; i < size; i++) {
        if (array[i]) {// if is 0, means null
            *string += " ";
            *string += std::to_string(array[i]);
        }
    }
}