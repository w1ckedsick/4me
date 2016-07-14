#include "simul.h"
#include "models.h"
#include <iostream>
#include <fstream>

const bool DEBUG = true;

/*
 * Read 2 bytes from a file
 */
int readParam(std::ifstream& infile, uint16_t& param){
    char byte;
    if (infile.get(byte)){
        param = (uint16_t)byte << 8;
    }
    else{
        return 1;
    }
    if (infile.get(byte)){
        param |= (uint16_t)byte;
    }
    else{
        return 1;
    }
    return 0;
}

/*
 * Parse the given binary file and fill the simulator data classes accordingly
 */
int parseInput(Memory& memory){
    char byte;
    int index = 0;
    int ret = 0;
    char header[4] = {0};
    std::ifstream infile("input", std::ios::binary);
    uint16_t code_sz = 0;
    uint16_t cdata = 0;
    uint16_t cdata_sz = 0;
    uint16_t smc = 0;
    uint16_t data = 0;
    uint16_t data_sz = 0;
    uint16_t mem = 0;
    uint16_t dbg_sz = 0;


    // reading the filetype=header string
    while (index < 4){
        if (infile.get(byte)){
            header[index] = byte;
            index++;
        }
        else{
            break;
        }
    }
    if ((header[0] != 'T') || (header[1] != 'o') || (header[2] != 'y') || (header[3] != '1') || (index != 4)){
        std::cout << "Wrong input file format =" << header << std::endl;
        infile.close();
        return 1;
    }

    // read 8 control fields 2 bytes each
    ret += readParam(infile, code_sz);
    ret += readParam(infile, cdata);
    ret += readParam(infile, cdata_sz);
    ret += readParam(infile, smc);
    ret += readParam(infile, data);
    ret += readParam(infile, data_sz);
    ret += readParam(infile, mem);
    ret += readParam(infile, dbg_sz);
    if (ret != 0){
        infile.close();
        std::cout << "Control section cannot be read" << std::endl;
        return 1;
    }

    // look what we've read so far
    if (DEBUG){
        std::cout << " header   = "   << header << std::endl
                  << " code_sz  = "   << std::dec << code_sz  << std::endl
                  << " cdata    = 0x" << std::hex << cdata    << std::endl
                  << " cdata_sz = "   << std::dec << cdata_sz << std::endl
                  << " smc      = 0x" << std::hex << smc      << std::endl
                  << " data     = 0x" << std::hex << data     << std::endl
                  << " data_sz  = "   << std::dec << data_sz  << std::endl
                  << " mem      = 0x" << std::hex << mem      << std::endl
                  << " dbg_sz   = "   << std::dec << dbg_sz   << std::endl;
    }

    // sanity and parameters pre-requirements check

    // non-zero code section, shall 4-bytes aligned
    if (code_sz == 0){
        std::cout << "code_sz cannot be zero" << std::endl;
        infile.close();
        return 1;
    }
    if (code_sz % 4){
        std::cout << "code_sz shall be 4-bytes aligned" << std::endl;
        infile.close();
        return 1;
    }

    // couple of extra flags, showing that the section shall be created (non-zero)
    bool cdata_nz = cdata != 0;
    bool smc_nz = smc != 0;
    bool data_nz = data != 0;
    bool mem_nz = mem != 0;

    // sanity check
    if (!cdata_nz && cdata_sz != 0){
        std::cout << "cdata_sz cannot be non-zero for cdata = 0" << std::endl;
        infile.close();
        return 1;
    }
    if (!data_nz && data_sz != 0){
        std::cout << "data_sz cannot be non-zero for data = 0" << std::endl;
        infile.close();
        return 1;
    }

    // check for monotonous section address raise
    if (smc_nz){
        if (smc <= cdata){
            std::cout << "smc must be > cdata" << std::endl;
            infile.close();
            return 1;
        }
    }
    if (data_nz){
        if (data <= smc){
            std::cout << "data must be > smc" << std::endl;
            infile.close();
            return 1;
        }
        if (data <= cdata){
            std::cout << "data must be > cdata" << std::endl;
            infile.close();
            return 1;
        }
    }
    if (mem_nz){
        if (mem <= data){
            std::cout << "mem must be > data" << std::endl;
            infile.close();
            return 1;
        }
        if (mem <= smc){
            std::cout << "mem must be > smc" << std::endl;
            infile.close();
            return 1;
        }
        if (mem <= cdata){
            std::cout << "mem must be > cdata" << std::endl;
            infile.close();
            return 1;
        }
    }

    // check section sizes and their representaions in the file
    uint16_t section_data_size = 0;
    uint16_t section_code_size = 0;
    uint16_t section_cdata_size = 0;
    
    section_data_size = mem_nz ? mem - data : 0xf000 - data;
    section_cdata_size = smc_nz ? smc - cdata : data_nz ? data - cdata : mem_nz ? mem - cdata : 0xf000 - cdata;
    section_code_size = cdata_nz ? cdata - 4 : smc_nz ? smc - 4 : data_nz ? data - 4 : mem_nz ? mem - 4 : 0xeffc;

    if (section_data_size < data_sz){
        std::cout << "data_sz is more than the actual section size = " << section_data_size << std::endl;
        infile.close();
        return 1;
    }

    if (section_code_size < code_sz){
        std::cout << "code_sz is more than the actual code size = " << section_code_size << std::endl;
        infile.close();
        return 1;
    }

    if (section_cdata_size < cdata_sz){
        std::cout << "cdata_sz is more than the actual section size = " << section_cdata_size << std::endl;
        infile.close();
        return 1;
    }



    // as we've got to this point, everyting shall be correct
    // create described memory regions and add them to the memory map
    ret = 0;
    MemoryRange* range;

    // dedicated first segment
    range = new MemoryRange(0, 3, 0, "reserved");
    ret += memory.registerMemoryRange(range);

    // code section
    range = new MemoryRange(4, cdata - 1, 5, "code");
    ret += memory.registerMemoryRange(range);

    // constant data section
    range = new MemoryRange(cdata, smc - 1, 4, "cdata");
    ret += memory.registerMemoryRange(range);

    // aux code section
    range = new MemoryRange(smc, data - 1, 4, "smc");
    ret += memory.registerMemoryRange(range);

    // data section
    range = new MemoryRange(data, mem - 1, 4, "data");
    ret += memory.registerMemoryRange(range);

    // heap section
    range = new MemoryRange(mem, 0xefff, 4, "heap");
    ret += memory.registerMemoryRange(range);

    // I/O section
    range = new MemoryRange(0xf000, 0xffff, 4, "i/o");
    ret += memory.registerMemoryRange(range);

    if (ret){
        std::cout << "There were errors while registering memory regions" << std::endl;
        infile.close();
        return 1;
    }


    // fill the memory with pre-defined data from the file
    uint16_t addr = 0;

    // code section
    index = 0;
    addr = 0x4;
    MemoryTransaction req = MemoryTransaction(addr, (uint32_t*)&byte, 1, 1, 1);
    
/*    while (index < code_sz){
        if (infile.get(byte)){
            memory.access(&req);
            addr++;
            index++;
        }
        else{
            break;
        }
    }*/

    infile.close();
    return 0;    
}

int main(){
    Memory mem = Memory();
    parseInput(mem);
}
