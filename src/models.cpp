#include "models.h"
#include <string>
#include <map>
#include <iostream>
#include <iomanip>

MemoryRange::MemoryRange(uint16_t start, uint16_t end, uint8_t mode, const std::string &name){
    if (end < start){
        throw std::invalid_argument("memory range end < start");
    }
    this->start = start;
    this->end = end;
    executable = false;
    writeable = false;
    executable = false;
    special = false;
    if (mode & 0x1){
        executable = true;
    }
    if (mode & 0x2){
        writeable = true;
    }
    if (mode & 0x4){
        readable = true;
    }
    if (mode & 0x8){
        special = true;
    }
    this->name = name;
}

uint16_t MemoryRange::getStart(){
    return start;
}

uint16_t MemoryRange::getEnd(){
    return end;
}

MemoryRange::~MemoryRange(){
    
}

inline uint8_t MemoryRange::getUninitMem(){
    return 0xff;
}

void MemoryRange::checkAccessPermissions(MemoryTransaction *req){
    // check if access can granted, cases are obvious
    if (special){
        // since the i/o memory request have some special access rules we don't know,
        // allow any i/o request. Override this method in case of I/O ranges inheritance

    }
    else{
        if (req->iswrite && !writeable){
            throw std::domain_error("Memory write is not granted");
        }
        if (!(req->iswrite) && !readable){
            throw std::domain_error("Memory read is not granted");
        }
        if (!(req->iswrite) && req->exec && !executable){
            throw std::domain_error("Instruction fetch is not granted");
        }
    }
    
}

void MemoryRange::directAccess(MemoryTransaction *req){
    // access to a memory cell with no respect to permissions
    uint8_t size = req->size;
    if (size == 1){
        if (req->iswrite){
            memory[req->addr] = (uint8_t)((*req->buf) & 0xff);
        }
        else{
            auto it = memory.find(req->addr);
            *req->buf = it == memory.end() ? (uint32_t)getUninitMem() : (uint32_t)it->second;
        }
    }
    else
    if (size == 2){
        if (req->iswrite){
            memory[req->addr] = (uint8_t)((*req->buf >> 8) & 0xff);
            memory[req->addr + 1] = (uint8_t)(*req->buf & 0xff);
        }
        else{
            std::unordered_map<uint16_t, uint8_t>::const_iterator it;
            uint32_t buf;
            it = memory.find(req->addr);
            buf = it == memory.end() ? (uint32_t)getUninitMem() : (uint32_t)it->second;
            it = memory.find(req->addr + 1);
            buf <<= 8;
            buf += it == memory.end() ? (uint32_t)getUninitMem() : (uint32_t)it->second;
            *req->buf = buf;
        }
    }
    else
    if (size == 4){
        if (req->iswrite){
            memory[req->addr] = (uint8_t)((*req->buf >> 24) & 0xff);
            memory[req->addr + 1] = (uint8_t)((*req->buf >> 16) & 0xff);
            memory[req->addr + 2] = (uint8_t)((*req->buf >> 8) & 0xff);
            memory[req->addr + 3] = (uint8_t)(*req->buf & 0xff);
        }
        else{
            std::unordered_map<uint16_t, uint8_t>::const_iterator it;
            uint32_t buf;
            it = memory.find(req->addr);
            buf = it == memory.end() ? (uint32_t)getUninitMem() : (uint32_t)it->second;
            it = memory.find(req->addr + 1);
            buf <<= 8;
            buf += it == memory.end() ? (uint32_t)getUninitMem() : (uint32_t)it->second;
            it = memory.find(req->addr + 2);
            buf <<= 8;
            buf += it == memory.end() ? (uint32_t)getUninitMem() : (uint32_t)it->second;
            it = memory.find(req->addr + 3);
            buf <<= 8;
            buf += it == memory.end() ? (uint32_t)getUninitMem() : (uint32_t)it->second;
            *req->buf = buf;
        }
    }
    else{
        throw std::invalid_argument("Size shall be 1,2 or 4");
    }
}

// access to a memory cell with all the respect to permissions
void MemoryRange::access(MemoryTransaction *req){
    checkAccessPermissions(req);
    directAccess(req);
}


