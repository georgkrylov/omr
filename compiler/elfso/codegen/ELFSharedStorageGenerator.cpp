/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "codegen/ELFGenerator.hpp"
#include "infra/Assert.hpp"

#if defined(LINUX)

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <elf.h>
#include <dlfcn.h>

#include "env/CompilerEnv.hpp"
#include "control/Options.hpp"
#include "infra/STLUtils.hpp"
#include "control/Options_inlines.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "env/AOTMethodHeader.hpp"


ELF::ELFSharedObjectGenerator::ELFSharedObjectGenerator(TR::RawAllocator rawAllocator,
                            uint8_t const * codeStart, size_t codeSize):
                            OMR::ELFSharedObjectGeneratorConnector(rawAllocator, codeStart, codeSize)//,  OMR::AOTStorageInterfaceConnector(), ELFDataMap{}
                            {
                                initialize();
                            }

 ELF::ELFSharedObjectGenerator::ELFSharedObjectGenerator(TR::RawAllocator rawAllocator):
                            OMR::ELFSharedObjectGeneratorConnector(rawAllocator, 0, 0),  OMR::AOTStorageInterfaceConnector(), ELFDataMap(str_comparator)
                            {
                                initialize();
                            } 

void
ELF::ELFSharedObjectGenerator::initialize(void)
{
   // printf("\n In initialize\n");

    ELFEHeader *hdr =
    static_cast<ELFEHeader *>(_rawAllocator.allocate(sizeof(ELFEHeader),
    std::nothrow));
    _header = hdr;

    initializeELFHeader();
    
    initializeELFHeaderForPlatform();

}


void 
ELF::ELFSharedObjectGenerator::initializeAotCDSection(uint32_t shName, ELFAddress shAddress,
                                                 ELFOffset shOffset, uint32_t shSize)
{

    ELFSectionHeader * shdr = static_cast<ELFSectionHeader *>(_rawAllocator.allocate(sizeof(ELFSectionHeader)));
    
    shdr->sh_name = shName;
    shdr->sh_type = SHT_PROGBITS;
    shdr->sh_flags = SHF_ALLOC | SHF_WRITE | SHF_EXECINSTR ;//SHF_ALLOC | SHF_EXECINSTR;
    shdr->sh_addr = shAddress;//shAddress;
    shdr->sh_offset = shOffset;
    shdr->sh_size = shSize;
    shdr->sh_link = 0;
    shdr->sh_info = 0;
    shdr->sh_addralign = 16;
    shdr->sh_entsize = 0;

    _AotCDSection = shdr;
    strcpy(_AotCDSectionName, ".aotcd");
}

void 
ELF::ELFSharedObjectGenerator::initializeDataSection(uint32_t shName, ELFAddress shAddress,
                                                 ELFOffset shOffset, uint32_t shSize)
{

    ELFSectionHeader * shdr = static_cast<ELFSectionHeader *>(_rawAllocator.allocate(sizeof(ELFSectionHeader)));
    
    shdr->sh_name = shName;
    shdr->sh_type = SHT_PROGBITS;
    shdr->sh_flags = SHF_ALLOC | SHF_WRITE;
    shdr->sh_addr = shAddress + (ELFAddress) 0x200000;
    shdr->sh_offset = shOffset;
    shdr->sh_size = shSize;
    shdr->sh_link = 0;
    shdr->sh_info = 0;
    shdr->sh_addralign = 8;
    shdr->sh_entsize = 0;

    _dataSection = shdr;
    strcpy(_dataSectionName, ".data");
}
    
void
ELF::ELFSharedObjectGenerator::initializeDynSymSection(uint32_t shName, ELFAddress shAddress, ELFOffset shOffset, uint32_t shSize, uint32_t shLink)
{
    
    ELFSectionHeader * shdr = static_cast<ELFSectionHeader *>(_rawAllocator.allocate(sizeof(ELFSectionHeader)));

    shdr->sh_name = shName;
    shdr->sh_type = SHT_DYNSYM; // SHT_DYNSYM
    shdr->sh_flags = 0; //SHF_ALLOC;
    shdr->sh_addr = shAddress; //// fake address because not continuous
    shdr->sh_offset = shOffset;
    shdr->sh_size = shSize;
    shdr->sh_link = shLink; // dynamic string table index
    shdr->sh_info = 1; // index of first non-local symbol: for now all symbols are global //One greater than the symbol table index of the last local symbol (binding STB_LOCAL).
    shdr->sh_addralign = TR::Compiler->target.is64Bit() ? 8 : 4;
    shdr->sh_entsize = sizeof(ELFSymbol);

    _dynSymSection = shdr;
    strcpy(_dynSymSectionName, ".dynsym");
}

void
ELF::ELFSharedObjectGenerator::initializeStrTabSection(uint32_t shName, ELFAddress shAddress, ELFOffset shOffset, uint32_t shSize)
{    
    ELFSectionHeader * shdr = static_cast<ELFSectionHeader *>(_rawAllocator.allocate(sizeof(ELFSectionHeader)));
    
    shdr->sh_name = shName;
    shdr->sh_type = SHT_STRTAB;
    shdr->sh_flags = 0;
    shdr->sh_addr = shAddress;
    shdr->sh_offset = shOffset;
    shdr->sh_size = shSize;
    shdr->sh_link = 0;
    shdr->sh_info = 0;
    shdr->sh_addralign = 1;
    shdr->sh_entsize = 0;

    _shStrTabSection = shdr;
    strcpy(_shStrTabSectionName, ".shstrtab");
}

