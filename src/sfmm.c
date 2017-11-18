/**
 * All functions you make for the assignment must be implemented in this file.
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include "sfmm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/**
 * You should store the heads of your free lists in these variables.
 * Doing so will make it accessible via the extern statement in sfmm.h
 * which will allow you to pass the address to sf_snapshot in a different file.
 */
free_list seg_free_list[4] = {
    {NULL, LIST_1_MIN, LIST_1_MAX},
    {NULL, LIST_2_MIN, LIST_2_MAX},
    {NULL, LIST_3_MIN, LIST_3_MAX},
    {NULL, LIST_4_MIN, LIST_4_MAX}
};

void addToFreeList(sf_header* head);
void removeFromFreeList(sf_header* nextHead);
void makeFooter(sf_footer* newPageFooter, int alloc, int pad, int req_size, int block_size);
int sf_errno = 0;
int pageCounter = 0;

void *sf_malloc(size_t size) {
    sf_header *head;
    //check size requirements
    if(size <= 0 || size > 16384){
        sf_errno = EINVAL;
        return NULL;
    }
    //go through the list and pick the smallest block if there exists one. If there is,
    //make usingOldPage to True, and update the current page size.
    int usingOldPage = 0;
    int arrCount = 0;
    int currentPageSize = 4096;
    while(arrCount < 4){
        sf_free_header *listPtr = seg_free_list[arrCount].head;
        sf_free_header *listElement = listPtr;
        while(listElement != NULL){
            if(((listElement->header.block_size << 4)-16) >= size){
                head = &listElement->header;
                usingOldPage = 1;
                currentPageSize = listElement->header.block_size <<4;
                //take this block out of the list
                if(listElement->prev == NULL && listElement->next == NULL){
                    seg_free_list[arrCount].head = NULL;
                }
                else if(listElement->prev == NULL && listElement->next != NULL){
                    seg_free_list[arrCount].head = listElement->next;
                    listElement->next->prev = NULL;
                }
                else if(listElement->prev != NULL && listElement->next != NULL){
                    listElement->prev->next = listElement->next;
                    listElement->next->prev = listElement->prev;
                }
                else{
                    listElement->prev->next = NULL;
                }
                break;
            }
            listElement = listElement->next;
        }
        if(usingOldPage == 1){
            break;
        }
        arrCount++;
    }
    int padSize = 0;
    if(16 -(size % 16) != 16){
        padSize = 16 -(size % 16);
    }

    int blockSize = size + 16 + padSize;
    sf_footer *newPageFooter;
    //If there is no old page found, then call sbsrk and update the
    //page counter.  Then set the pointer for the page footer.
    if(usingOldPage == 0 && pageCounter < 4){
        int sizeCheck = size;
        head = sf_sbrk();
        newPageFooter = (sf_footer*)((char*)head + 4088);
        pageCounter++;
        if(head > (sf_header*)get_heap_start()){
            sf_footer *prevFoot = (sf_footer*)((char*)head - 8);
            if(prevFoot->allocated == 0){
                head = (sf_header*)((char*)head- (prevFoot->block_size << 4));

                //Delete the prevHeader from the list
                removeFromFreeList(head);
                //Update the currentPageSize
                currentPageSize += (head->block_size << 4);
                sizeCheck-= (head->block_size << 4);
            }
        }
        sizeCheck-= 4080;
        //If there is more than one page asked by the user, this loop will run.
        //It will call sb_sbrk and add the page right after the last page called
        //until the sizeCheck var <= 0.
        while(sizeCheck > 0 && pageCounter <= 4){
            if(pageCounter >= 4){
                head->block_size = currentPageSize>>4;
                head->allocated = 0;
                head->padded = 0;
                sf_footer* foot = (sf_footer*)((char*)head + currentPageSize - 8);
                makeFooter(foot, 0, 0, 0, currentPageSize);
                addToFreeList(head);
                sf_errno = ENOMEM;
                return NULL;
            }
            sf_sbrk();
            currentPageSize += 4096;
            pageCounter++;
            sizeCheck-= 4096;
            newPageFooter= (sf_footer*)((char*)newPageFooter + 4096);
        }
        //check PageCounter to make sure that it is less than 4.

    }
    else if(pageCounter >= 4 && usingOldPage == 0){
        sf_errno = ENOMEM;
        return NULL;
    }
    else{
        newPageFooter = (sf_footer*)((char*)head + currentPageSize - 8);
    }
    //Set the contents of the header.
    head->allocated = 1;
    if(padSize == 0){
        head->padded = 0;
    }
    else{
        head->padded = 1;
    }
    head->two_zeroes=0;
    head->unused=0;
    head->block_size= blockSize >> 4;
    //*payload is the RETURN VALUE.
    void* payload = (char*)head + 8;
    //set the footer value.  This footer is for the
    //memory being allocated
    sf_footer *foot = payload + size + padSize;
    foot->requested_size = size;
    foot->allocated = 1;
    if(padSize == 0){
        foot->padded = 0;
    }
    else{
        foot->padded = 1;
    }
    foot->two_zeroes=0;
    foot->block_size= blockSize >> 4;

    //if the block size != currentPageSize, then remove the remaining
    //page and place it into the list.
    if(blockSize != currentPageSize){

        sf_header *remainingPageHeader = (sf_header*)((char*)foot + 8);
        if(currentPageSize - blockSize != 16){
            remainingPageHeader->unused = 0;
            remainingPageHeader->allocated = 0;
            remainingPageHeader->padded = 0;
            remainingPageHeader->two_zeroes = 0;
            remainingPageHeader->block_size = (currentPageSize - blockSize) >> 4;

            newPageFooter->allocated = 0;
            newPageFooter->padded = 0;
            newPageFooter->two_zeroes = 0;
            newPageFooter->requested_size = 0;
            newPageFooter->block_size = (currentPageSize - blockSize) >> 4;

            int getListIndex = 0;
    //loop thru to find the correct list to place the head.
            while(getListIndex < 4){
                int listMin = seg_free_list[getListIndex].min;
                int listMax = seg_free_list[getListIndex].max;
                if((currentPageSize - blockSize) >= listMin && (currentPageSize - blockSize) <= listMax){
                    break;
                }
                getListIndex++;
            }

    //Set the values for the free header that saves the remaining page head.
            sf_free_header *remainingHeaderWrapper = (sf_free_header*)remainingPageHeader;
            remainingHeaderWrapper->header=*remainingPageHeader;
            remainingHeaderWrapper->prev = NULL;

    //Change the prev val for the last head of the freelist only if
    //there is something in the list head right now.
            if(seg_free_list[getListIndex].head != NULL){
                remainingHeaderWrapper->next = seg_free_list[getListIndex].head;
                remainingHeaderWrapper->next->prev = remainingHeaderWrapper;
            }

    //change the head of the freelist.
            seg_free_list[getListIndex].head = remainingHeaderWrapper;
        }
        else{
        //deal with the SPLINTER here
            head->block_size = (blockSize + 16)>>4;
            head->padded = 1;
            foot->padded = 1;
            sf_footer *newFooter = (sf_footer*)((char*)head + blockSize + 8);
            makeFooter(newFooter, 1, foot->padded, size, blockSize + 16);
        }
    }
    return payload;
}

