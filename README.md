# Operating System I - Project I - Simple File System

### 17302010002 黄元敏



Design
------

1. To implement `Filesystem::debug`, you will need to load the file system data structures and report the **superblock** and **inodes**.
      - How will you read the superblock?
           - The superblock contains four numbers, the magic number, the number of blocks, the number of inode blocks and the number of inodes. They can be read in using the `Data` block type in the defined union `Block` and then be interpreted easily using the `Super` block type. 
      - How will you traverse all the inodes?
          - From `Super.InodeBlocks`, we can get the number of inode blocks. Also, we know that from the second block, the inode blocks begin. Hence, we only need to have a loop which traverses from the `second` block to the `InodeBlocks + 1`th block. 
          - Then, in each inode block, we know that there are `INODES_PER_BLOCK = 128` inodes, so here is the second loop which goes from the first inode to the 128th one within one inode block . 
          - With the two loops above, we can go through every inode in the given disk.  
      - How will you determine all the information related to an inode?
          - Within each inode, the first thing we need to determine is the validity, which we can achieve from `Inode.Valid`. 
          - Then, the size of a inode is defined in `Inode.Size`. 
      - How will you determine all the blocks related to an inode?
          - The direct blocks can be found using a loop built according to `POINTERS_PER_INODE = 5`. We just need to concatenate all the non-zero block numbers written in each direct array element.
          - Whether the inode contains an indirect block can be told by whether `Inode.Indirect` equals zero. Therefore, if `Inode.Indirect` is a non-zero number, it points to an indirect block. If so, we can read in the block using `Disk::read` and then go through the `POINTERS_PER_INODE = 1024` block pointers in the block to find non-zero data block pointers. 

2. To implement `FileSystem::format`, you will need to write the superblock and clear the remaining blocks in the file system.
      - What pre-condition must be true before this operation can succeed?
           - `Disk::mounted` must be false, otherwise, do nothing and return false.  
      - What information must be written into the superblock?
           - According to the definition of superblock, we should construct a union `Block` and give it a `MagicNumber = MAGIC_NUMBER`, a `Blocks = disk->size()`, a `InodeBlocks` which is rounded from 10% of `Blocks` and `Inodes = InodeBlocks * INODES_PER_BLOCK`.  
      - How would you clear all the remaining blocks?
           - To utilize the `Disk::write` function, we can use a loop from the second block to the last one to clear each of the remaining blocks one by one. For each block, we can write out a block filled with `0`, created using `memset` function. 

3. To implement `FileSystem::mount`, you will need to prepare a filesystem for use by reading the superblock and allocating the free block bitmap.

      - What pre-condition must be true before this operation can succeed?
        - The disk hasn't been mounted. 
      - What sanity checks must you perform?
        - The `MagicNumber` should be right. 
        - The `InodeBlocks` should equal `Ceil(0.1 * Blocks)`. 
        - The `Inodes` should equal `InodeBlocks * INODES_PER_BLOCK`. 
      - How will you record that you mounted a disk?
        - Call `Disk::mount` which will update the `Disk::Mounts` attribute. 
      - How will you determine which blocks are free?
        - I will build a bit map using `std::vector`, where contains `Blocks` of int values, each one of which represents whether a block is free or occupied. In my design, there are two const int values defined in `fs.h`: `FREE = 1`, `OCCUPIED = 0`. 
        - Go through the inode blocks, together with the indirect blocks defined within them, to mark the corresponding element in bit map for each one of the used data block. 
        - The work is done by `initialize_free_blocks()`. 

4. To implement `FileSystem::create`, you will need to locate a free inode and save a new inode into the inode table.

      - How will you locate a free inode?
        - This is a relatively basic operation which only calls for a double loop to go through every inode block of a disk then every inode of an inode block. The program doesn't pursue performance, so we can simply do a linear scan and record the first invalid inode we find which would be the free inode we are looking for. 
        - If no empty inode was found, -1 should be returned. 
      - What information would you see in a new inode?
        - The new inode should hava `Valid = 1`, `Size = 0`, `Direct[0 ~ (POINTERS_PER_INODE - 1)] = 0` and `Indirect = 0`. 
      - How will you record this new inode?
        - I implemented `FileSystem::save_inode` as instructed. The function should compute the block index and inode index according to the `inumber` passed in. 
        - Then, it should read in the corresponding block and modify the selected inode. 
        - Finally, it should write the whole block back onto the disk. 

5. To implement `FileSystem::remove`, you will need to locate the inode and then free its associated blocks.

      - How will you determine if the specified inode is valid?
      - How will you free the direct blocks?
      - How will you free the indirect blocks?
      - How will you update the inode table?



6. To implement `FileSystem::stat`, you will need to locate the inode and return its size.
   
      - How will you determine if the specified inode is valid?
      - How will you determine the inode's size?



7. To implement `FileSystem::read`, you will need to locate the inode and copy data from appropriate blocks to the user-specified data buffer.

      - How will you determine if the specified inode is valid?
      - How will you determine which block to read from?
      - How will you handle the offset?
      - How will you copy from a block to the data buffer?



8. To implement `FileSystem::write`, you will need to locate the inode and copy data the user-specified data buffer to data blocks in the file system.

      - How will you determine if the specified inode is valid?
      - How will you determine which block to write to?
      - How will you handle the offset?
      - How will you know if you need a new block?
      - How will you manage allocating a new block if you need another one?
      - How will you copy from a block to the data buffer?
      - How will you update the inode?

Errata
------

> Describe any known errors, bugs, or deviations from the requirements.

Extra Credit
------------

> Describe what extra credit (if any) that you implemented.