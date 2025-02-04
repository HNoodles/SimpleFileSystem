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
            debugArray(inode.Direct, POINTERS_PER_INODE, &directBlocks);
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
            debugArray(indirect.Pointers, POINTERS_PER_BLOCK, &indirectBlocks);
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
    memset(superBlock.Data, 0, disk->BLOCK_SIZE);
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
    || superBlock.Super.InodeBlocks != (size_t)((float)(superBlock.Super.Blocks * 0.1) + 0.5)  // check inode ratio
    || superBlock.Super.Inodes != superBlock.Super.InodeBlocks * INODES_PER_BLOCK) // check inode number
        return false;

    // Set device and mount
    disk->mount();

    // Copy metadata
    this->disk = disk;
    this->blocks = superBlock.Super.Blocks;
    this->inodeBlocks = superBlock.Super.InodeBlocks;
    this->inodes = superBlock.Super.Inodes;

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
                inumber =  i * INODES_PER_BLOCK + j;
                break;
            }
        }

        // break if already found invalid node
        if (inumber != -1)
            break;
    }

    // found, write inode
    if (inumber != -1) {
        Inode inode;
        inode.Valid = 1;
        inode.Size = 0;
        for (size_t k = 0; k < POINTERS_PER_INODE; k++) {
            inode.Direct[k] = 0;
        }
        inode.Indirect = 0;

        // write inode onto disk
        save_inode(inumber, &inode, &inodeBlock, false);
    }

    // Record inode, if not found, inumber = -1
    return inumber;
}

// Remove inode ----------------------------------------------------------------

bool FileSystem::remove(size_t inumber) {
    // Load inode information
    Block inodeBlock;
    Inode inode;
    if (!load_inode(inumber, &inode, &inodeBlock, true))
        // invalid inode to remove
        return false;

    // Free direct blocks
    for (size_t i = 0; i < POINTERS_PER_INODE; i++) {
        bitMap[inode.Direct[i]] = FREE;
    }

    // Free indirect blocks if there are
    if (inode.Indirect) {
        Block indirect;
        disk->read(inode.Indirect, indirect.Data);
        for (size_t i = 0; i < POINTERS_PER_BLOCK; i++) {
            bitMap[indirect.Pointers[i]] = FREE;
        }
    }

    // Clear inode in inode table
    inode.Valid = 0;
    save_inode(inumber, &inode, &inodeBlock, false);

    return true;
}

// Inode stat ------------------------------------------------------------------

ssize_t FileSystem::stat(size_t inumber) {
    // Load inode information
    Block inodeBlock;
    Inode inode;
    if (!load_inode(inumber, &inode, &inodeBlock, true)) {
        // invalid inode to remove
        return -1;
    }
    
    return inode.Size;
}

// Read from inode -------------------------------------------------------------

ssize_t FileSystem::read(size_t inumber, char *data, size_t length, size_t offset) {
    // Load inode information
    Block inodeBlock;
    Inode inode;
    if (!load_inode(inumber, &inode, &inodeBlock, true)) {
        // invalid inode to remove
        return -1;
    }

    // Adjust length, length shouldn't be larger than the data remaining
    size_t rlength = std::min(length, inode.Size - offset);
    size_t size = 0;

    // return when there is nothing to read
    if (rlength == 0)
        return size;

    // Read block and copy to data
    size_t skipBlocks = offset / disk->BLOCK_SIZE;
    size_t remainder = offset % disk->BLOCK_SIZE;

    switch (readArray(inode.Direct, POINTERS_PER_INODE, &size, 
    &skipBlocks, &remainder, &rlength, data)) {
        case 0: // if read finished
            return size;
        case -1: // error occurred
            return -1;
        case 1: // still needs reading
            break;
    }
    
    // indirect block needs to be read
    // if there is no indirect block
    if (!inode.Indirect) {
        return -1;
    }

    // read in indirect block
    Block indirect;
    disk->read(inode.Indirect, indirect.Data);
    
    switch (readArray(indirect.Pointers, POINTERS_PER_BLOCK, &size, 
    &skipBlocks, &remainder, &rlength, data)) {
        case 0: // if read finished
            return size;
        case -1: // error occurred
            return -1;
        case 1: // still needs reading
            break;
    }
        
    // still have bytes unread, something wrong
    return -1;
}

