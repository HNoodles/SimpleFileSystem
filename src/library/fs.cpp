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
    // loop over inode blocks
    for (size_t i = 0; i < superBlock.Super.InodeBlocks; i++) {
        // read in one inode block
        disk->read(i + 1, inodeBlock.Data);

        // loop over inodes in the block
        for (size_t j = 0; j < INODES_PER_BLOCK; j++) {
            Inode inode = inodeBlock.Inodes[j];

            // skip invalid inode
            if (!inode.Valid)
                continue;
            
            // loop over direct blocks
            std::string directBlocks = "";
            readArray(inode.Direct, POINTERS_PER_INODE, &directBlocks);
            printf("Inode %zu:\n"            , j);
            printf("    size: %u bytes\n"   , inode.Size);
            printf("    direct blocks:%s\n" , directBlocks.c_str());

            // skip invalid indirect node
            if (!inode.Indirect)
                continue;
            
            // loop over indirect blocks
            Block indirect;
            disk->read(inode.Indirect, indirect.Data);
            std::string indirectBlocks = "";
            readArray(indirect.Pointers, POINTERS_PER_BLOCK, &indirectBlocks);
            printf("    indirect block: %u\n"       , inode.Indirect);
            printf("    indirect data blocks:%s\n"  , indirectBlocks.c_str());
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
    for (size_t i = 1; i < superBlock.Super.Blocks; i++) {
        disk->write(i, empty.Data);
    }

    return true;
}

// Mount file system -----------------------------------------------------------

bool FileSystem::mount(Disk *disk) {
    // return false if mounted, prevent repeated mounting
    if (disk->mounted())
        return false;

    // Read superblock
    Block superBlock;
    disk->read(0, superBlock.Data);

    if (superBlock.Super.MagicNumber != MAGIC_NUMBER  // check magic number
    || superBlock.Super.InodeBlocks != superBlock.Super.Blocks * 0.1 + 0.5  // check inode ratio
    || superBlock.Super.Inodes != superBlock.Super.InodeBlocks * INODES_PER_BLOCK) // check inode number
        return false;

    // Set device and mount
    disk->mount();

    // Copy metadata
    this->disk = disk;
    this->blocks = superBlock.Super.Blocks;
    this->inodeBlocks = superBlock.Super.InodeBlocks;

    // Allocate free block bitmap
    initialize_free_blocks();

    return true;
}

// Create inode ----------------------------------------------------------------

ssize_t FileSystem::create() {
    // Locate free inode in inode table
    // Read Inode blocks
    Block inodeBlock;
    Inode inode;
    // loop over inode blocks to find an empty inode
    ssize_t inumber = -1;
    for (size_t i = 0; i < inodeBlocks; i++) {
        // read in one inode block
        disk->read(i + 1, inodeBlock.Data);

        // loop over inodes in the block
        for (size_t j = 0; j < INODES_PER_BLOCK; j++) {
            inode = inodeBlock.Inodes[j];
            // find invalid inode
            if (!inode.Valid) {
                inumber = i * INODES_PER_BLOCK + j;
                break;
            }
        }

        // break if already found an empty inode
        if (inumber != -1) {
            break;
        }
    }

    // not found, create failed
    if (inumber == -1)
        return inumber;

    // found, set valid bit and size
    inode.Valid = 1;
    inode.Size = 0;
    // set direct and indirect
    for (size_t k = 0; k < POINTERS_PER_INODE; k++) {
        inode.Direct[k] = 0;
    }
    inode.Indirect = 0;

    // write inode onto disk
    save_inode(inumber, &inode);

    // Record inode if found
    return inumber;
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

// Helper functions ------------------------------------------------------------
void FileSystem::initialize_free_blocks() {
    bitMap = std::vector<int>(blocks, FREE);

    // super block is occupied
    bitMap[0] = OCCUPIED;

    // Read Inode blocks
    Block inodeBlock;
    // loop over inode blocks
    for (size_t i = 0; i < inodeBlocks; i++) {
        // read in one inode block
        disk->read(i + 1, inodeBlock.Data);

        // loop over inodes in the block
        for (size_t j = 0; j < INODES_PER_BLOCK; j++) {
            Inode inode = inodeBlock.Inodes[j];

            // skip invalid inode
            if (!inode.Valid)
                continue;
            
            // loop over direct blocks
            for (size_t k = 0; k < POINTERS_PER_INODE; k++) {
                bitMap[inode.Direct[k]] = OCCUPIED;
            }

            // skip invalid indirect node
            if (!inode.Indirect)
                continue;
            
            // loop over indirect blocks
            Block indirect;
            disk->read(inode.Indirect, indirect.Data);
            for (size_t k = 0; k < POINTERS_PER_BLOCK; k++) {
                bitMap[indirect.Pointers[k]] = OCCUPIED;
            }
        }
    }
}

bool FileSystem::save_inode(size_t inumber, Inode *node) {
    uint32_t blockIndex = inumber / INODES_PER_BLOCK + 1;
    uint32_t pointerIndex = inumber % INODES_PER_BLOCK;

    // read the whole block
    Block block;
    disk->read(blockIndex, block.Data);

    // modify the inode
    block.Inodes[pointerIndex] = *node;

    // write back
    disk->write(blockIndex, block.Data);

    return true;
}

void FileSystem::readArray(uint32_t array[], size_t size, std::string* string) {
    for (size_t i = 0; i < size; i++) {
        if (array[i]) {// if is 0, means null
            *string += " ";
            *string += std::to_string(array[i]);
        }
    }
}