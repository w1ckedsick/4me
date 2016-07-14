#ifndef MODELS_H
#define MODELS_H
#include <vector>
#include <unordered_map>
#include <string>

class MemoryTransaction{
    public:
        // reference address
        uint16_t& addr;
        // data storage (either to store in or to load from), the type is the largest possible
        uint32_t *buf;
        // size of the request, in bytes
        uint8_t size;
        // flag if the memory is going to be executed, shall be considered only for reads
        bool exec;
        // flag is the request is a write one
        bool iswrite;

        MemoryTransaction(uint16_t& addr, uint32_t *buf, uint8_t size, bool exec, bool iswrite) :
            addr(addr),
            buf(buf),
            size(size),
            exec(exec), 
            iswrite(iswrite)
            {};

        ~MemoryTransaction() {};
};

class MemoryRange{
    private:
        std::string name;
        bool readable;
        bool writeable;
        bool executable;
        uint16_t start;
        uint16_t end;
        std::unordered_map<uint16_t, uint8_t> memory;
    public:
        unsigned int size;

        MemoryRange(uint16_t start, uint16_t end, uint8_t mode, const std::string &name);

        uint16_t getStart();
        uint16_t getEnd();

        void access(MemoryTransaction *req);

        inline uint8_t getUninitMem();

        ~MemoryRange();
};


class Memory{
    private:
        std::vector<MemoryRange*> memranges;
        // an array of [(x.start, x.end) for x in memranges]
        std::vector< std::pair<int, int> > bounds;
    public:
        Memory() {};

        int registerMemoryRange(MemoryRange* range);
        int unregisterMemoryRange(MemoryRange* range);

        void access(MemoryTransaction *req);

        void memoryDump();

        ~Memory();
};

//Memory::memoryDump(){
//    for it
//}


class Cpu{
};

#endif