void *sf_realloc(void *ptr, size_t size) {
    if(!ptr || ptr < get_heap_start() || ptr > get_heap_end()|| size < 0 || size >= 16384){
        abort();
    }

    //Get header and footer of the block
    sf_header *head = (sf_header*)((char*)ptr - 8);
    sf_footer *foot = (sf_footer*)((char*)head + (head->block_size <<4) - 8);

    if((void*)head < get_heap_start() || (void*)((char*)foot+8) > get_heap_end()){
        abort();
    }

    //CHECK IF PTR IS VALID
    //Must be allocated
    if(head->allocated == 0 || foot->allocated == 0|| head->block_size<<4 < 32 || foot->block_size<<4 < 32){
        abort();
    }
    //head and foot must have same vals
    if(head->padded != foot->padded || head->block_size != foot->block_size){
        abort();
    }
    //Get the padding value of prev val
    int padSize = 0;
    if(16 -(foot->requested_size % 16) != 16){
        padSize = 16 -(foot->requested_size % 16);
    }
    //Make sure that padding is consistent
    if(((padSize == 0 && head->padded == 1)&& foot->requested_size + padSize + 32 != head->block_size<<4) || (padSize > 0 && head->padded == 0)){
        abort();
    }
    if(foot->requested_size + 16 != head->block_size<<4 && head->padded == 0){
        abort();
    }
    //VALID PTR

    //If size = 0, then free.
    if(size == 0){
        sf_free(ptr);
        return NULL;
    }

    int blockSize = head->block_size << 4;
    int prevReqSize = blockSize - 16 - padSize;

    //newPad holds the padding for the new size and sizeWithPad is
    //the newPad and newly requested size
    int newPad = 0;
    if(16 -(size % 16) != 16){
        newPad = 16 -(size % 16);
    }
    int sizeWithPad = size + newPad;

    //What the user asked for is already the SAME.
    if(prevReqSize + padSize == sizeWithPad){
        if(newPad == 0){
            head->padded = 0;
        }
        else{
            head->padded = 1;
        }
        foot->padded = head->padded;
        foot->requested_size = size;
        return ptr;
    }

    //what the user asked for is LESS than the currently allocated size. If the diff = 16,
    //there will be a splinter.
    if(sizeWithPad < prevReqSize + padSize && prevReqSize + padSize - sizeWithPad != 16){

        //Update the values in the head
        if(newPad == 0){
            head->padded = 0;
        }
        else{
            head->padded = 1;
        }
        head->block_size = (sizeWithPad + 16)>>4;

        //make a new footer and update its values
        sf_footer *newFooter = (sf_footer*)((char*)head + sizeWithPad + 8);
        newFooter->block_size = (sizeWithPad + 16)>>4;
        newFooter->requested_size = size;
        newFooter->allocated = 1;
        newFooter->two_zeroes = 0;
        if(newPad == 0){
            newFooter->padded = 0;
        }
        else{
            newFooter->padded = 1;
        }

        //now update the new header and old footer after the newly alloc block
        sf_header *newHeader = (sf_header*)((char*)newFooter + 8);
        newHeader->unused = 0;
        newHeader->two_zeroes = 0;
        newHeader->allocated = 0;
        newHeader->padded = 0;

        if((void*)((char*)foot + 8) < get_heap_end()){
            sf_header *nextHead = (sf_header*)((char*)foot + 8);
            if(nextHead->allocated == 0){
                newHeader->block_size = (prevReqSize + padSize - sizeWithPad + (nextHead->block_size<<4))>>4;
                removeFromFreeList(nextHead);

            }else{
                newHeader->block_size = (prevReqSize + padSize - sizeWithPad)>>4;
            }
        }
        else{
            newHeader->block_size = (prevReqSize + padSize - sizeWithPad)>>4;
        }
        sf_footer *footer = (sf_footer*)((char*)newHeader + (newHeader->block_size<<4) - 8);
        makeFooter(footer, 0, 0, 0, newHeader->block_size<<4);
        addToFreeList(newHeader);

        return ptr;
    }
    else if(sizeWithPad < prevReqSize + padSize && prevReqSize + padSize - sizeWithPad == 16){
    //since return will cause a splinter, update val in header and footer and return.
        head->padded = 1;
        foot->padded = 1;
        foot->requested_size = size;
        return ptr;
    }
    //what the user asked for is GREATER than the currently allocated size.
    if(sizeWithPad > prevReqSize + padSize){
        void* newPtr = sf_malloc(size);
        if(newPtr == NULL){
            return NULL;
        }
        memcpy(newPtr, ptr, prevReqSize + padSize);
        sf_free(ptr);
        return newPtr;
    }
    return NULL;
}