void
ELF::ELFSharedObjectGenerator::initializeDynStrSection(uint32_t shName, ELFAddress shAddress, ELFOffset shOffset, uint32_t shSize)
{
    ELFSectionHeader * shdr = static_cast<ELFSectionHeader *>(_rawAllocator.allocate(sizeof(ELFSectionHeader)));

    shdr->sh_name = shName;
    shdr->sh_type = SHT_STRTAB;
    shdr->sh_flags = 0;
    shdr->sh_addr = shAddress;
    shdr->sh_offset = shOffset;
    shdr->sh_size = shSize;
    shdr->sh_link = 0;
    shdr->sh_info = 0;
    shdr->sh_addralign = 1;
    shdr->sh_entsize = 0;

    _dynStrSection = shdr;
    strcpy(_dynStrSectionName, ".dynstr");
}

void
ELF::ELFSharedObjectGenerator::initializeRelaSection(uint32_t shName, ELFAddress shAddress, ELFOffset shOffset, uint32_t shSize)
{
    ELFSectionHeader * shdr = static_cast<ELFSectionHeader *>(_rawAllocator.allocate(sizeof(ELFSectionHeader)));

    shdr->sh_name = shName;
    shdr->sh_type = SHT_RELA;
    shdr->sh_flags = 0;
    shdr->sh_addr = shAddress;
    shdr->sh_offset = shOffset;
    shdr->sh_size = shSize;
    shdr->sh_link = 3; // dynsymSection index in the elf file. Kept this hardcoded for now as this shdr applies to relocatable only
    shdr->sh_info = 1;
    shdr->sh_addralign = TR::Compiler->target.is64Bit() ? 8 : 4;
    shdr->sh_entsize = sizeof(ELFRela);

    _relaSection = shdr;
    strcpy(_relaSectionName, ".rela.text");
}

void
ELF::ELFSharedObjectGenerator::initializeDynamicSection(uint32_t shName, ELFAddress shAddress, ELFOffset shOffset, uint32_t shSize)
{
    ELFSectionHeader * shdr = static_cast<ELFSectionHeader *>(_rawAllocator.allocate(sizeof(ELFSectionHeader)));

    shdr->sh_name = shName;
    shdr->sh_type = SHT_DYNAMIC;
    shdr->sh_flags = 0;
    shdr->sh_addr = shAddress + (ELFAddress) 0x200000;
    shdr->sh_offset = shOffset;
    shdr->sh_size = shSize;
    shdr->sh_link = 3;
    shdr->sh_info = 0;
    shdr->sh_addralign = TR::Compiler->target.is64Bit() ? 8 : 4;    
    shdr->sh_entsize = sizeof(ELFDynamic);

    _dynamicSection = shdr;
    strcpy(_dynamicSectionName, ".dynamic");
}

void
ELF::ELFSharedObjectGenerator::initializeHashSection(uint32_t shName, ELFAddress shAddress, ELFOffset shOffset, uint32_t shSize)
{
    ELFSectionHeader * shdr = static_cast<ELFSectionHeader *>(_rawAllocator.allocate(sizeof(ELFSectionHeader)));

    shdr->sh_name = shName;
    shdr->sh_type = SHT_HASH;
    shdr->sh_flags = SHF_ALLOC;
    shdr->sh_addr = shAddress;
    shdr->sh_offset = shOffset;
    shdr->sh_size = shSize;
    shdr->sh_link = 2;
    shdr->sh_info = 0;
    shdr->sh_addralign = TR::Compiler->target.is64Bit() ? 8 : 4;  
    shdr->sh_entsize = 0;

    _hashSection = shdr;
    strcpy(_hashSectionName, ".hash");
}

void 
ELF::ELFSharedObjectGenerator::writeCodeSegmentToFile(::FILE *fp)
{
    fwrite(static_cast<const void *>(_textSegmentStart), sizeof(uint8_t), _codeSize, fp);
}


void
ELF::ELFSharedObjectGenerator::initializePHdr(void)
{
    ELFProgramHeader *phdr_loadR =
    static_cast<ELFProgramHeader *>(_rawAllocator.allocate(sizeof(ELFProgramHeader),
    std::nothrow));

    ELFProgramHeader *phdr_loadW =
    static_cast<ELFProgramHeader *>(_rawAllocator.allocate(sizeof(ELFProgramHeader),
    std::nothrow));

    ELFProgramHeader *phdr_dynamic =
    static_cast<ELFProgramHeader *>(_rawAllocator.allocate(sizeof(ELFProgramHeader),
    std::nothrow));
    
    _programHeaderLoadRX = phdr_loadR;
    _programHeaderLoadRW = phdr_loadW;
    _programHeaderDynamic = phdr_dynamic;

    //printf("\n In initializePHdr\n");
    _programHeaderLoadRX->p_type = PT_LOAD; //should be loaded in memory
    _programHeaderLoadRX->p_offset = 0; //offset of program header from the first byte of file to be loaded
    _programHeaderLoadRX->p_vaddr = 0; //(ELFAddress) _codeStart; //virtual address to load into
    _programHeaderLoadRX->p_paddr = 0; //(ELFAddress) _codeStart; //physical address to load into
    _programHeaderLoadRX->p_filesz = (Elf64_Xword) (dynamicSectionStartOffset );//_codeSize; //in-file size
    _programHeaderLoadRX->p_memsz = (Elf64_Xword) (dynamicSectionStartOffset );//_codeSize; //in-memory size
    _programHeaderLoadRX->p_flags = PF_X | PF_R | PF_W; // should add PF_W if we get around to loading patchable code
    _programHeaderLoadRX->p_align = 0x200000;

    _programHeaderLoadRW->p_type = PT_LOAD; //should be loaded in memory
    _programHeaderLoadRW->p_offset = (ELFOffset) dataSectionStartOffset; //offset of program header from the first byte of file to be loaded
    _programHeaderLoadRW->p_vaddr = (ELFAddress) (dataSectionStartOffset + 0x200000); //(ELFAddress) _codeStart; //virtual address to load into
    _programHeaderLoadRW->p_paddr = (ELFAddress) (dataSectionStartOffset + 0x200000); //(ELFAddress) _codeStart; //physical address to load into
    _programHeaderLoadRW->p_filesz = (Elf64_Xword) (_dataSize + (sizeof(ELFDynamic) * 6));//_codeSize; //in-file size
    _programHeaderLoadRW->p_memsz = (Elf64_Xword) (_dataSize + (sizeof(ELFDynamic) * 6));//_codeSize; //in-memory size
    _programHeaderLoadRW->p_flags = PF_R | PF_W; // should add PF_W if we get around to loading patchable code
    _programHeaderLoadRW->p_align = 0x200000;

    _programHeaderDynamic->p_type = PT_DYNAMIC; //should be loaded in memory
    _programHeaderDynamic->p_offset = (ELFOffset) dynamicSectionStartOffset; //offset of program header from the first byte of file to be loaded
    _programHeaderDynamic->p_vaddr = (ELFAddress) (dynamicSectionStartOffset + 0x200000); //virtual address to load into
    _programHeaderDynamic->p_paddr = (ELFAddress) (dynamicSectionStartOffset + 0x200000); //physical address to load into
    _programHeaderDynamic->p_filesz = (Elf64_Xword) (sizeof(ELFDynamic) * 6); //in-file size
    _programHeaderDynamic->p_memsz = (Elf64_Xword) (sizeof(ELFDynamic) * 6); //in-memory size
    _programHeaderDynamic->p_flags = PF_R | PF_W; // should add PF_W if we get around to loading patchable code
    _programHeaderDynamic->p_align = 0x8;
    //printf("\n Out initializePHdr\n");
}

