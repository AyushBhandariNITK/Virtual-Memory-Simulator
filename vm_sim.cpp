#include <iostream>
#include<cstdio>
#include<cstdlib>
#include<string>
#include<bits/stdc++.h>
#include <unistd.h>
#include <alloca.h>
#include <ctime>
#include "vmtypes.h"


 
#define FRAME_SIZE        256       // Size of each frame1
#define TOTAL_FRAME_COUNT 256       // Total number of frames in physical memory1
#define PAGE_MASK         0xFF00    // Masks everything but the page number
#define OFFSET_MASK       0xFF      // Masks everything but the offset
#define SHIFT             8         // Amount to shift when bitmasking
#define TLB_SIZE          16        // size of the TLB1
#define PAGE_TABLE_SIZE   256       // size of the page table1
#define MAX_ADDR_LEN      10        // The number of characters to read for each line from input file.1
#define PAGE_READ_SIZE    256       // Number of bytes to read1

using namespace std; 

class vm_sim
{
public:
int y;

vmTable_t* tlbTable; // The TLB Structure from the vmTable_t structure
vmTable_t* pageTable; // The Page Table  from the vmTable_t structure
int** dram; // Physical Memory from dramAllocate() function

char algo_choice; // menu stdin
char display_choice; // menu stdin


int nextTLBentry = 0; // Tracks the next available index of entry into the TLB
int nextPage = 0;  // Tracks the next available page in the page table
int nextFrame = 0; // Tracks the next available frame TLB or Page Table

// input file and backing store
FILE* address_file;
FILE* backing_store;

// how we store reads from input file
//or you can say what we get from the input file are these things
char addressReadBuffer[MAX_ADDR_LEN];//MAX_ADDR_LEN:The number of characters to read for each line from input file
int virtual_addr;
int page_number;
int offset_number;

// Generating length of time for a function
clock_t start, end;
double cpu_time_used;
int functionCallCount = 0;

// the buffer containing reads from backing store
signed char fileReadBuffer[PAGE_READ_SIZE];

// the translatedValue of the byte (signed char) in memory
signed char translatedValue;

// Function Prototypes
void translateAddress();
void readFromStore(int pageNumber);
void tlbFIFOinsert(int pageNumber, int frameNumber);
void tlbLRUinsert(int pageNumber, int frameNumber);
int getOldestEntry(int tlbSize);
double getAvgTimeInBackingStore();





};


// main opens necessary files and calls on translateAddress for every entry in the addresses file
int main(int argc, char *argv[])
{
    vm_sim v;
    v.tlbTable = createVMtable(TLB_SIZE); // The TLB Structure
    v.pageTable = createVMtable(PAGE_TABLE_SIZE); // The Page Table
    v.dram = dramAllocate(TOTAL_FRAME_COUNT, FRAME_SIZE); // Physical Memory
    int translationCount = 0;
    string algoName;

    // performing  basic error checking
    if (argc != 2) {
        fprintf(stderr,"Usage: ./vm_sim [input file]\n");
        return -1;
    }

    // open the file containing the backing store
    v.backing_store = fopen("BACKING_STORE.bin", "rb");

    if (v.backing_store == NULL) {
        fprintf(stderr, "Error opening BACKING_STORE.bin %s\n","BACKING_STORE.bin");
        return -1;
    }

    // open the file containing the logical addresses
    v.address_file = fopen(argv[1], "r");//means its reading from inputfile.txt

    if (v.address_file == NULL) {
        fprintf(stderr, "Error opening %s. Expecting [InputFile].txt or equivalent.\n",argv[1]);
        return -1;
    }

    cout<<"\nWelcome to our VM Simulator Version"<<endl;
    cout<<"\nNumber of logical pages: "<<PAGE_TABLE_SIZE<<"\nPage size: "<<PAGE_READ_SIZE<<" bytes\nPage Table Size: "<<PAGE_TABLE_SIZE<<"\nTLB Size: "<<TLB_SIZE<<" entries\nNumber of Physical Frames: "<<FRAME_SIZE<<"\nPhysical Memory Size: "<<PAGE_READ_SIZE * FRAME_SIZE<<" bytes";


    do {
        cout<<"\nDisplay All Physical Addresses? [y/n]: ";
        cin>>v.display_choice;
    } while (v.display_choice != 'n' && v.display_choice != 'y');

    do {
        cout<<"\nChoose TLB Replacement Strategy [1: FIFO, 2: LRU]: ";
        cin>>v.algo_choice;
    } while (v.algo_choice != '1' && v.algo_choice != '2');


    // Read through the input file and output each virtual address
    while (fgets(v.addressReadBuffer, MAX_ADDR_LEN, v.address_file) != NULL) {
        v.virtual_addr = atoi(v.addressReadBuffer); // converting from ascii to int

        // 32-bit masking function to extract page number
        v.page_number = getPageNumber(PAGE_MASK, v.virtual_addr, SHIFT);

        // 32-bit masking function to extract page offset
        v.offset_number = getOffset(OFFSET_MASK, v.virtual_addr);

        // Get the physical address and translated Value stored at that address
        v.translateAddress();//translateAddress funcion and learn his functioning
        translationCount++;  // increment the number of translated addresses(means total addresses converted)
    }

    // Determining stdout algo name for Menu
    if (v.algo_choice == '1') {
        algoName = "FIFO";
    }
    else {
        algoName = "LRU";
    }

    cout<<"\n-----------------------------------------------------------------------------------\n";
    // calculate and print out the stats
    cout<<"\nResults Using "<<algoName<<" Algorithm: \n";
    cout<<"Number of translated addresses = "<<translationCount<<"\n";
    double pfRate = (double)v.pageTable->pageFaultCount / (double)translationCount;//total 
    double TLBRate = (double)v.tlbTable->tlbHitCount / (double)translationCount;
    
    cout<<"Page Faults = "<<(v.pageTable->pageFaultCount)<<"\n";
    cout<<"Page Fault Rate = "<<pfRate * 100<<" %%\n";
    cout<<"TLB Hits = "<<(v.tlbTable->tlbHitCount)<<"\n";
    cout<<"TLB Hit Rate = "<<TLBRate * 100<<" %%\n";
    cout<<"Average time spent retrieving data from backing store:"<<v.getAvgTimeInBackingStore()<<" millisec\n";
    cout<<"\n-----------------------------------------------------------------------------------\n";


    // close the input file and backing store
    fclose(v.address_file);
    fclose(v.backing_store);

    // NOTE: REMEMBER TO FREE DYNAMICALLY ALLOCATED MEMORY
    freeVMtable(&(v.tlbTable));
    freeVMtable(&(v.pageTable));
    freeDRAM(&(v.dram), TOTAL_FRAME_COUNT);
    return 0;
}


