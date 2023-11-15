#include <stddef.h>
#include <functional>
#include <map>
#include <vector>

using namespace std;

int bestFit(int sizeInWords, void *list);

int worstFit(int sizeInWords, void *list);

class MemoryManager
{

private:
    bool active = false;

    // data about the memory array
    uint16_t numWords;
    uint8_t wordSize;

    // make array to store bitmap data
    uint8_t *bitmap = nullptr;
    // array to store hole data
    uint16_t *holeList = nullptr;

    map<int, int> holes;
    map<int, int> blocks;

    // array to store the whole memory array
    uint8_t *memoryArray;

    // functio that is used to find the memory location in the memory block that will be used for adding in new data
    function<int(int, void *)> defaultAllocator;

public:
    uint16_t getOffset(uint64_t address);
    uint64_t revertOffset(uint16_t offset);
    uint8_t mirrorByte(uint8_t byte);

    MemoryManager(unsigned _wordSize, function<int(int, void *)> allocator);
    ~MemoryManager();
    void initialize(size_t sizeInWords);
    void shutdown();
    void *allocate(size_t sizeInBytes);
    void free(void *address);
    void setAllocator(function<int(int, void *)> allocator);
    int dumpMemoryMap(char *filename);
    void *getList();
    void *getBitmap();
    unsigned getWordSize();
    void *getMemoryStart();
    unsigned getMemoryLimit();
};