void
ELF::ELFSharedObjectGenerator::initializeELFHeader(void)
{
    //printf("\n In initializeELFHeader\n");
    _header->e_type = ET_DYN;           
    _header->e_entry = (ELFAddress) _textSegmentStart; //no associated entry point for shared object ELF files
    _header->e_phoff = sizeof(ELFEHeader); //program header for shared object files
    _header->e_shoff = sizeof(ELFEHeader) + (sizeof(ELFProgramHeader)*3) + _codeSize; //start of the section header table in bytes from the first byte of the ELF file
    _header->e_phentsize = sizeof(ELFProgramHeader); //no program headers in shared object elf
    _header->e_phnum = 3;
    _header->e_shnum = 8; 
    _header->e_shstrndx = 3; //index of section header string table
    //printf("\n Out initializeELFHeader\n");
}

void
ELF::ELFSharedObjectGenerator::initializeSectionNames(void)
{ 
    //printf("\n In initializeSectionNames\n");
    _zeroSectionName[0] = 0;
    strcpy(_shStrTabSectionName, ".shstrtab");
    strcpy(_AotCDSectionName, ".aotcd");
    strcpy(_dynSymSectionName, ".dynsym");
    strcpy(_dynStrSectionName, ".dynstr");
    strcpy(_hashSectionName, ".hash");
    strcpy(_dataSectionName, ".data");
    strcpy(_dynamicSectionName, ".dynamic");
    
    shStrTabNameLength = sizeof(_zeroSectionName) +
                                  sizeof(_shStrTabSectionName) +
                                  sizeof(_AotCDSectionName) +
                                  //sizeof(_relaSectionName) +
                                  sizeof(_dynSymSectionName) +
                                  sizeof(_dynStrSectionName) +
                                  sizeof(_hashSectionName) +
                                  sizeof(_dataSectionName) +
                                  sizeof(_dynamicSectionName);

    //printf("\n shStrTabNameLength %d \n ",shStrTabNameLength);                            
}

void
ELF::ELFSharedObjectGenerator::initializeSectionOffsets(void)
{ 
    //printf("\n In initializeSectionOffsets\n");
    dynamicSectionStartOffset = 0;
    
    AotCDSectionStartOffset = sizeof(ELFEHeader) + (sizeof(ELFProgramHeader)*3);
    //printf("\n textSectionStartOffset %d \n ",textSectionStartOffset);
    
    dynsymSectionStartOffset  = sizeof(ELFEHeader) + (sizeof(ELFProgramHeader)*3) + _codeSize +   
                                  (sizeof(ELFSectionHeader) * /* # shdr */ 8) +
                                  shStrTabNameLength;
    //printf("\n dynsymSectionStartOffset %d \n ",dynsymSectionStartOffset);
    
    dynstrSectionStartOffset  = dynsymSectionStartOffset + 
                                      (_numSymbols + /* UNDEF */ 2) * sizeof(ELFSymbol);
    //printf("\n dynstrSectionStartOffset %d \n ",dynstrSectionStartOffset);
    
    //printf("\n _totalELFSymbolNamesLength %d \n ",_totalELFSymbolNamesLength);
    hashSectionStartOffset =  dynstrSectionStartOffset + _totalELFSymbolNamesLength;   
    //printf("\n hashSectionStartOffset %d \n ",hashSectionStartOffset);  

    dataSectionStartOffset = hashSectionStartOffset + (sizeof(uint32_t) * (2 + nchain + nbucket));
    //printf("\n dataSectionStartOffset %d \n ",dataSectionStartOffset);
    
    dynamicSectionStartOffset = dataSectionStartOffset + _dataSize;  
    //printf("\n dynamicSectionStartOffset %d \n ",dynamicSectionStartOffset);                   
}

