#include "MemoryManager.h"
#include <iostream>
#include <queue>
#include <fstream>
#include <cmath>
#include <stack>
#include <vector>
#include <fcntl.h>
#include <unistd.h>

using namespace std;

int bestFit(int sizeInWords, void *list)
{
    // cast the list into the proper data type
    uint16_t *List = (uint16_t *)list;

    // get the size of the list entries
    int size = List[0];

    uint16_t currBest = INT16_MAX, offset = -1;
    for (int i = 1; i < size * 2; i += 2)
    {
        if (List[i + 1] - sizeInWords < currBest && List[i + 1] >= 0)
        {
            offset = List[i];
            currBest = List[i + 1] - sizeInWords;
        }
    }

    return offset;
}

int worstFit(int sizeInWords, void *list)
{
    uint16_t *List = (uint16_t *)list;

    // get the size of the list entries
    int size = List[0];

    int currWorst = -1, index = -1;
    for (int i = 1; i < size * 2; i += 2)
    {
        int currGap = List[i + 1] - sizeInWords;
        if (currGap > currWorst)
        {
            index = List[i];
            currWorst = List[i + 1] - sizeInWords;
        }
    }

    return index;
}

MemoryManager::MemoryManager(unsigned _wordSize, function<int(int, void *)> allocator)
{
    // cout << "Making the manager..." << endl;
    defaultAllocator = allocator;
    wordSize = _wordSize;
}

MemoryManager::~MemoryManager()
{
    // cout << "Deleting the manager..." << endl;

    // clear all dynamic data
    if (memoryArray != nullptr)
        std::free(memoryArray);

    holes.clear();
    blocks.clear();
}

void MemoryManager::initialize(size_t sizeInWords)
{
    // cout << "Initializing the manager..." << endl;

    numWords = sizeInWords;

    // clear all dynamic data
    std::free(memoryArray);
    holes.clear();
    blocks.clear();

    // reinitialize all of the data for the manager
    memoryArray = (uint8_t *)malloc(numWords * wordSize);

    holes.emplace(0, sizeInWords);

    active = true;
}

void MemoryManager::shutdown()
{
    // cout << "Shutting down..." << endl;
    //  clear all dynamic data
    if (memoryArray != nullptr)
        std::free(memoryArray);

    memoryArray = nullptr;
    holes.clear();
    blocks.clear();

    active = false;
}

void *MemoryManager::allocate(size_t sizeInBytes)
{
    // cout << "Allocating..." << endl;

    if (!active)
        return nullptr;

    // need to find the index of the place in memory we can store this data

    // transform the sizeofbytes to sizeofwords
    int sizeInWords = sizeInBytes / wordSize;

    // cout << sizeInWords << endl;
    uint16_t *List = (uint16_t *)getList();
    // use the default allocator and see the value that it returns as an offset - this will return the index of a hole
    int offset = defaultAllocator(sizeInWords, List);

    delete[] List;

    // cout << offset << endl;

    if (offset > numWords || offset < 0)
        return nullptr;

    // update the hole map
    // make a hole after the new data that was just place in
    // find offset of the first index after the new data

    int holeOffset = offset + sizeInWords;

    // cout << " going with index at " << offset << " for " << sizeInWords << endl;

    // store an int of the current size of the hole
    int sizeOfHole = holes.at(offset);
    int newSizeOfHole = sizeOfHole - sizeInWords;

    // make a new entry and delete the old one
    if (holeOffset != numWords)
        holes.emplace(holeOffset, newSizeOfHole);
    holes.erase(offset);

    // now update the blocks map
    blocks.emplace(offset, sizeInWords);

    return &memoryArray[offset * wordSize];
}

void MemoryManager::free(void *address)
{
    // cout << "Freeing..." << endl;

    if (!active)
        return;

    // get the offset of the address
    int offset = getOffset((uint64_t)address);

    if (offset > numWords || offset < 0)
        return;

    // delete the old memory block from the blocks map
    int blockSize = blocks.at(offset);
    blocks.erase(offset);

    int totalHoleAddition = blockSize;

    // see if the block ahead of the new hole needs to be collapsed back

    bool extendHole = false;
    int bufferOffset = offset;

    while (true)
    {
        if (blocks.find(bufferOffset) != blocks.end())
        {
            break;
        }
        else if (holes.find(bufferOffset) != holes.end())
        {
            extendHole = true;
            break;
        }

        bufferOffset++;

        if (bufferOffset == INT16_MAX)
            break;
    }

    // if extendHole then add to the hole addition and collapse the hole back
    if (extendHole)
    {
        totalHoleAddition += holes.at(bufferOffset);
        holes.erase(bufferOffset);
    }

    // find out if making the offset into a hole requires an extension of a previous hole

    extendHole = false;
    bufferOffset = offset;

    while (true)
    {
        if (blocks.find(bufferOffset) != blocks.end())
        {
            break;
        }
        else if (holes.find(bufferOffset) != holes.end())
        {
            extendHole = true;
            break;
        }

        bufferOffset--;

        if (bufferOffset < 0)
            break;
    }

    // if extent hold then add to the total addition
    if (extendHole)
    {
        // cout << "Found backing hole" << endl;
        holes.at(bufferOffset) += totalHoleAddition;
    }
    // else the back block is used memory so we make a new hole
    else
    {
        holes.emplace(offset, totalHoleAddition);
    }
}

