#include "models.h"

MemoryRange::MemoryRange(uint16_t start, uint16_t end, uint8_t mode, const std::string &name){
    if (end < start){
        throw std::invalid_argument("memory range end < start");
    }
    this->start = start;
    this->end = end;
    if (mode & 0x1){
        executable = true;
    }
    else
    if (mode & 0x2){
        writeable = true;
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

void MemoryRange::access(MemoryTransaction *req){
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
//    throw std::domain_error;
}


int Memory::registerMemoryRange(MemoryRange *range){
    int ret = 0;
    uint16_t start = range->getStart();
    uint16_t end = range->getEnd();

    // check if the memory range overlaps with an existing one
    for (auto it = bounds.begin(); it != bounds.end(); it++){
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
    for (auto it = memranges.begin(); it != memranges.end(); it++){
        delete *it;
    }
}

void Memory::access(MemoryTransaction* req){
    uint16_t addr_hi = req->addr;
    uint16_t addr_lo = req->addr + req->size - 1;
    for (auto it = bounds.begin(); it != bounds.end(); it++){
        uint16_t bound_lo = it->first;
        uint16_t bound_hi = it->second;
        // linear search through an array
        // TODO -> to hashed search
        if (addr_hi >= bound_lo && addr_hi <= bound_hi){
            // the hightes byte (big-endian) is withing the range, check if the lowest one is not out of range
            if (addr_lo >= bound_lo && addr_hi <= bound_hi){
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