void
ELF::ELFSharedObjectGenerator::buildSectionHeaders(void)
{    
    //printf("\n In ELFSharedObjectGenerator buildSectionHeaders\n");
    shStrTabNameLength = 0;
    dynamicSectionStartOffset = 0;
    shStrTabNameLength = sizeof(_zeroSectionName) +
                                  sizeof(_shStrTabSectionName) +
                                  sizeof(_AotCDSectionName) +
                                  //sizeof(_relaSectionName) +
                                  sizeof(_dynSymSectionName) +
                                  sizeof(_dynStrSectionName) +
                                  sizeof(_hashSectionName) +
                                  sizeof(_dataSectionName) +
                                  sizeof(_dynamicSectionName);
                                  

    //printf("\n shStrTabNameLength %d \n ",shStrTabNameLength); 
    //printf("\n _codeSize %d \n ",_codeSize); 

    /* offset calculations */
    uint32_t trailerStartOffset = sizeof(ELFEHeader) + (sizeof(ELFProgramHeader)*3) +_codeSize;
    uint32_t symbolsStartOffset = trailerStartOffset +
                                  (sizeof(ELFSectionHeader) * /* # shdr */ 8) +
                                  shStrTabNameLength;
    uint32_t symbolNamesStartOffset = symbolsStartOffset + 
                                      (_numSymbols + /* UNDEF */ 2) * sizeof(ELFSymbol);
    //uint32_t relaStartOffset = symbolNamesStartOffset + _totalELFSymbolNamesLength;
    uint32_t shNameOffset = 0;
    //printf("\n symbolNamesStartOffset %d \n ",symbolNamesStartOffset);
    //printf("\n _totalELFSymbolNamesLength %d \n ",_totalELFSymbolNamesLength);
    dynamicSectionStartOffset = dataSectionStartOffset + _dataSize;
    //printf("\n dynamicSectionStartOffset %d \n ",dynamicSectionStartOffset);   
    initializeZeroSection();
    shNameOffset += sizeof(_zeroSectionName);

    initializeAotCDSection(shNameOffset,
                          (ELFAddress) AotCDSectionStartOffset,
                          sizeof(ELFEHeader) + (sizeof(ELFProgramHeader)*3),
                          _codeSize);
    shNameOffset += sizeof(_AotCDSectionName);

/*     initializeRelaSection(shNameOffset,
                          relaStartOffset, 
                          _numRelocations * sizeof(ELFRela));
    shNameOffset += sizeof(_relaSectionName); */

    initializeDynSymSection(shNameOffset, 
                            (ELFAddress) symbolsStartOffset,
                            symbolsStartOffset,
                            symbolNamesStartOffset - symbolsStartOffset,
                            /*Index of dynStrTab*/ 4);
    shNameOffset += sizeof(_dynSymSectionName);

    initializeStrTabSection(shNameOffset, 
                            (ELFAddress) (symbolsStartOffset - shStrTabNameLength),
                            symbolsStartOffset - shStrTabNameLength, 
                            shStrTabNameLength);
    shNameOffset += sizeof(_shStrTabSectionName);

    initializeDynStrSection(shNameOffset,
                            (ELFAddress) symbolNamesStartOffset,
                            symbolNamesStartOffset, 
                            _totalELFSymbolNamesLength);
    shNameOffset += sizeof(_dynStrSectionName);

    initializeHashSection(shNameOffset,
                            (ELFAddress) hashSectionStartOffset, 
                            hashSectionStartOffset, 
                            sizeof(uint32_t) * (2 + nchain + nbucket));
    shNameOffset += sizeof(_hashSectionName);

    initializeDataSection(shNameOffset,
                          (ELFAddress) dataSectionStartOffset,
                          dataSectionStartOffset,
                          _dataSize);
    shNameOffset += sizeof(_dataSectionName);

    initializeDynamicSection(shNameOffset,
                            (ELFAddress) dynamicSectionStartOffset, 
                            dynamicSectionStartOffset, 
                            sizeof(ELFDynamic) * 6);
    shNameOffset += sizeof(_dynamicSectionName);

}

bool 
ELF::ELFSharedObjectGenerator::emitELF(const char * filename,
                TR::CodeCacheSymbol *symbols, uint32_t numSymbols,
                uint32_t totalELFSymbolNamesLength,
                TR::CodeCacheRelocationInfo *relocations,
                uint32_t numRelocations)
{
    _symbols = symbols;
    _relocations = relocations;
    _numSymbols = numSymbols;
    _numRelocations = numRelocations;
    _totalELFSymbolNamesLength = totalELFSymbolNamesLength;

    buildSectionHeaders();
    
    return emitELFFile(filename);
}

bool 
ELF::ELFSharedObjectGenerator::emitAOTELF(const char * filename,
                //TR::CodeCacheSymbol *symbols, 
                uint32_t numSymbols,
                uint32_t totalELFSymbolNamesLength)//,
                //TR::CodeCacheRelocationInfo *relocations,
               // uint32_t numRelocations)
{
    //_symbols = symbols;
    //_relocations = relocations;
    _numSymbols = numSymbols;
    //_numRelocations = numRelocations;
    _totalELFSymbolNamesLength = totalELFSymbolNamesLength;
    //TR::Compiler->aotAdapter->_methodNameToHeaderMap;
    
    //printf("\n In emitAOTELF\n");

    char temp[] = "[Dummy data in data segment]";
    _dataSize = sizeof(temp);
    //printf("\n Size of data = %d", _dataSize);

    initializeHashValues(_numSymbols);
   // printf("\n After initializeHashValues\n");
    
    initializeSectionNames();
  //  printf("\n After initializeSectionNames\n");
    
    initializeSectionOffsets();

    initializePHdr();

    buildSectionHeaders();
    //printf("\n Before emitELFFile\n");
    bool val = emitAOTELFFile(filename);
    //printf("\n Last\n");
    return val;
}