/*
    This function takes the virtual address and translates
    into physical addressing, and retrieves the translatedValue stored
    @Param algo_type Is '1' for tlbFIFOinsert and '2' for tlbLRUinsert
 */
//till 170
void vm_sim:: translateAddress()
{
    // First try to get page from TLB
    int frame_number = -1; // Assigning -1 to frame_number means invalid initially

    /*
        Look through the TLB to see if memory page exists.
        If found, extract frame number and increment hit count
    */
    for (int i = 0; i < tlbTable->length; i++) {
        if (tlbTable->pageNumArr[i] == page_number) {
            frame_number = tlbTable->frameNumArr[i];
            tlbTable->tlbHitCount++;
            break;
        }
    }

    /*
        We now need to see if there was a TLB miss.
        If so, increment the tlb miss count and go through
        the page table to see if the page number exists. If not on page table,
        read secondary storage and increment page table fault count.
    */
    if (frame_number == -1) {
        tlbTable->tlbMissCount++; // Increment the miss count
        // walk the contents of the page table
        for(int i = 0; i < nextPage; i++){
            if(pageTable->pageNumArr[i] == page_number){  // If the page is found in those contents
                frame_number = pageTable->frameNumArr[i]; // Extract the frame_number
                break; // Found in pageTable
            }
        }
        // NOTE: Page Table Fault Encountered
        if(frame_number == -1) {  // if the page is not found in those contents
            pageTable->pageFaultCount++;   // Increment the number of page faults
            // page fault, call to readFromStore to get the frame into physical memory and the page table
            start = clock();
            readFromStore(page_number);
            cpu_time_used += (double)(clock() - start) / CLOCKS_PER_SEC;
            functionCallCount++;
            frame_number = nextFrame - 1;  // And set the frame_number to the current nextFrame index
        }
    }

    if (algo_choice == '1') {
        // Call function to insert the page number and frame number into the TLB
        tlbFIFOinsert(page_number, frame_number);
    }
    else {
        tlbLRUinsert(page_number, frame_number);
    }

    // Frame number and offset used to get the signed translatedValue stored at that address
    translatedValue = dram[frame_number][offset_number];
    y=(frame_number << SHIFT) | offset_number;
    if (display_choice == 'y') {
        // output the virtual address, physical address and translatedValue of the signed char to the console
        cout<<"\nVirtual address: "<<virtual_addr<<"\t\tPhysical address: "<<y<<"\t\tValue: "<<(int)translatedValue;
    }
}

// Function to read from the backing store and bring the frame into physical memory and the page table
void vm_sim :: readFromStore(int pageNumber)
{
    // first seek to byte PAGE_READ_SIZE in the backing store
    // SEEK_SET in fseek() seeks from the beginning of the file
    if (fseek(backing_store, pageNumber * PAGE_READ_SIZE, SEEK_SET) != 0) {
        fprintf(stderr, "Error seeking in backing store\n");
    }

    // now read PAGE_READ_SIZE bytes from the backing store to the fileReadBuffer
    if (fread(fileReadBuffer, sizeof(signed char), PAGE_READ_SIZE, backing_store) == 0) {
        fprintf(stderr, "Error reading from backing store\n");
    }

    // Load the bits into the first available frame in the physical memory 2D array
    for (int i = 0; i < PAGE_READ_SIZE; i++) {
        dram[nextFrame][i] = fileReadBuffer[i];
    }

    // Then we want to load the frame number into the page table in the next frame
    pageTable->pageNumArr[nextPage] = pageNumber;
    pageTable->frameNumArr[nextPage] = nextFrame;

    // increment the counters that track the next available frames
    nextFrame++;
    nextPage++;


}