// Write to inode --------------------------------------------------------------

ssize_t FileSystem::write(size_t inumber, char *data, size_t length, size_t offset) {
    // Load inode
    Block inodeBlock;
    Inode inode;
    if (!load_inode(inumber, &inode, &inodeBlock, true)) {
        // invalid inode to remove
        return -1;
    }

    size_t rlength = length;
    size_t size = 0;

    // return when there is nothing to read
    if (rlength == 0)
        return size;

    // Read block and copy to data
    size_t skipBlocks = offset / disk->BLOCK_SIZE;
    size_t remainder = offset % disk->BLOCK_SIZE;

    switch (writeArray(inode.Direct, POINTERS_PER_INODE, &size, 
    &skipBlocks, &remainder, &rlength, data)) {
        case 0: // if write finished
            // update inode
            inode.Size += size;
            save_inode(inumber, &inode, &inodeBlock, false);
            return size;
        case -1: // error occurred
            // update inode
            inode.Size += size;
            save_inode(inumber, &inode, &inodeBlock, false);
            return -1;
        case 1: // still needs writing
            break;
    }
    
    // indirect block needs to be read
    Block indirect;

    // if there is no indirect block, allocate one
    if (!inode.Indirect) {
        ssize_t blockNum = allocate_free_block();

        // no free block
        if (blockNum == -1)
            return -1;
        
        inode.Indirect = blockNum;

        memset(indirect.Data, 0, disk->BLOCK_SIZE);
    } else {
        // already have indirect block, read in
        disk->read(inode.Indirect, indirect.Data);
    }
    
    switch (writeArray(indirect.Pointers, POINTERS_PER_BLOCK, &size, 
    &skipBlocks, &remainder, &rlength, data)) {
        case 0: // if write finished
            // update inode
            inode.Size += size;
            save_inode(inumber, &inode, &inodeBlock, false);
            disk->write(inode.Indirect, indirect.Data);
            return size;
        case -1: // error occurred
            // update inode
            inode.Size += size;
            save_inode(inumber, &inode, &inodeBlock, false);
            disk->write(inode.Indirect, indirect.Data);
            return -1;
        case 1: // still needs writing
            break;
    }
        
    // still have bytes unwritten, something wrong
    // update inode
    inode.Size += size;
    save_inode(inumber, &inode, &inodeBlock, false);
    disk->write(inode.Indirect, indirect.Data);
    return -1;
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
        // inode blocks are occupied
        bitMap[i + 1] = OCCUPIED;

        // read in one inode block
        disk->read(i + 1, inodeBlock.Data);

        // loop over inodes in the block
        for (size_t j = 0; j < INODES_PER_BLOCK; j++) {
            Inode inode = inodeBlock.Inodes[j];

            // skip invalid inode
            if (!inode.Valid)
                continue;
            
            // // compute how many blocks are needed
            size_t blockNum = inode.Size / disk->BLOCK_SIZE;
            if ((inode.Size % disk->BLOCK_SIZE) > 0)
                blockNum++;
            
            // loop over direct blocks
            for (size_t k = 0; k < POINTERS_PER_INODE && blockNum > 0; k++) {
                bitMap[inode.Direct[k]] = OCCUPIED;
                blockNum--;
            }

            // skip invalid indirect node
            if (!inode.Indirect)
                continue;
            
            // loop over indirect blocks
            Block indirect;
            disk->read(inode.Indirect, indirect.Data);
            bitMap[inode.Indirect] = OCCUPIED;
            for (size_t k = 0; k < POINTERS_PER_BLOCK && blockNum > 0; k++) {
                bitMap[indirect.Pointers[k]] = OCCUPIED;
                blockNum--;
            }
        }
    }
}

ssize_t FileSystem::allocate_free_block() {
    for (size_t i = 1 + inodeBlocks; i < blocks; i++) {
        if (bitMap[i] == FREE) {
            bitMap[i] = OCCUPIED;
            return i;
        }
    }
    return -1; 
}