bool
ELF::ELFSharedObjectGenerator::emitAOTELFFile(const char * filename)
{
    //printf("\n In emitAOTELFFile\n");
    ::FILE *elfFile = fopen(filename, "wb");

    if (NULL == elfFile)
    {
        return false;
    }

    uint32_t size = 0;

    writeHeaderToFile(elfFile);

    writeProgramHeaderToFile(elfFile);

    writeCodeSegmentToFile(elfFile);
    
    writeSectionHeaderToFile(elfFile, _zeroSection);

    writeSectionHeaderToFile(elfFile, _AotCDSection);

   // if (_relaSection)
   // {
   //     writeSectionHeaderToFile(elfFile, _relaSection);
   // }

    writeSectionHeaderToFile(elfFile, _dynSymSection);

    writeSectionHeaderToFile(elfFile, _shStrTabSection);

    writeSectionHeaderToFile(elfFile, _dynStrSection);

    writeSectionHeaderToFile(elfFile, _hashSection);

    writeSectionHeaderToFile(elfFile, _dataSection);
    
   writeSectionHeaderToFile(elfFile, _dynamicSection);

    writeSectionNameToFile(elfFile, _zeroSectionName, sizeof(_zeroSectionName));

    writeSectionNameToFile(elfFile, _AotCDSectionName, sizeof(_AotCDSectionName));
    
    //if (_relaSection)
   // {
   //     writeSectionNameToFile(elfFile, _relaSectionName, sizeof(_relaSectionName));
  //  }
    writeSectionNameToFile(elfFile, _dynSymSectionName, sizeof(_dynSymSectionName));

    writeSectionNameToFile(elfFile, _shStrTabSectionName, sizeof(_shStrTabSectionName));

    writeSectionNameToFile(elfFile, _dynStrSectionName, sizeof(_dynStrSectionName));

    writeSectionNameToFile(elfFile, _hashSectionName, sizeof(_hashSectionName));

    writeSectionNameToFile(elfFile, _dataSectionName, sizeof(_dataSectionName));

    writeSectionNameToFile(elfFile, _dynamicSectionName, sizeof(_dynamicSectionName));
    
    processAllSymbols(elfFile);

    writeHashSectionToFile(elfFile);

    writeDataSegmentToFile(elfFile);

    writeDynamicSectionEntries(elfFile);
    
    //writeELFSymbolsToFile(elfFile);
   // if(_relaSection)
   // {
   //     writeRelaEntriesToFile(elfFile);
   // }
    //printf("\n After writeAOTELFSymbolsToFile\n");
    char str[] = "ELFEnd";
    fwrite(str , 1 , sizeof(str) , elfFile );
    fclose(elfFile);
    //printf("\n Out emitAOTELFFile\n");
    return true;
}

void 
ELF::ELFSharedObjectGenerator::writeProgramHeaderToFile(::FILE *fp)
{
    fwrite(_programHeaderLoadRX, sizeof(uint8_t), sizeof(ELFProgramHeader), fp);
    fwrite(_programHeaderLoadRW, sizeof(uint8_t), sizeof(ELFProgramHeader), fp);
    fwrite(_programHeaderDynamic, sizeof(uint8_t), sizeof(ELFProgramHeader), fp);
}

void 
ELF::ELFSharedObjectGenerator::processAllSymbols(::FILE *fp)
{
    //printf("\n In writeAOTELFSymbolsToFile\n");
    ELFSymbol * elfSym = static_cast<ELFSymbol*>(_rawAllocator.allocate(sizeof(ELFSymbol)));
    char ELFSymbolNames[_totalELFSymbolNamesLength];
    //uint8_t hash_index = 0;
    /* Writing the UNDEF symbol*/
    writeSymbolToFile(fp, elfSym, 0, 0, 0, 0, 0, 0);

    //hash_index++;

    ELFSymbolNames[0] = 0; //first bit needs to be 0, corresponding to the name of the UNDEF symbol
    char * names = ELFSymbolNames + 1; //the rest of the array will contain method names
    TR::CodeCacheSymbol *sym = _symbols; 

    const uint8_t* rangeStart; //relocatable elf files need symbol offset from segment base
    rangeStart = 0;

    
    //values that are unchanged are being kept out of the while loop
    //elfSym->st_other = ELF_ST_VISIBILITY(STV_DEFAULT);
    //elfSym->st_info = ELF_ST_INFO(STB_GLOBAL,STT_FUNC);
    /* this while loop re-uses the ELFSymbol and write
       CodeCacheSymbol info into file */
    uint32_t symbolDefStart = AotCDSectionStartOffset;
    TR::AOTMethodHeader* hdr;

     // for( auto it = ELFDataMap.begin(); it != ELFDataMap.end(); ++it )
      for( auto it = ELFDataMap.begin(); it != ELFDataMap.end(); ++it )
        {
        char const * methodName = it->first;
        hdr = it->second;

        //printf("\n hdr->self()->newCompiledCodeStart = [ %p ]", hdr->self()->newCompiledCodeStart);
        memcpy(names, methodName, strlen(methodName) + 1);
        unsignedInt st_name = names - ELFSymbolNames;
       // elfSym->st_name = names - ELFSymbolNames;
        //elfSym->st_shndx = hdr->self()->compiledCodeStart ? 1 : SHN_UNDEF;
        //elfSym->st_value = hdr->self()->newCompiledCodeStart ? (ELFAddress)(hdr->self()->newCompiledCodeStart - rangeStart) : 0;
        //elfSym->st_value = (ELFAddress) symbolDefStart;
        uint32_t AotCDBufferSize = hdr->self()->sizeOfSerializedVersion();
       // elfSym->st_size = AotCDBufferSize;
        //fwrite(elfSym, sizeof(uint8_t), sizeof(ELFSymbol), fp);
        writeSymbolToFile(fp, 
                          elfSym,
                          st_name, 
                          ELF_ST_INFO(STB_GLOBAL,STT_FUNC), 
                          ELF_ST_VISIBILITY(STV_DEFAULT),  
                          hdr->self()->compiledCodeStart ? 1 : SHN_UNDEF, 
                          (ELFAddress) symbolDefStart, 
                          AotCDBufferSize);


        symbolDefStart += AotCDBufferSize;
        names += (strlen(methodName) + 1);
        }

    /* Writing the DYNAMIC symbol*/
    char const * dynamicSymbolName = "_DYNAMIC";
    memcpy(names, dynamicSymbolName, strlen(dynamicSymbolName) + 1);
    
    writeSymbolToFile(fp,
                      elfSym, 
                      names - ELFSymbolNames, 
                      ELF_ST_INFO(STB_LOCAL,STT_OBJECT), 
                      ELF_ST_VISIBILITY(STV_DEFAULT), 
                      1, 
                      dynamicSectionStartOffset, 
                      0);


    names += (strlen(dynamicSymbolName) + 1);

    fwrite(ELFSymbolNames, sizeof(uint8_t), _totalELFSymbolNamesLength, fp);

    _rawAllocator.deallocate(elfSym);
    //printf("\n Out writeAOTELFSymbolsToFile\n");
}