//till 264
// Function to insert a page number and frame number into the TLB with a FIFO replacement
void vm_sim :: tlbFIFOinsert(int pageNumber, int frameNumber)
{
    int i;

    // If it's already in the TLB, break
    for(i = 0; i < nextTLBentry; i++){
        if(tlbTable->pageNumArr[i] == pageNumber){
            break;
        }
    }

    // if the number of entries is equal to the index
    if(i == nextTLBentry){
        if(nextTLBentry < TLB_SIZE){  // IF TLB Buffer has open positions
            tlbTable->pageNumArr[nextTLBentry] = pageNumber;    // insert the page and frame on the end
            tlbTable->frameNumArr[nextTLBentry] = frameNumber;
        }
        else{ // No room in TLB Buffer

            // Replace the last TLB entry with our new entry
            tlbTable->pageNumArr[nextTLBentry - 1] = pageNumber;
            tlbTable->frameNumArr[nextTLBentry - 1] = frameNumber;

            // Then shift up all the TLB entries by 1 so we can maintain FIFO order
            for(i = 0; i < TLB_SIZE - 1; i++){
                tlbTable->pageNumArr[i] = tlbTable->pageNumArr[i + 1];
                tlbTable->frameNumArr[i] = tlbTable->frameNumArr[i + 1];
            }
        }
    }

    // If the index is not equal to the number of entries
    else{

        for(i = i; i < nextTLBentry - 1; i++){  // iterate through up to one less than the number of entries
            // Move everything over in the arrays
            tlbTable->pageNumArr[i] = tlbTable->pageNumArr[i + 1];
            tlbTable->frameNumArr[i] = tlbTable->frameNumArr[i + 1];
        }
        if(nextTLBentry < TLB_SIZE){  // if there is still room in the array, put the page and frame on the end
             // Insert the page and frame on the end
            tlbTable->pageNumArr[nextTLBentry] = pageNumber;
            tlbTable->frameNumArr[nextTLBentry] = frameNumber;

        }
        else{  // Otherwise put the page and frame on the number of entries - 1
            tlbTable->pageNumArr[nextTLBentry - 1] = pageNumber;
            tlbTable->frameNumArr[nextTLBentry - 1] = frameNumber;
        }
    }
    if(nextTLBentry < TLB_SIZE) { // If there is still room in the arrays, increment the number of entries
        nextTLBentry++;
    }
}


// Function to insert a page number and frame number into the TLB with a LRU replacement
void vm_sim :: tlbLRUinsert(int pageNumber, int frameNumber)
{

    bool freeSpotFound = false;
    bool alreadyThere = false;
    int replaceIndex = -1;

    // SEEK -- > Find the index to replace and increment age for all other entries
    for (int i = 0; i < TLB_SIZE; i++) {
        if ((tlbTable->pageNumArr[i] != pageNumber) && (tlbTable->pageNumArr[i] != 0)) {
            // If entry is not in TLB and not a free spot, increment its age
            tlbTable->entryAgeArr[i]++;
        }
        else if ((tlbTable->pageNumArr[i] != pageNumber) && (tlbTable->pageNumArr[i] == 0)) {
            // Available spot in TLB found
            if (!freeSpotFound) {
                replaceIndex = i;
                freeSpotFound = true;
            }
        }
        else if (tlbTable->pageNumArr[i] == pageNumber) {
            // Entry is already in TLB -- Reset its age
            if(!alreadyThere) {
                tlbTable->entryAgeArr[i] = 0;
                alreadyThere = true;
            }
        }
    }

    // REPLACE
    if (alreadyThere) {
        return;
    }
    else if (freeSpotFound) { // If we found a free spot, insert
        tlbTable->pageNumArr[replaceIndex] = pageNumber;    // Insert into free spot
        tlbTable->frameNumArr[replaceIndex] = frameNumber;
        tlbTable->entryAgeArr[replaceIndex] = 0;
    }
    else { // No more free spots -- Need to replace the oldest entry
        replaceIndex = getOldestEntry(TLB_SIZE);
        tlbTable->pageNumArr[replaceIndex] = pageNumber;    // Insert into oldest entry
        tlbTable->frameNumArr[replaceIndex] = frameNumber;
        tlbTable->entryAgeArr[replaceIndex] = 0;

    }
}

// Finds the oldest entry in TLB and returns the replacement Index
int vm_sim :: getOldestEntry(int tlbSize) {
  int i, max, index;

  max = tlbTable->entryAgeArr[0];
  index = 0;

  for (i = 1; i < tlbSize; i++) {
    if (tlbTable->entryAgeArr[i] > max) {
       index = i;
       max = tlbTable->entryAgeArr[i];
    }
  }
  return index;
}

// Just computes the average time complexity of accessing the backing store
double vm_sim :: getAvgTimeInBackingStore() {
    double temp = (double) cpu_time_used / (double) functionCallCount;
    return temp * 1000000;
}
