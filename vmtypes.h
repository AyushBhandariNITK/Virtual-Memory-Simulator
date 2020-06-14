#ifndef VMTYPES_H_   /* Include guard */
#define VMTYPES_H_

/*
    Defines a Virtual Memory Addressing table that can be
    represented as either a TLB cache or Page Table
*/
typedef struct vmTable_t {
    int *pageNumArr; // page number array
    int *frameNumArr; // frame number array for this
    int *entryAgeArr; // Age of each index
    int length;//the table size
    int pageFaultCount;//represents no of page faults,if uses as a page table
    int tlbHitCount;//represents tlb hit count
    int tlbMissCount;//represents tlb miss count 
} vmTable_t;



// This function creates a new Virtual Memory Table for
// Logical address referencing -- Can represent either the TLB or Page Table Cache
vmTable_t* createVMtable(int length);

// This function prints contents of the vmTable
void displayTable(vmTable_t** tableToView);


// This function frees dynamically allocated memory
void freeVMtable(vmTable_t** table);

// Accepts an int double pointer for creating simulated physical memory space
int** dramAllocate(int frameCount, int blockSize);

// Will free dram memory after usage
void freeDRAM(int*** dblPtrArr, int frameCount);


// 32-Bit masking function to extract page number
int getPageNumber(int mask, int value, int shift);

// 32-Bit masking function to extract page offset
int getOffset(int mask, int value);


#endif // VMTYPES_H_