//ELFSymbol *elfSym can be alllocate and deallocate din the constructors/destructor
void 
ELF::ELFSharedObjectGenerator::writeSymbolToFile(::FILE *fp, ELFSymbol *elfSym, uint32_t st_name, unsigned char st_info, unsigned char st_other, ELFSection st_shndx, ELFAddress st_value, uint64_t st_size)
{
    ELFSymbol * elfSyms = static_cast<ELFSymbol*>(_rawAllocator.allocate(sizeof(ELFSymbol)));
    elfSyms->st_name = st_name;
    elfSyms->st_info = st_info;
    elfSyms->st_other = st_other;
    elfSyms->st_shndx = st_shndx;
    elfSyms->st_value = st_value;
    elfSyms->st_size = st_size;
    fwrite(elfSyms, sizeof(uint8_t), sizeof(ELFSymbol), fp);
}

void
ELF::ELFSharedObjectGenerator::writeDynamicSectionEntries(::FILE *fp)
{
    //printf(" \n In writeDynamicSectionEntries \n");
    ELFDynamic * dynEntry = static_cast<ELFDynamic*>(_rawAllocator.allocate(sizeof(ELFDynamic)));
    //.dynsym Entry
    writeDynamicEntryToFile(fp, dynEntry, DT_SYMTAB, 0, dynsymSectionStartOffset, 0);
    //.dynstr Entry
    writeDynamicEntryToFile(fp, dynEntry, DT_STRTAB, 0, dynstrSectionStartOffset, 0);
    //.hash Entry
    writeDynamicEntryToFile(fp, dynEntry, DT_HASH, 0, hashSectionStartOffset, 0);

    writeDynamicEntryToFile(fp, dynEntry, DT_STRSZ, _totalELFSymbolNamesLength, 0, 1);

    writeDynamicEntryToFile(fp, dynEntry, DT_SYMENT, sizeof(ELFSymbol) * (_numSymbols+2), 0, 1);
    //NULL Entry
    writeDynamicEntryToFile(fp, dynEntry, DT_NULL, 0, 0, 0);
    //.rela.text Entry
   
    _rawAllocator.deallocate(dynEntry);
    //printf(" \n Out writeDynamicSectionEntries \n");
}

void
ELF::ELFSharedObjectGenerator::writeDynamicEntryToFile(::FILE *fp, ELFDynamic * dynEntry, Elf64_Sxword tag, uint32_t value, uint64_t ptr, uint32_t flag)
{
    //printf(" \n In writeDynamicValueEntryToFile \n");
    dynEntry->d_tag = tag;
    if(flag == 1){
        dynEntry->d_un.d_val = value; 
        }
    else{
        dynEntry->d_un.d_ptr = ptr;
        }
    fwrite(dynEntry, sizeof(uint8_t), sizeof(ELFDynamic), fp);
    //printf(" \n Out writeDynamicEntryToFile \n");
}

// Refer https://flapenguin.me/elf-dt-hash
uint32_t 
ELF::ELFSharedObjectGenerator::elfHashSysV(const char* symbolName)
{
  uint32_t h = 0, g = 0;
  
  while (*symbolName) 
    {
    h = (h << 4) + *symbolName++;
    if((g = h & 0xf0000000))
        h ^= g >> 24;
    h &= ~g;
    }

    return h;
}

