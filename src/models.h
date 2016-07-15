#ifndef MODELS_H
#define MODELS_H
#include <vector>
#include <unordered_map>
#include <string>
#include <array>

class MemoryTransaction{
    public:
        // reference address
        uint16_t addr;
        // data storage (either to store in or to load from), the type is the largest possible
        uint32_t *buf;
        // size of the request, in bytes
        uint8_t size;
        // flag if the memory is going to be executed, shall be considered only for reads
        bool exec;
        // flag is the request is a write one
        bool iswrite;

        MemoryTransaction(uint16_t addr, uint32_t *buf, uint8_t size, bool exec, bool iswrite) :
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
        bool special;
        uint16_t start;
        uint16_t end;
        std::unordered_map<uint16_t, uint8_t> memory;
    public:
        MemoryRange(uint16_t start, uint16_t end, uint8_t mode, const std::string &name) :
            name(name),
            readable(mode & 0x4),
            writeable(mode & 0x2),
            executable(mode & 0x1),
            special(mode & 0x8),
            start(start),
            end(end)
            {};
            

        uint16_t getStart();
        uint16_t getEnd();
        std::string getName() {return name;};

        void access(MemoryTransaction *req);
        void directAccess(MemoryTransaction *req);
        void checkAccessPermissions(MemoryTransaction *req);

        void memoryDump();

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
        MemoryRange* getRangeByName(std::string name);

        void access(MemoryTransaction *req);

        void memoryDump();

        ~Memory();
};


class Core{
    private:
        // instruction pointer - next instruction to fetch
        // and since there's no pipeline (nor instruction buffer) - to decode and execute too
        uint16_t ip;
        // fetch stage result
        // actually, if for some reason I decided to make more pseudo-stages, 
        // it would be better to keep here every intermediate cross-stage results
        uint32_t fetched_instr;
        // associated memory
        Memory *memory;
        // register file
        std::array<uint16_t, 16> reg;
        // logging is enabled = verbose execution
        bool log_en;
    public:
        // code entry point is set up in the constructor
        Core(uint16_t ip, bool log_en) : ip(ip), fetched_instr(0xffffffff), reg({0}), log_en(log_en) {};
        
        void bindMemory(Memory* memory);

        void fetch();
        int execute();

        // jumps to an instruction
        void jump(uint16_t dst) {ip = dst;};

        void printRegFile();

        ~Core() {};
};

#endif