void sf_free(void *ptr) {
    //Ptr is null, before or after heap, then abort
    if(!ptr || ptr < get_heap_start() || ptr > get_heap_end() ){
        abort();
    }
    sf_header *head = (sf_header*)((char*)ptr - 8);
    sf_footer *foot = (sf_footer*)((char*)head + (head->block_size <<4) - 8);

    if((void*)head < get_heap_start() || (void*)((char*)foot+8) > get_heap_end()){
        abort();
    }

    //Must be allocated
    if(head->allocated == 0 || foot->allocated == 0|| head->block_size<<4 < 32 || foot->block_size<<4 < 32){
        abort();
    }
    //head and foot must have same vals
    if(head->padded != foot->padded || head->block_size != foot->block_size){
        abort();
    }
    //Get the padding value
    int padSize = 0;
    if(16 -(foot->requested_size % 16) != 16){
        padSize = 16 -(foot->requested_size % 16);
    }
    //Make sure that padding is consistent
    if(((padSize == 0 && head->padded == 1)&& foot->requested_size + padSize + 32 != head->block_size<<4) || (padSize > 0 && head->padded == 0)){
        abort();
    }
    if(foot->requested_size + 16 != head->block_size<<4 && head->padded == 0){
        abort();
    }

    int blockSize = head->block_size << 4;

    //COALESCE check here
    if((void*)((char*)foot + 8) < get_heap_end()){
        sf_header *nextHead = (sf_header*)((char*)foot + 8);
        if(nextHead->allocated == 0){
            blockSize += nextHead->block_size << 4;

            //Delete the coalesced mem segment from the list.
            removeFromFreeList(nextHead);
        }
    }
    //Change the head and footer value
    head->block_size = blockSize >> 4;
    head->padded = 0;
    head->allocated = 0;
    sf_footer *editedFoot = (sf_footer*)((char*)head + blockSize - 8);
    editedFoot->block_size = blockSize >> 4;
    editedFoot->padded = 0;
    editedFoot->allocated = 0;
    editedFoot->requested_size = 0;

    addToFreeList(head);

    return;
}

