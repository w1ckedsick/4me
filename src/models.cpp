#include "models.h"
#include <string>
#include <map>
#include <iostream>
#include <iomanip>
/*
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
}*/

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
    if (size > 4){
        throw std::invalid_argument("Memory request size must be <= 4");
    }

    if (req->iswrite){
        for (uint16_t i = 0; i < req->size; i++){
            memory[req->addr + i] = (uint8_t)((*req->buf >> ((size - i - 1)*8)) & 0xff);
        }
    }
    else{
        uint32_t buf = 0;
        for (uint16_t i = 0; i < req->size; i++){
            auto it = memory.find(req->addr + i);
            buf <<= 8;
            buf |= it == memory.end() ? (uint32_t)getUninitMem() : (uint32_t)it->second;
        }
        *req->buf = buf;
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
    if (log_en)
        std::cout << "FETCH: 0x" << std::setfill('0') << std::setw(4) << std::hex << (uint32_t)ip << std::endl;
    MemoryTransaction req = MemoryTransaction(ip, &fetched_instr, 4, 1, 0);
    memory->access(&req);
    // next instruction has 4-byte offset
    ip += 4;
}

/*
 * Exec stage (merged with decode, memory access and writeback, because sadly there's no pipeline)
 */
int Core::execute(){
    // decode the instruction
    uint8_t opc = 0;
    uint8_t rd_index = 0;
    uint8_t rs1_index = 0;
    uint8_t rs2_index = 0;
    uint16_t imm = 0;

    if (log_en)
        std::cout << "DECODE: 0x" << std::setfill('0') << std::setw(8) << std::hex << fetched_instr << std::endl;

    opc = (uint8_t)(fetched_instr >> 28);
    rd_index = (uint8_t)((fetched_instr >> 24) & 0xf);
    rs1_index = (uint8_t)((fetched_instr >> 20) & 0xf);
    rs2_index = (uint8_t)((fetched_instr >> 16) & 0xf);
    imm = (uint16_t)(fetched_instr & 0xffff);

    uint16_t dummy = 0;
    // a case of dedicated r0 which cannot be written, create a link to a dummy stack variable
    uint16_t &rd  = rd_index ? reg[rd_index] : dummy;
    uint16_t &rs1 = reg[rs1_index];
    uint16_t &rs2 = reg[rs2_index];

    if (log_en)
        std::cout << "EXECUTE: opc=0x" << std::hex << (uint32_t)opc
                  << ", dest=r" << std::dec << (uint32_t)rd_index 
                  << " 0x" << std::setfill('0') << std::setw(4) << std::hex << (uint32_t)rd //<< "]"
                  << ", src1=r" << std::dec << (uint32_t)rs1_index 
                  << " 0x" << std::setfill('0') << std::setw(4) << std::hex << (uint32_t)rs1 //<< "]"
                  << ", src2=r" << std::dec << (uint32_t)rs2_index 
                  << " 0x" << std::setfill('0') << std::setw(4) << std::hex << (uint32_t)rs2 //<< "]"
                  << ", imm=0x" << std::setfill('0') << std::setw(4) << std::hex << (uint32_t)imm << std::endl;
    

    // execute
    switch (opc){
        case 0x0:
            // ADD
            rd = rs1 + (rs2 | imm);
            break;
        case 0x1:
            // SUB
            rd = rs1 - (rs2 | imm);
            break;
        case 0x2:
            // MUL
            rd = (int16_t)rs1 * (int16_t)(rs2 | imm);
            break;
        case 0x3:
            // MODU
            rd = rs1 % (rs2 | imm);
            break;
        case 0x4:
            // DIV
            rd = (int16_t)rs1 / (int16_t)(rs2 | imm);
            break;
        case 0x5:
            // DIVU
            rd = rs1 / (rs2 | imm);
            break;
        case 0x6:
            // ORNOT
            rd = rs1 | ~(rs2 | imm);
            break;
        case 0x7:
            // AND
            rd = rs1 & (rs2 | imm);
            break;
        case 0x8:
            // LSL
            rd = rs1 << (rs2 | imm);
            break;
        case 0x9:
            // LSR
            rd = rs1 >> (rs2 | imm);
            break;
        case 0xa:
            // ASR
            rd = (int16_t)rs1 >> (rs2 | imm);
            break;
        case 0xb:
            // CMP
            switch (imm){
                case 0x0:
                    // EQ
                    rd = rs1 == rs2 ? 0 : 1;
                    break;
                case 0x1:
                    // NE
                    rd = rs1 != rs2 ? 0 : 1;
                    break;
                case 0x2:
                    // BN
                    rd = rs1 & rs2 ? 0 : 1;
                    break;
                case 0x3:
                    // BS
                    rd = !(rs1 & rs2) ? 0 : 1;
                    break;
                case 0x4:
                    // LS
                    rd = (int16_t)rs1 < (int16_t)rs2 ? 0 : 1;
                    break;
                case 0x5:
                    // GT
                    rd = (int16_t)rs1 > (int16_t)rs2 ? 0 : 1;
                    break;
                case 0x6:
                    // GE
                    rd = (int16_t)rs1 >= (int16_t)rs2 ? 0 : 1;
                    break;
                case 0x7:
                    // LE
                    rd = (int16_t)rs1 <= (int16_t)rs2 ? 0 : 1;
                    break;
                case 0x8:
                    // BL
                    rd = rs1 < rs2 ? 0 : 1;
                    break;
                case 0x9:
                    // AB
                    rd = rs1 > rs2 ? 0 : 1;
                    break;
                case 0xa:
                    // BE
                    rd = rs1 <= rs2 ? 0 : 1;
                    break;
                case 0xb:
                    // AE
                    rd = rs1 >= rs2 ? 0 : 1;
                    break;
                default:
                    return 2;
                    break;
            }
            break;
        case 0xc:
            // BRN
            rd = ip + 4;
            if (!rs1){
                uint16_t jump_dst = rs2 | imm;
                if (jump_dst & 0x3)
                    // Got HALT
                    return 1;
                else
                    jump(jump_dst);
            }
            break;
        case 0xd:{
            // LD
            uint32_t buf; //TODO
            MemoryTransaction req = MemoryTransaction(imm + rs1 + rs2, (uint32_t*)&buf/*&rd*/, 2, 0, 0);
            memory->access(&req);
            rd = (uint16_t)(buf & 0xffff);
            if (log_en)
                std::cout << "WRITEBACK: r" << std::dec << (uint32_t)rd_index << " <- [0x" 
                          << std::setfill('0') << std::setw(4) << std::hex << (uint32_t)req.addr << "] = 0x"
                          << std::setfill('0') << std::setw(4) << std::hex << (uint32_t)rd << std::endl;
            break;
        }
        case 0xe:{
            // ST
            uint32_t buf = (uint32_t)rd; //TODO
            MemoryTransaction req = MemoryTransaction(imm + rs1 + rs2, (uint32_t*)&buf/*&rd*/, 2, 0, 1);
            memory->access(&req);
            if (log_en)
                std::cout << "WRITEBACK: [0x" << std::setw(4) << std::hex << (uint32_t)req.addr << "] <- 0x" 
                          << std::setfill('0') << std::setw(4) << std::hex << (uint32_t)rd << std::endl;
            break;
        }
        default:
            return 2;
            break;
    }

    if (log_en && (opc < 0xd))
        std::cout << "WRITEBACK: r" << std::dec << (uint32_t)rd_index << " <- 0x" 
                  << std::setfill('0') << std::setw(4) << std::hex << (uint32_t)rd << std::endl;


    return 0;
}

/*
 * Prints core's register file
 */
void Core::printRegFile(){
    std::cout << "-====== Register File ======-" << std::endl;
    for (size_t i = 0; i < 16; ++i){
        std::cout << "r" << i
                  << ":  0x" << std::setfill('0') << std::setw(4) << std::hex << (uint32_t)reg[i] 
                  << std::endl;
        
    }
    std::cout << "-===========================-" << std::endl;
}