bool FileSystem::load_inode(size_t inumber, Inode *node, Block *block, bool needRead) {
    size_t blockIndex = (inumber / INODES_PER_BLOCK) + 1;
    size_t pointerIndex = inumber % INODES_PER_BLOCK;

    // read the whole block if needed
    if (needRead)
        disk->read(blockIndex, block->Data);

    // read the inode
    *node = block->Inodes[pointerIndex];

    return node->Valid;
}

bool FileSystem::save_inode(size_t inumber, Inode *node, Block *block, bool needRead) {
    size_t blockIndex = (inumber / INODES_PER_BLOCK) + 1;
    size_t pointerIndex = inumber % INODES_PER_BLOCK;

    // read the whole block if needed
    if (needRead)
        disk->read(blockIndex, block->Data);

    // modify the inode
    block->Inodes[pointerIndex] = *node;

    // write back
    disk->write(blockIndex, block->Data);

    return true;
}

void FileSystem::debugArray(uint32_t array[], size_t arraySize, std::string* string) {
    for (size_t i = 0; i < arraySize; i++) {
        if (array[i]) {// if is 0, means null
            *string += " ";
            *string += std::to_string(array[i]);
        }
    }
}

ssize_t FileSystem::readArray(uint32_t array[], size_t arraySize, size_t *size, 
size_t *skipBlocks, size_t *remainder, size_t *rlength, char *data) {
    // block for reading data
    Block block;
    
    for (size_t i = 0; i < arraySize; i++) {
        // skip blocks if needed
        if ((*skipBlocks) > 0) {
            (*skipBlocks)--;
            continue;
        }

        // invalid blocks
        if (array[i] == 0)
            return -1;
            
        disk->read(array[i], block.Data);

        // determine how long to read
        size_t bytesToRead = std::min(disk->BLOCK_SIZE - (*remainder), (*rlength));

        // skip remainder if there is, only first block will have remainder
        memcpy(data + (*size), block.Data + (*remainder), bytesToRead);
        (*size) += bytesToRead;

        // mark that one data block has been read
        (*rlength) -= bytesToRead;
        
        if ((*remainder) != 0)
            (*remainder) = 0;

        // return when read completed
        if ((*rlength) == 0)
            return 0;
    }

    // means that still needs further reading
    return 1;
}

ssize_t FileSystem::writeArray(uint32_t array[], size_t arraySize, size_t *size, 
size_t *skipBlocks, size_t *remainder, size_t *rlength, char *data) {
    // block for writing data
    Block block;

    for (size_t i = 0; i < arraySize; i++) {
        // skip blocks if needed
        if ((*skipBlocks) > 0) {
            (*skipBlocks)--;
            continue;
        }

        // determine how long to write
        size_t bytesToWrite = std::min(disk->BLOCK_SIZE - (*remainder), (*rlength));
        bool writingPartialBlock = bytesToWrite < disk->BLOCK_SIZE;

        // invalid blocks, allocate one
        if (array[i] == 0) {
            // only need to clean the new allocated block when we write 
            // partial of the block
            ssize_t blockNum = allocate_free_block();
            
            // no free block
            if (blockNum == -1)
                return -1;
            
            array[i] = blockNum;

            // clear the offset bits
            if (writingPartialBlock)
                memset(block.Data, 0, *remainder);
        }

        // should read in the block only when we write part of the block
        // otherwise, we don't care what was in the block
        if (writingPartialBlock) {
            disk->read(array[i], block.Data);
        }
        
        // skip remainder if there is, only first block will have remainder
        memcpy(block.Data + (*remainder), data + (*size), bytesToWrite);
        (*size) += bytesToWrite;

        disk->write(array[i], block.Data);

        // mark that one data block has been written
        (*rlength) -= bytesToWrite;
        
        if ((*remainder) != 0)
            (*remainder) = 0;

        // return when write completed
        if ((*rlength) == 0)
            return 0;
    }

    // means that still needs further writing
    return 1;
}