int Memory::registerMemoryRange(MemoryRange *range){
    int ret = 0;
    uint16_t start = range->getStart();
    uint16_t end = range->getEnd();

    // check if the memory range overlaps with an existing one
    for (auto it = bounds.begin(); it != bounds.end(); ++it){
        // there're 6 cases of intervals positioning, only 2 of them are acceptable
        if (!((it->first < start && it->second < end) || (it->first > end && it->second > end))){
            ret = 1;
            break;
        }
    }
    try{
        this->bounds.emplace_back(std::pair<int, int>(start, end));
        this->memranges.push_back(range);
    }
    catch (std::bad_alloc& e){
        // cannot allocate a vector element, handle it softly
        ret = 2;
    }
    return ret;
}

Memory::~Memory(){
    for (auto it = memranges.begin(); it != memranges.end(); ++it){
        delete *it;
    }
}

MemoryRange* Memory::getRangeByName(std::string name){
    for (auto it = memranges.begin(); it != memranges.end(); ++it){
        if (!name.compare((*it)->getName())){
            return *it;
        }
    }
    return nullptr;
}

void Memory::access(MemoryTransaction* req){
    uint16_t addr_hi = req->addr;
    uint16_t addr_lo = req->addr + req->size - 1;
    for (auto it = bounds.begin(); it != bounds.end(); ++it){
        uint16_t bound_lo = it->first;
        uint16_t bound_hi = it->second;
        // linear search through an array
        // TODO -> to hashed search
        if (addr_hi >= bound_lo && addr_hi <= bound_hi){
            // the hightes byte (big-endian) is withing the range, check if the lowest one is not out of range
            if ((addr_lo >= bound_lo) && (addr_lo <= bound_hi)){
                // all clear, access the range with corresponding index
                memranges.at(it - bounds.begin())->access(req);
                return;
            }
            else{
                throw std::out_of_range("memory request is out of map region boundaries");
            }
        }
    }
    throw std::out_of_range("memory request to nowhere");
}

void Memory::memoryDump(){
    // quick (in terms of code complexity) implementation just for debugging purposes (if needed)
    // requites a ton of time and extra memory
    std::unordered_map<uint16_t, MemoryRange*> bound_range_unordered;
    for (auto it = bounds.begin(); it != bounds.end(); ++it){
        bound_range_unordered[it->first] = memranges.at(it - bounds.begin());
    }
    std::map<uint16_t, MemoryRange*> bound_range_ordered(bound_range_unordered.begin(), bound_range_unordered.end());
    
    std::cout << "-====== MEMORY DUMP ======-" << std::endl;
    for(auto it = bound_range_ordered.begin(); it != bound_range_ordered.end(); ++it){
        // for each region from the very bottom of the memory pool in the ascending order
        // print the mem dump
        std::cout << "=== " << it->second->getName() << " ===" << std::endl;
        std::cout << "0x" << std::setfill('0') << std::setw(4) << std::hex
                  << it->second->getStart() << " ---- section start " << std::endl;
        it->second->memoryDump();
        std::cout << "0x" << std::setfill('0') << std::setw(4) << std::hex
                  << it->second->getEnd() << " ---- section end " << std::endl;
    }
    std::cout << "-=========================-" << std::endl;
}

void MemoryRange::memoryDump(){
    // debugging purposes only, prints all the used memory in a memory range in an ascending order
    std::map<uint16_t, uint8_t> ordered_memory(memory.begin(), memory.end());
    for(auto it = ordered_memory.begin(); it != ordered_memory.end(); ++it){
        std::cout << "0x" << std::setfill('0') << std::setw(4) << std::hex << it->first
                  << ":  0x" << std::setfill('0') << std::setw(2) << std::hex << (uint32_t)it->second 
                  << std::endl;
    }
}

/*
 * Created a relation between a memory subsystem and a core - 
 * every core's request will be fed to the bound memory
 */
void Core::bindMemory(Memory* memory){
    this->memory = memory;
}

/*
 * Get the next instruction to execute
 */
void Core::fetch(){
    MemoryTransaction req = MemoryTransaction(ip, &fetched_instr, 4, 1, 0);
    memory->access(&req);
}

/*
 * Exec stage (merged with decode, memory access and writeback, because sadly there's no pipeline)
 */
void Core::execute(){
    // decode the instruction
}