void removeFromFreeList(sf_header* head){
    int a = 0;
    while(a < 4){
        sf_free_header *listPtr = seg_free_list[a].head;
        sf_free_header *listElement = listPtr;
        while(listElement != NULL){
            if(&listElement->header == head){
                if(listElement->prev == NULL && listElement->next == NULL){
                    seg_free_list[a].head = NULL;
                    break;
                }
                else if(listElement->prev == NULL && listElement->next != NULL){
                    seg_free_list[a].head = listElement->next;
                    listElement->next->prev = NULL;
                    break;
                }
                else if(listElement->prev != NULL && listElement->next != NULL){
                    listElement->prev->next = listElement->next;
                    listElement->next->prev = listElement->prev;
                    break;
                }
                else{
                    listElement->prev->next = NULL;
                    break;
                }
            }
            listElement = listElement->next;
        }
        a++;
    }
    return;
}

void addToFreeList(sf_header* head){
    int getListIndex = 0;
    //loop thru to find the correct list to place the head.
    while(getListIndex < 4){
        int listMin = seg_free_list[getListIndex].min;
        int listMax = seg_free_list[getListIndex].max;
        if((head->block_size << 4) >= listMin && (head->block_size << 4) <= listMax){
            break;
        }
        getListIndex++;
    }

    //Set the values for the free header that saves the remaining page head.
    sf_free_header *headerWrapper = (sf_free_header*)head;
    headerWrapper->header=*head;
    headerWrapper->prev = NULL;
    headerWrapper->next = NULL;

    //Change the prev val for the last head of the freelist only if
    //there is something in the list head right now.
    if(seg_free_list[getListIndex].head != NULL){
        headerWrapper->next = seg_free_list[getListIndex].head;
        headerWrapper->next->prev = headerWrapper;
    }

    //change the head of the freelist.
    seg_free_list[getListIndex].head = headerWrapper;
}
void makeFooter(sf_footer* newPageFooter, int alloc, int pad, int req_size, int block_size){
    newPageFooter->allocated = alloc;
    newPageFooter->padded = pad;
    newPageFooter->two_zeroes = 0;
    newPageFooter->requested_size = req_size;
    newPageFooter->block_size = block_size >> 4;
}