void MemoryManager::setAllocator(function<int(int, void *)> allocator)
{
    defaultAllocator = allocator;
}

int MemoryManager::dumpMemoryMap(char *filename)
{
    if (!active)
        return -1;

    int file = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0777);

    uint16_t *List = (uint16_t *)getList();

    string message = "";

    for (int i = 1; i < List[0] * 2; i += 2)
    {
        if (i != 1)
            message += " - ";
        message += "[" + to_string(List[i]) + ", " + to_string(List[i + 1]) + "]";
    }

    write(file, message.c_str(), message.length());

    close(file);

    delete[] List;

    return 0;
}

void *MemoryManager::getList()
{
    if (!active)
        return nullptr;

    for (auto i : holes)
    {
        if (i.second == 0)
        {
            holes.erase(i.first);
        }
    }

    holeList = new uint16_t[1 + holes.size() * 2];
    holeList[0] = holes.size();
    int index = 1;

    for (auto i : holes)
    {
        // cout << "Hole at: " << i.first << "   ";

        holeList[index++] = i.first;

        // cout << "sizeof " << i.second << "   " << endl;

        holeList[index++] = i.second;
    }
    return holeList;
}

void *MemoryManager::getBitmap()
{
    // cout << "Getting the bitmap..." << endl;
    if (!active)
        return nullptr;

    for (auto i : holes)
    {
        if (i.second == 0)
        {
            holes.erase(i.first);
        }
    }

    // make a vector with all of the data that needs to be stored in the bitmap
    vector<pair<int, int>> data;
    auto hP = holes.begin();
    auto bP = blocks.begin();
    while (hP != holes.end() && bP != blocks.end())
    {
        if (hP->first < bP->first)
        {
            data.push_back({0, hP->second});
            hP++;
        }
        else
        {
            data.push_back({1, bP->second});
            bP++;
        }
    }

    while (hP != holes.end())
    {
        data.push_back({0, hP->second});
        hP++;
    }
    while (bP != blocks.end())
    {
        data.push_back({1, bP->second});
        bP++;
    }

    // make a second vector to store the bytedata for the bitmap
    vector<uint8_t> bitM;
    int adds = 0;
    uint8_t currByte = 0;
    bool newByte = false;

    // push back the size of the vector data
    bitM.push_back(0);
    bitM.push_back(0);

    // cout << data.size() << endl;

    for (auto i : data)
    {
        // cout << i.first << " for " << i.second << endl;
        for (int j = 0; j < i.second; j++)
        {
            currByte += i.first;
            adds++;
            // cout << i.first;
            if (adds < 8)
            {
                newByte = true;
                currByte *= 2;
            }
            else
            {
                adds = 0;
                // mirror the byte
                currByte = mirrorByte(currByte);
                bitM.push_back(currByte);
                currByte = 0;
                newByte = false;
                // cout << endl;
            }
        }
    }

    for (int i = adds; i < 8; i++)
    {
        currByte *= 2;
    }

    if (newByte)
    {
        currByte = mirrorByte(currByte);
        bitM.push_back(currByte);
    }

    // change the size of the starting two ints in the dataM
    uint16_t size = bitM.size() - 2;

    uint8_t upper8 = size & 65280;
    uint8_t lower8 = size & 255;

    bitM[0] = lower8;
    bitM[1] = upper8;

    bitmap = new uint8_t[bitM.size()];

    int index = 0;
    for (auto i : bitM)
    {
        // cout << unsigned(i) << "  ";
        bitmap[index++] = i;
    }

    return bitmap;
}

uint8_t MemoryManager::mirrorByte(uint8_t byte)
{
    uint8_t mirroredByte = 0;

    for (int i = 0; i < 8; ++i)
    {
        // Shift the mirroredByte to the left
        mirroredByte <<= 1;

        mirroredByte |= (byte & 1);

        // Shift the original byte to the right to get the next bit
        byte >>= 1;
    }

    return mirroredByte;
}

unsigned MemoryManager::getWordSize()
{
    if (!active)
        return -1;

    return wordSize;
}

void *MemoryManager::getMemoryStart()
{
    if (!active)
        return nullptr;

    return &memoryArray[0];
}

unsigned MemoryManager::getMemoryLimit()
{
    if (!active)
        return -1;
    return numWords * wordSize;
}

uint16_t MemoryManager::getOffset(uint64_t address)
{
    // store the address of the first pointer in the memory array
    uint64_t firstAddress = (uint64_t)getMemoryStart();

    // get how far from that initial address the passed in address is
    uint64_t offset = address - firstAddress;

    // now get how many words can fit in that gap
    offset = offset / wordSize;

    // cout << offset << endl;

    return offset;
}

uint64_t MemoryManager::revertOffset(uint16_t offset)
{
    // convert the offset to the respective size in bytes
    uint16_t bytesOffset = offset * wordSize;

    // add that offset to the starting array address and return that
    return (uint64_t)getMemoryStart() + bytesOffset;
}