void 
ELF::ELFSharedObjectGenerator::writeHashSectionToFile(::FILE *fp)
{
    uint8_t hash_index = 1;

    for( auto it = ELFDataMap.begin(); it != ELFDataMap.end(); ++it )
        {
        char const * methodName = it->first;
        hash_array[hash_index] = elfHashSysV(methodName);
        mod_array[hash_index] = hash_array[hash_index] % nbucket;
        hash_index++;
        }

    char const * dynamicSymbolName = "_DYNAMIC";
    hash_array[hash_index] = elfHashSysV(dynamicSymbolName);
    mod_array[hash_index] = hash_array[hash_index] % nbucket;
    hash_index++;

    for(int i = 0; i < nbucket; i++) 
        {
        int flag = 0, temp = 0;
        for(int j = 1; j < nchain; j++) 
            {
            if(i == mod_array[j])
            {
                if(flag == 0)
                    {
                    bucket_array[i] = j;
                    flag = 1;
                    temp = j;
                    }
                else
                    {
                    chain_array[temp] = j;
                    temp = j;
                    }
                    //printf("\n bucket_array[%d] = %u",i,j);
                   // continue;
                }   
            }
        }

   // for(int i = 0; i < nchain; i++) 
     //   {
        //printf("\n hash_array[%d] = %u mod_array[%d] = %u chain_array[%u] = %u  ",i,hash_array[i],i,mod_array[i],i,chain_array[i]);
     //   }
  //  for(int i = 0; i < nbucket; i++) 
    //    {
        //printf("\n bucket_array[%d] = %u",i,bucket_array[i]);
   //     }
    
    int written = 0;
    written = fwrite(&nbucket,sizeof(uint32_t),1, fp);
    if (written == 0) {
    printf("Error during writing to file 1!");
    }
    written = fwrite(&nchain,sizeof(uint32_t),1, fp);
    if (written == 0) {
    printf("Error during writing to file 2!");
    }
    written = fwrite(bucket_array, sizeof(uint32_t), nbucket, fp);
    if (written == 0) {
    printf("Error during writing to file 3!");
    }
    written = fwrite(chain_array, sizeof(uint32_t), nchain, fp);
    if (written == 0) {
    printf("Error during writing to file 4!");
    }
}

size_t 
ELF::ELFSharedObjectGenerator::getNoOfBuckets(uint32_t numSymbols)
{
    static const size_t elfBuckets[] = {0, 1, 3, 17, 37, 67, 97, 131, 197, 263, 521, 1031, 2053, 4099, 8209, 16411, 32771};
    size_t n = sizeof(elfBuckets) / sizeof(elfBuckets[0]);
    for(int i = 0; i < n; i++)
    {
        if(elfBuckets[i] > numSymbols)
        {
            //printf("\n elfBuckets[i] = [%d]",elfBuckets[i]);
            //printf("\n elfBuckets[i-1] = [%d]",elfBuckets[i-1]);
            return elfBuckets[i-1];     
        }
    } 
}

void 
ELF::ELFSharedObjectGenerator::initializeHashValues(uint32_t numSymbols)
{
    //printf(" \n In initializeHashValues \n");
    nbucket = (uint32_t)getNoOfBuckets(numSymbols + 2);
   // printf("\n nbucket = [%u]",nbucket);
    nchain = numSymbols + 2;
   // printf("\n nchain = [%u]",nchain);
    bucket_array = static_cast<uint32_t*>(_rawAllocator.allocate(sizeof(uint32_t) * nbucket));
    chain_array = static_cast<uint32_t*>(_rawAllocator.allocate(sizeof(uint32_t) * nchain));
    hash_array = static_cast<uint32_t*>(_rawAllocator.allocate(sizeof(uint32_t) * nchain));
    mod_array = static_cast<uint32_t*>(_rawAllocator.allocate(sizeof(uint32_t) * nchain));
    for(int i = 0; i < nchain; i++) 
        {
        chain_array[i] = 0;
        hash_array[i] = 0;
        mod_array[i] = 0;
        }
    for(int i = 0; i < nbucket; i++) 
        {
        bucket_array[i] = 0;
        }
   // printf(" \n Out initializeHashValues \n");
}

 TR::AOTStorageInterface* ELF::ELFSharedObjectGenerator::self() //suspicious behaviour .. not sure about correctness
   {
   return reinterpret_cast<TR::AOTStorageInterface*> (this);
   }


/* uint8_t* ELF::ELFSharedObjectGenerator::loadEntry(const char* key)
   {
   TR_UNIMPLEMENTED();
   return 0;
   } */

void 
ELF::ELFSharedObjectGenerator::storeEntry(const char* key, TR::AOTMethodHeader* hdr)
   {
     // printf("\n In ELF storeEntry  ");
     // printf("\n CompiledCodeStart = [ %p ]",hdr->self()->compiledCodeStart);
    //  printf("\n CompiledCodeSize = [ %d ]",hdr->self()->compiledCodeSize);
    //  printf("\n RelocationsDataStart = [ %p ]",hdr->self()->relocationsStart);
    //  printf("\n RelocationsDataSize = [ %d ]",hdr->self()->relocationsSize);
      printf("\n Method Name = %s\n", key);
      //_key = key;
      ELFDataMap[key] = hdr;
   }

void 
ELF::ELFSharedObjectGenerator::registerInMap(const char*  methodName,TR::AOTMethodHeader* header)
    {
    ELFDataMap[methodName]=header;
    }

void 
ELF::ELFSharedObjectGenerator::consolidateCompiledCode(uint32_t methodCount, char * filename)
    {
    TR::AOTMethodHeader* hdr;
    TR::AOTStorageInterface* interface;
    uint32_t totalMethodNameLength = 0;
    uint8_t *ptrStart, *ptrEnd; 
    std::pair<uint32_t, uint32_t> total_CodeSizeMethodNameLength;
    
   for( auto it = ELFDataMap.begin(); it != ELFDataMap.end(); ++it )
        {
         const char *methodName;
         methodName = it->first;
         printf ("\n  methodName = %s\n", methodName);
        }
   
    total_CodeSizeMethodNameLength = calculateAggregateSize();

    //printf("\n Total compiled code size = [ %d ]\n", total_CodeSizeMethodNameLength.first);
   // printf("\n Total totalMethodNameLength = [ %d ]\n", total_CodeSizeMethodNameLength.second);

    uint8_t* compiledCodeBuffer = (uint8_t*) malloc (total_CodeSizeMethodNameLength.first);
    ptrStart = ptrEnd = compiledCodeBuffer;

      for( auto it = ELFDataMap.begin(); it != ELFDataMap.end(); ++it )
        {
        hdr = it->second;
        memcpy(ptrEnd, hdr->self()->compiledCodeStart, hdr->self()->compiledCodeSize);
        hdr->self()->newCompiledCodeStart = ptrEnd;
        ptrEnd += hdr->self()->compiledCodeSize;

        }

    char *inp = "_input.txt";
    char *soFilename = (char *) malloc (1 + strlen(filename) + strlen(inp));   
    strcpy(soFilename, filename);
    strcat(soFilename, inp); 

    FILE *inFile = fopen(soFilename, "wb");
    fwrite(ptrStart, sizeof(uint8_t), total_CodeSizeMethodNameLength.first, inFile);
    fclose(inFile);

    storeEntries(filename, ptrStart, total_CodeSizeMethodNameLength.first, total_CodeSizeMethodNameLength.second, methodCount);   
    }

void ELF::ELFSharedObjectGenerator::consolidateBuffers(uint32_t methodCount, char * filename)
    {
    TR::AOTMethodHeader* hdr;
    TR::AOTStorageInterface* interface;
    uint32_t totalMethodNameLength = 0;
    uint8_t *ptrStart, *ptrEnd; 
    std::pair<uint32_t, uint32_t> total_BufferMethodNameLength;
    
    total_BufferMethodNameLength = calculateAggregateBufferSize();

    printf("\n Total Buffer size = [ %d ]\n", total_BufferMethodNameLength.first);
    printf("\n Total totalMethodNameLength = [ %d ]\n", total_BufferMethodNameLength.second);
    uint8_t* compiledCodeBuffer = (uint8_t*) malloc (total_BufferMethodNameLength.first);
    ptrStart = ptrEnd = compiledCodeBuffer;

      for( auto it = ELFDataMap.begin(); it != ELFDataMap.end(); ++it )
        {
        hdr = it->second;
        hdr->self()->serialize(ptrEnd);
        ptrEnd += hdr->sizeOfSerializedVersion();
        
        }
    printf("\n Consolidation done!\n");
    /* char *inp = "_input.txt";
    char *soFilename = (char *) malloc (1 + strlen(filename) + strlen(inp));   
    strcpy(soFilename, filename);
    strcat(soFilename, inp); 

    FILE *inFile = fopen(soFilename, "wb");
    fwrite(ptrStart, sizeof(uint8_t), total_BufferMethodNameLength.first, inFile);
    fclose(inFile); */

    storeEntries(filename, ptrStart, total_BufferMethodNameLength.first, total_BufferMethodNameLength.second, methodCount);   
    }


std::pair<uint32_t, uint32_t> 
ELF::ELFSharedObjectGenerator::calculateAggregateSize()
    {
   // printf("\n calculateTotalCompiledCodeSize");
    const char * methodName;
    uint32_t totalMethodNameLength = 0;
    TR::AOTMethodHeader* hdr;
    uint32_t totalCodeSize = 0;

    for( NameToHeaderMap::iterator it = ELFDataMap.begin(); it != ELFDataMap.end(); ++it )
        {
        methodName = it->first;
        hdr = ELFDataMap[methodName];
        totalCodeSize += hdr->self()->compiledCodeSize;
        totalMethodNameLength += strlen(methodName) + 1;
        }
    char const * _DYNAMIC  = "_DYNAMIC";
    totalMethodNameLength += strlen(_DYNAMIC) + 1;
    return std::make_pair(totalCodeSize,totalMethodNameLength);
    }

std::pair<uint32_t, uint32_t> 
ELF::ELFSharedObjectGenerator::calculateAggregateBufferSize()
    {
   // printf("\n calculateTotalCompiledCodeSize");
    const char * methodName;
    uint32_t totalMethodNameLength = 0;
    TR::AOTMethodHeader* hdr;
    uint32_t totalBufferSize = 0;

    for( NameToHeaderMap::iterator it = ELFDataMap.begin(); it != ELFDataMap.end(); ++it )
        {
        methodName = it->first;
        hdr = ELFDataMap[methodName];
   ////     printf("\n Size of buffer = %u \n",hdr->sizeOfSerializedVersion());
        totalBufferSize += hdr->sizeOfSerializedVersion();
        totalMethodNameLength += strlen(methodName) + 1;
        }
    char const * _DYNAMIC  = "_DYNAMIC";
    totalMethodNameLength += strlen(_DYNAMIC) + 1;
    return std::make_pair(totalBufferSize,totalMethodNameLength);
    }


void 
ELF::ELFSharedObjectGenerator::storeEntries(const char* fileName, uint8_t *codeStart, uint32_t codeSize, uint32_t totalMethodNameLength, uint32_t methodCount)
{
      printf("\n In fileName , methodCount , totalMethodNameLength = [%s] [%u] [%u]\n",fileName, methodCount, totalMethodNameLength);
      _textSegmentStart = codeStart;
      _codeSize = codeSize;
      initialize();
      emitAOTELF(fileName, methodCount, totalMethodNameLength);                                 
}

void 
ELF::ELFSharedObjectGenerator::dynamicLoading(const char*  fileName)
{
    //printf("\nIn ELF dynamicLoading");
    _handle = dlopen(fileName, RTLD_LAZY);
    if (!_handle) {
        fputs (dlerror(), stderr);
        exit(1);
    }                              
}

uint8_t* 
ELF::ELFSharedObjectGenerator::loadEntry(const char*  key)
{
    //printf("\nIn dynamicLoading loadEntry\n");
    char *error;
    if(_handle == NULL)
    {
        //printf("\n return null\n");
        return nullptr;
    }
    uint8_t* addr = (uint8_t*) dlsym(_handle, key);
        if ((error = dlerror()) != NULL)  {
            fputs(error, stderr);
            return nullptr;
        }else {
            return addr;
        }                                
}

#endif //LINUX
