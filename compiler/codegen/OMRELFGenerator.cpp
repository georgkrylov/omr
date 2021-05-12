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
#include "env/CompilerEnv.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "env/AOTMethodHeader.hpp"

OMR::ELFExecutableGenerator::ELFExecutableGenerator(TR::RawAllocator rawAllocator,
                            uint8_t const * codeStart, size_t codeSize):
                            ELFGenerator(rawAllocator, codeStart, codeSize)
                            {
                                initialize();
                            }

OMR::ELFRelocatableGenerator::ELFRelocatableGenerator(TR::RawAllocator rawAllocator,
                            uint8_t const * codeStart, size_t codeSize):
                            ELFGenerator(rawAllocator, codeStart, codeSize)
                            {
                                initialize();
                            }

OMR::ELFSharedObjectGenerator::ELFSharedObjectGenerator(TR::RawAllocator rawAllocator,
                            uint8_t const * codeStart, size_t codeSize):
                            ELFGenerator(rawAllocator, codeStart, codeSize),
                            numOfSections(8),
                            numOfProgramHeaders(3),
                            numOfDynamicEntries(6)
                            {}


/* TR::ELFGenerator *
OMR::ELFGenerator::self()
   {
   return static_cast<TR::ELFGenerator *>(this);
   } */

void
OMR::ELFGenerator::initializeELFHeaderForPlatform(void)
{
    _header->e_ident[EI_MAG0] = ELFMAG0;
    _header->e_ident[EI_MAG1] = ELFMAG1;
    _header->e_ident[EI_MAG2] = ELFMAG2;
    _header->e_ident[EI_MAG3] = ELFMAG3;
    _header->e_ident[EI_CLASS] = ELFClass;
    _header->e_ident[EI_VERSION] = EV_CURRENT;
    _header->e_ident[EI_ABIVERSION] = 0;
    _header->e_ident[EI_DATA] = TR::Compiler->target.cpu.isLittleEndian() ? ELFDATA2LSB : ELFDATA2MSB;
    
    for (auto b = EI_PAD;b < EI_NIDENT;b++)
        _header->e_ident[b] = 0;
    _header->e_ident[EI_OSABI] = ELFOSABI_LINUX; // Current support for Linux only. AIX would use the macro ELFOSABI_AIX.
    
    if (TR::Compiler->target.cpu.isX86())
    {
        _header->e_machine = TR::Compiler->target.is64Bit() ? EM_X86_64 : EM_386;
    }
    else if (TR::Compiler->target.cpu.isPower())
    {
        _header->e_machine = TR::Compiler->target.is64Bit() ? EM_PPC64 : EM_PPC;
    }
    else if (TR::Compiler->target.cpu.isZ()){
        _header->e_machine = EM_S390;
    }
    else
    {
        TR_ASSERT(0, "Unrecognized architecture: Failed to initialize ELF Header!");
    }

    _header->e_version = EV_CURRENT;
    _header->e_flags = 0; //processor-specific flags associated with the file
    _header->e_ehsize = sizeof(ELFEHeader);
    _header->e_shentsize = sizeof(ELFSectionHeader);
}

void 
OMR::ELFGenerator::initializeZeroSection()
{
    ELFSectionHeader * shdr = static_cast<ELFSectionHeader *>(_rawAllocator.allocate(sizeof(ELFSectionHeader)));
    
    shdr->sh_name = 0;
    shdr->sh_type = 0;
    shdr->sh_flags = 0;
    shdr->sh_addr = 0;
    shdr->sh_offset = 0;
    shdr->sh_size = 0;
    shdr->sh_link = 0;
    shdr->sh_info = 0;
    shdr->sh_addralign = 0;
    shdr->sh_entsize = 0;

    _zeroSection = shdr;
    _zeroSectionName[0] = 0;
}
    
void 
OMR::ELFGenerator::initializeTextSection(uint32_t shName, ELFAddress shAddress,
                                                 ELFOffset shOffset, uint32_t shSize)
{

    ELFSectionHeader * shdr = static_cast<ELFSectionHeader *>(_rawAllocator.allocate(sizeof(ELFSectionHeader)));
    
    shdr->sh_name = shName;
    shdr->sh_type = SHT_PROGBITS;
    shdr->sh_flags = SHF_ALLOC | SHF_EXECINSTR;
    shdr->sh_addr = shOffset;//shAddress;
    shdr->sh_offset = shOffset;
    shdr->sh_size = shSize;
    shdr->sh_link = 0;
    shdr->sh_info = 0;
    shdr->sh_addralign = 32;
    shdr->sh_entsize = 0;

    _textSection = shdr;
    strcpy(_textSectionName, ".text");
}

void 
OMR::ELFGenerator::initializeDataSection(uint32_t shName, ELFAddress shAddress,
                                                 ELFOffset shOffset, uint32_t shSize)
{

    ELFSectionHeader * shdr = static_cast<ELFSectionHeader *>(_rawAllocator.allocate(sizeof(ELFSectionHeader)));
    
    shdr->sh_name = shName;
    shdr->sh_type = SHT_PROGBITS;
    shdr->sh_flags = SHF_ALLOC | SHF_WRITE;
    shdr->sh_addr = shOffset + 0x200000;
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
OMR::ELFGenerator::initializeDynSymSection(uint32_t shName, ELFOffset shOffset, uint32_t shSize, uint32_t shLink)
{
    
    ELFSectionHeader * shdr = static_cast<ELFSectionHeader *>(_rawAllocator.allocate(sizeof(ELFSectionHeader)));

    shdr->sh_name = shName;
    shdr->sh_type = SHT_DYNSYM; // SHT_DYNSYM
    shdr->sh_flags = 0; //SHF_ALLOC;
    shdr->sh_addr = shOffset; //// fake address because not continuous
    shdr->sh_offset = shOffset;
    shdr->sh_size = shSize;
    shdr->sh_link = shLink; // dynamic string table index
    shdr->sh_info = 2; // index of first non-local symbol: for now all symbols are global //One greater than the symbol table index of the last local symbol (binding STB_LOCAL).
    shdr->sh_addralign = TR::Compiler->target.is64Bit() ? 8 : 4;
    shdr->sh_entsize = sizeof(ELFSymbol);

    _dynSymSection = shdr;
    strcpy(_dynSymSectionName, ".dynsym");
}

void
OMR::ELFGenerator::initializeStrTabSection(uint32_t shName, ELFOffset shOffset, uint32_t shSize)
{    
    ELFSectionHeader * shdr = static_cast<ELFSectionHeader *>(_rawAllocator.allocate(sizeof(ELFSectionHeader)));
    
    shdr->sh_name = shName;
    shdr->sh_type = SHT_STRTAB;
    shdr->sh_flags = 0;
    shdr->sh_addr = shOffset;
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
OMR::ELFGenerator::initializeDynStrSection(uint32_t shName, ELFOffset shOffset, uint32_t shSize)
{
    ELFSectionHeader * shdr = static_cast<ELFSectionHeader *>(_rawAllocator.allocate(sizeof(ELFSectionHeader)));

    shdr->sh_name = shName;
    shdr->sh_type = SHT_STRTAB;
    shdr->sh_flags = 0;
    shdr->sh_addr = shOffset;
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
OMR::ELFGenerator::initializeRelaSection(uint32_t shName, ELFOffset shOffset, uint32_t shSize)
{
    ELFSectionHeader * shdr = static_cast<ELFSectionHeader *>(_rawAllocator.allocate(sizeof(ELFSectionHeader)));

    shdr->sh_name = shName;
    shdr->sh_type = SHT_RELA;
    shdr->sh_flags = 0;
    shdr->sh_addr = shOffset;
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
OMR::ELFGenerator::initializeDynamicSection(uint32_t shName, ELFOffset shOffset, uint32_t shSize)
{
    ELFSectionHeader * shdr = static_cast<ELFSectionHeader *>(_rawAllocator.allocate(sizeof(ELFSectionHeader)));

    shdr->sh_name = shName;
    shdr->sh_type = SHT_DYNAMIC;
    shdr->sh_flags = 0;
    shdr->sh_addr = shOffset + 0x200000;
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
OMR::ELFGenerator::initializeHashSection(uint32_t shName, ELFOffset shOffset, uint32_t shSize)
{
    ELFSectionHeader * shdr = static_cast<ELFSectionHeader *>(_rawAllocator.allocate(sizeof(ELFSectionHeader)));

    shdr->sh_name = shName;
    shdr->sh_type = SHT_HASH;
    shdr->sh_flags = SHF_ALLOC;
    shdr->sh_addr = shOffset;
    shdr->sh_offset = shOffset;
    shdr->sh_size = shSize;
    shdr->sh_link = 2;
    shdr->sh_info = 0;
    shdr->sh_addralign = 1;    
    shdr->sh_entsize = 0;

    _hashSection = shdr;
    strcpy(_hashSectionName, ".hash");
}

bool
OMR::ELFGenerator::emitELFFile(const char * filename)
{
    ::FILE *elfFile = fopen(filename, "wb");

    if (NULL == elfFile)
    {
        return false;
    }

    uint32_t size = 0;

    writeHeaderToFile(elfFile);

    if (_programHeader)
    {
        writeProgramHeaderToFile(elfFile);
    }

    writeCodeSegmentToFile(elfFile);
    
    writeDataSegmentToFile(elfFile);

    writeSectionHeaderToFile(elfFile, _zeroSection);

    writeSectionHeaderToFile(elfFile, _textSection);

    writeSectionHeaderToFile(elfFile, _dataSection);

    if (_relaSection)
    {
        writeSectionHeaderToFile(elfFile, _relaSection);
    }

    writeSectionHeaderToFile(elfFile, _dynSymSection);

    writeSectionHeaderToFile(elfFile, _shStrTabSection);

    writeSectionHeaderToFile(elfFile, _dynStrSection);

    writeSectionNameToFile(elfFile, _zeroSectionName, sizeof(_zeroSectionName));

    writeSectionNameToFile(elfFile, _textSectionName, sizeof(_textSectionName));

    writeSectionNameToFile(elfFile, _dataSectionName, sizeof(_dataSectionName));

    if (_relaSection)
    {
        writeSectionNameToFile(elfFile, _relaSectionName, sizeof(_relaSectionName));
    }
    writeSectionNameToFile(elfFile, _dynSymSectionName, sizeof(_dynSymSectionName));

    writeSectionNameToFile(elfFile, _shStrTabSectionName, sizeof(_shStrTabSectionName));

    writeSectionNameToFile(elfFile, _dynStrSectionName, sizeof(_dynStrSectionName));
    
    writeELFSymbolsToFile(elfFile);
    if(_relaSection)
    {
        writeRelaEntriesToFile(elfFile);
    }
    fclose(elfFile);
    return true;
}



void
OMR::ELFGenerator::writeHeaderToFile(::FILE *fp)
{
    fwrite(_header, sizeof(uint8_t), sizeof(ELFEHeader), fp);
}

void 
OMR::ELFGenerator::writeProgramHeaderToFile(::FILE *fp)
{
    fwrite(_programHeader, sizeof(uint8_t), sizeof(ELFProgramHeader), fp);
}

void 
OMR::ELFGenerator::writeSectionHeaderToFile(::FILE *fp, ELFSectionHeader *shdr)
{
    fwrite(shdr, sizeof(uint8_t), sizeof(ELFSectionHeader), fp);
}

void 
OMR::ELFGenerator::writeSectionNameToFile(::FILE *fp, char * name, uint32_t size)
{
    fwrite(name, sizeof(uint8_t), size, fp);
}

void 
OMR::ELFGenerator::writeCodeSegmentToFile(::FILE *fp)
{
    fwrite(static_cast<const void *>(_codeStart), sizeof(uint8_t), _codeSize, fp);
}

void 
OMR::ELFGenerator::writeDataSegmentToFile(::FILE *fp)
{
    char temp[] = "[Dummy data in data segment]";
    fwrite(static_cast<const void *>(temp), sizeof(uint8_t), _dataSize, fp);
    return;
}

void 
OMR::ELFGenerator::writeELFSymbolsToFile(::FILE *fp)
{
    
    ELFSymbol * elfSym = static_cast<ELFSymbol*>(_rawAllocator.allocate(sizeof(ELFSymbol)));
    char ELFSymbolNames[_totalELFSymbolNamesLength];
    
    /* Writing the UNDEF symbol*/
    elfSym->st_name = 0;
    elfSym->st_info = 0;
    elfSym->st_other = 0;
    elfSym->st_shndx = 0;
    elfSym->st_value = 0;
    elfSym->st_size = 0;
    fwrite(elfSym, sizeof(uint8_t), sizeof(ELFSymbol), fp);
    
    ELFSymbolNames[0] = 0; //first bit needs to be 0, corresponding to the name of the UNDEF symbol
    char * names = ELFSymbolNames + 1; //the rest of the array will contain method names
    TR::CodeCacheSymbol *sym = _symbols; 
    

    const uint8_t* rangeStart; //relocatable elf files need symbol offset from segment base
    if (_relaSection)
    {
        rangeStart = _codeStart;    
    } 
    else 
    {
        rangeStart = 0;
    }
    
    //values that are unchanged are being kept out of the while loop
    elfSym->st_other = ELF_ST_VISIBILITY(STV_DEFAULT);
    elfSym->st_info = ELF_ST_INFO(STB_GLOBAL,STT_FUNC);
    /* this while loop re-uses the ELFSymbol and writes
       CodeCacheSymbol info into file */
    while (sym)
    {
        memcpy(names, sym->_name, sym->_nameLength);
        elfSym->st_name = names - ELFSymbolNames; 
        elfSym->st_shndx = sym->_start ? 1 : SHN_UNDEF; // text section
        elfSym->st_value = sym->_start ? (ELFAddress)(sym->_start - rangeStart) : 0;
        elfSym->st_size = sym->_size;
        
        fwrite(elfSym, sizeof(uint8_t), sizeof(ELFSymbol), fp);
        
        names += sym->_nameLength;
        sym = sym->_next;
    }
    /* Finally, write the symbol names to file */
    fwrite(ELFSymbolNames, sizeof(uint8_t), _totalELFSymbolNamesLength, fp);

    _rawAllocator.deallocate(elfSym);
}

void 
OMR::ELFGenerator::writeRelaEntriesToFile(::FILE *fp)
{
    if(_numRelocations > 0)
    {
        ELFRela * elfRelas = 
            static_cast<ELFRela*>(_rawAllocator.allocate(sizeof(ELFRela)));
        elfRelas->r_addend = 0; //addends are always 0, so it is kept out of the while loop
        TR::CodeCacheRelocationInfo *reloc = _relocations;
        /* this while loop re-uses the ELFRela and writes
            CodeCacheSymbol info into file */
        while (reloc)
            {
            elfRelas->r_offset = (ELFAddress)(reloc->_location - _codeStart);
            elfRelas->r_info = ELF_R_INFO(reloc->_symbol+1, reloc->_type);
            
            fwrite(elfRelas, sizeof(uint8_t), sizeof(ELFRela), fp);
            
            reloc = reloc->_next;
            }
        
        _rawAllocator.deallocate(elfRelas);
    }
}



//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void
OMR::ELFExecutableGenerator::initialize(void)
{
    ELFEHeader *hdr =
        static_cast<ELFEHeader *>(_rawAllocator.allocate(sizeof(ELFEHeader),
        std::nothrow));
        _header = hdr;
    
    ELFProgramHeader *phdr =
        static_cast<ELFProgramHeader *>(_rawAllocator.allocate(sizeof(ELFProgramHeader),
        std::nothrow));
    
    _programHeader = phdr;

    initializeELFHeader();

    initializeELFHeaderForPlatform();

    initializePHdr();
}

void
OMR::ELFExecutableGenerator::initializeELFHeader(void)
{
    _header->e_type = ET_EXEC;
    _header->e_entry = (ELFAddress) _codeStart; //virtual address to which the system first transfers control
    _header->e_phoff = sizeof(ELFEHeader); //program header offset
    _header->e_shoff = sizeof(ELFEHeader) + sizeof(ELFProgramHeader) + _codeSize; //section header offset
    _header->e_phentsize = sizeof(ELFProgramHeader);
    _header->e_phnum = 1; // number of ELFProgramHeaders
    _header->e_shnum = 5; // number of sections in trailer
    _header->e_shstrndx = 3; // index of section header string table in trailer
}

void
OMR::ELFExecutableGenerator::initializePHdr(void)
{
    _programHeader->p_type = PT_LOAD; //should be loaded in memory
    _programHeader->p_offset = sizeof(ELFEHeader); //offset of program header from the first byte of file to be loaded
    _programHeader->p_vaddr = (ELFAddress) _codeStart; //virtual address to load into
    _programHeader->p_paddr = (ELFAddress) _codeStart; //physical address to load into
    _programHeader->p_filesz = _codeSize; //in-file size
    _programHeader->p_memsz = _codeSize; //in-memory size
    _programHeader->p_flags = PF_X | PF_R; // should add PF_W if we get around to loading patchable code
    _programHeader->p_align = 0x1000;
}

void
OMR::ELFExecutableGenerator::buildSectionHeaders(void)
{
    uint32_t shStrTabNameLength = sizeof(_zeroSectionName) +
                                  sizeof(_shStrTabSectionName) +
                                  sizeof(_textSectionName) +
                                  sizeof(_dynSymSectionName) +
                                  sizeof(_dynStrSectionName);
    
    /* offset calculations */
    uint32_t trailerStartOffset = sizeof(ELFEHeader) + sizeof(ELFProgramHeader) +
                                  _codeSize;
    uint32_t symbolsStartOffset = trailerStartOffset + 
                                  (sizeof(ELFSectionHeader) * /* #shdr */ 5) +
                                  shStrTabNameLength;
    uint32_t symbolNamesStartOffset = symbolsStartOffset +
                                      ((_numSymbols + /* UNDEF */ 1) * sizeof(ELFSymbol));
    uint32_t shNameOffset = 0;

    initializeZeroSection();
    shNameOffset += sizeof(_zeroSectionName);

    initializeTextSection(shNameOffset,
                          (ELFAddress) _codeStart,
                          sizeof(ELFEHeader) + sizeof(ELFProgramHeader), 
                          _codeSize);
    shNameOffset += sizeof(_textSectionName);

    initializeDynSymSection(shNameOffset,
                            symbolsStartOffset, 
                            symbolNamesStartOffset - symbolsStartOffset,
                            /* Index of dynStrTab */ 4);
    shNameOffset += sizeof(_dynSymSectionName);
    
    initializeStrTabSection(shNameOffset, 
                            symbolsStartOffset - shStrTabNameLength, 
                            shStrTabNameLength);
    shNameOffset += sizeof(_shStrTabSectionName);

    initializeDynStrSection(shNameOffset, 
                            symbolNamesStartOffset, 
                            _totalELFSymbolNamesLength);
    shNameOffset += sizeof(_dynStrSectionName);
}

bool
OMR::ELFExecutableGenerator::emitELF(const char * filename,
                TR::CodeCacheSymbol *symbols, uint32_t numSymbols,
                uint32_t totalELFSymbolNamesLength)
{
    _symbols = symbols;
    _numSymbols = numSymbols;
    _totalELFSymbolNamesLength = totalELFSymbolNamesLength;
    
    buildSectionHeaders();
    return emitELFFile(filename);
}


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void
OMR::ELFRelocatableGenerator::initialize(void)
{
    ELFEHeader *hdr =
        static_cast<ELFEHeader *>(_rawAllocator.allocate(sizeof(ELFEHeader),
        std::nothrow));
    _header = hdr;
    
    initializeELFHeader();
    
    initializeELFHeaderForPlatform();
}

void
OMR::ELFRelocatableGenerator::initializeELFHeader(void)
{
    _header->e_type = ET_REL;           
    _header->e_entry = 0; //no associated entry point for relocatable ELF files
    _header->e_phoff = 0; //no program header for relocatable files
    _header->e_shoff = sizeof(ELFEHeader) + _codeSize; //start of the section header table in bytes from the first byte of the ELF file
    _header->e_phentsize = 0; //no program headers in relocatable elf
    _header->e_phnum = 0;
    _header->e_shnum = 7;
    _header->e_shstrndx = 5; //index of section header string table
}

void
OMR::ELFRelocatableGenerator::buildSectionHeaders(void)
{    
    uint32_t shStrTabNameLength = sizeof(_zeroSectionName) +
                                  sizeof(_shStrTabSectionName) +
                                  sizeof(_textSectionName) +
                                  sizeof(_dataSectionName) +
                                  sizeof(_relaSectionName) +
                                  sizeof(_dynSymSectionName) +
                                  sizeof(_dynStrSectionName);

    /* offset calculations */
    uint32_t trailerStartOffset = sizeof(ELFEHeader) + _codeSize;
    uint32_t symbolsStartOffset = trailerStartOffset +
                                  (sizeof(ELFSectionHeader) * /* # shdr */ 7) +
                                  shStrTabNameLength;
    uint32_t symbolNamesStartOffset = symbolsStartOffset + 
                                      (_numSymbols + /* UNDEF */ 1) * sizeof(ELFSymbol);
    uint32_t relaStartOffset = symbolNamesStartOffset + _totalELFSymbolNamesLength;
    uint32_t shNameOffset = 0;

    initializeZeroSection();
    shNameOffset += sizeof(_zeroSectionName);

    initializeTextSection(shNameOffset,
                          /*sh_addr*/ 0,
                          sizeof(ELFEHeader),
                          _codeSize);
    shNameOffset += sizeof(_textSectionName);

    initializeDataSection(shNameOffset,
                          /*sh_addr*/0,
                          sizeof(ELFEHeader) + _codeSize,
                          0);
    shNameOffset += sizeof(_dataSectionName);

    initializeRelaSection(shNameOffset,
                          relaStartOffset, 
                          _numRelocations * sizeof(ELFRela));
    shNameOffset += sizeof(_relaSectionName);

    initializeDynSymSection(shNameOffset, 
                            symbolsStartOffset,
                            symbolNamesStartOffset - symbolsStartOffset,
                            /*Index of dynStrTab*/ 6);
    shNameOffset += sizeof(_dynSymSectionName);

    initializeStrTabSection(shNameOffset, 
                            symbolsStartOffset - shStrTabNameLength, 
                            shStrTabNameLength);
    shNameOffset += sizeof(_shStrTabSectionName);

    initializeDynStrSection(shNameOffset,
                            symbolNamesStartOffset, 
                            _totalELFSymbolNamesLength);
    shNameOffset += sizeof(_dynStrSectionName);
}

bool 
OMR::ELFRelocatableGenerator::emitELF(const char * filename,
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


//===============================================================================================================================================================================
//===============================================================================================================================================================================
//===============================================================================================================================================================================
void
OMR::ELFSharedObjectGenerator::initialize(void)
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
OMR::ELFSharedObjectGenerator::writeCodeSegmentToFile(::FILE *fp)
{
    fwrite(static_cast<const void *>(_codeSegmentStart), sizeof(uint8_t), _codeSize, fp);
}

OMR::ELFSharedObjectGenerator::ELFProgramHeader *
OMR::ELFSharedObjectGenerator::initializeProgramHeader(uint32_t type, ELFOffset offset, ELFAddress vaddr, ELFAddress paddr, 
                                                uint32_t filesz, uint32_t memsz, uint32_t flags, uint32_t align)
{
   ELFProgramHeader *phdr =
    static_cast<ELFProgramHeader *>(_rawAllocator.allocate(sizeof(ELFProgramHeader),
    std::nothrow)); 

    phdr->p_type = type; //should be loaded in memory
    phdr->p_offset = offset; //offset of program header from the first byte of file to be loaded
    phdr->p_vaddr = vaddr; //(ELFAddress) _codeStart; //virtual address to load into
    phdr->p_paddr = paddr; //(ELFAddress) _codeStart; //physical address to load into
    phdr->p_filesz = filesz;//_codeSize; //in-file size
    phdr->p_memsz = memsz;//_codeSize; //in-memory size
    phdr->p_flags = flags; // should add PF_W if we get around to loading patchable code
    phdr->p_align = align;
    
    return phdr;
}

void
OMR::ELFSharedObjectGenerator::buildProgramHeaders()
{
    _programHeaderLoadRX = initializeProgramHeader(  PT_LOAD, //should be loaded in memory
                                            0, //offset of program header from the first byte of file to be loaded
                                            0, //(ELFAddress) _codeStart; //virtual address to load into
                                            0, //(ELFAddress) _codeStart; //physical address to load into
                                            (Elf64_Xword) (sectionOffsetMap[_dataSectionName] ),//_codeSize; //in-file size
                                            (Elf64_Xword) (sectionOffsetMap[_dataSectionName] ),//_codeSize; //in-memory size
                                            PF_X | PF_R, // should add PF_W if we get around to loading patchable code
                                            0x200000);

    _programHeaderLoadRW = initializeProgramHeader(  PT_LOAD, //should be loaded in memory
                                            (ELFOffset) sectionOffsetMap[_dataSectionName], //offset of program header from the first byte of file to be loaded
                                            (ELFAddress) (sectionOffsetMap[_dataSectionName] + 0x200000), //(ELFAddress) _codeStart; //virtual address to load into
                                            (ELFAddress) (sectionOffsetMap[_dataSectionName] + 0x200000), //(ELFAddress) _codeStart; //physical address to load into
                                            (Elf64_Xword) (_dataSize + (sizeof(ELFDynamic) * numOfDynamicEntries)),//_codeSize; //in-file size
                                            (Elf64_Xword) (_dataSize + (sizeof(ELFDynamic) * numOfDynamicEntries)),//_codeSize; //in-memory size
                                            PF_R | PF_W, // should add PF_W if we get around to loading patchable code
                                            0x200000);

    _programHeaderDynamic = initializeProgramHeader(  PT_DYNAMIC, //should be loaded in memory
                                            (ELFOffset) sectionOffsetMap[_dynamicSectionName], //offset of program header from the first byte of file to be loaded
                                            (ELFAddress) (sectionOffsetMap[_dynamicSectionName] + 0x200000), //virtual address to load into
                                            (ELFAddress) (sectionOffsetMap[_dynamicSectionName] + 0x200000), //physical address to load into
                                            (Elf64_Xword) (sizeof(ELFDynamic) * numOfDynamicEntries), //in-file size
                                            (Elf64_Xword) (sizeof(ELFDynamic) * numOfDynamicEntries), //in-memory size
                                            PF_R | PF_W, // should add PF_W if we get around to loading patchable code
                                            0x8);
}

void
OMR::ELFSharedObjectGenerator::initializeELFHeader(void)
{
    //printf("\n In initializeELFHeader\n");
    _header->e_type = ET_DYN;           
    _header->e_entry = (ELFAddress) _codeStart; //no associated entry point for shared object ELF files
    _header->e_phoff = sizeof(ELFEHeader); //program header for shared object files
    _header->e_shoff = sizeof(ELFEHeader) + ( sizeof(ELFProgramHeader) * numOfProgramHeaders ) + _codeSize; //start of the section header table in bytes from the first byte of the ELF file
    _header->e_phentsize = sizeof(ELFProgramHeader); //program header size
    _header->e_phnum = numOfProgramHeaders; //No of program headers
    _header->e_shnum = numOfSections; //No of section headers
    _header->e_shstrndx = 2; //index of section header string table
    //printf("\n Out initializeELFHeader\n");
}

void 
OMR::ELFSharedObjectGenerator::setCodeSegmentDetails(uint8_t *codeStart, uint32_t codeSize, uint32_t numSymbols, uint32_t totalELFSymbolNamesLength)
{
    _codeSegmentStart = codeStart;
    _codeSize = codeSize; 
    _numSymbols = numSymbols;
    _totalELFSymbolNamesLength = totalELFSymbolNamesLength;
}

void
OMR::ELFSharedObjectGenerator::initializeSectionNames(void)
{ 
    TR_UNIMPLEMENTED();
    //printf("\n In initializeSectionNames\n");
    _zeroSectionName[0] = 0;
    shStrTabNameLength = 0;

    strcpy(_shStrTabSectionName, ".shstrtab");
    strcpy(_textSectionName, ".text");
    strcpy(_dynSymSectionName, ".dynsym");
    strcpy(_dynStrSectionName, ".dynstr");
    strcpy(_hashSectionName, ".hash");
    strcpy(_dataSectionName, ".data");
    strcpy(_dynamicSectionName, ".dynamic");
    
    shStrTabNameLength = sizeof(_zeroSectionName) +
                                  sizeof(_shStrTabSectionName) +
                                  sizeof(_textSectionName) +
                                  sizeof(_dynSymSectionName) +
                                  sizeof(_dynStrSectionName) +
                                  sizeof(_hashSectionName) +
                                  sizeof(_dataSectionName) +
                                  sizeof(_dynamicSectionName);

    //printf("\n shStrTabNameLength %d \n ",shStrTabNameLength);                            
}

void
OMR::ELFSharedObjectGenerator::initializeSectionOffsets(void)
{ 
    //printf("\n In initializeSectionOffsets\n");
    sectionOffsetMap[_textSectionName] = sizeof(ELFEHeader) + (sizeof(ELFProgramHeader) * numOfProgramHeaders);
    //printf("\n textSectionStartOffset %d \n ",textSectionStartOffset);
    
    sectionOffsetMap[_shStrTabSectionName] = sizeof(ELFEHeader) + ( sizeof(ELFProgramHeader) *numOfProgramHeaders ) + _codeSize +   
                                             (sizeof(ELFSectionHeader) * /* # shdr */ numOfSections);
    
    sectionOffsetMap[_dynSymSectionName]  =  sectionOffsetMap[_shStrTabSectionName] +
                                                shStrTabNameLength;
    //printf("\n dynsymSectionStartOffset %d \n ",dynsymSectionStartOffset);

    sectionOffsetMap[_dynStrSectionName]  = sectionOffsetMap[_dynSymSectionName] + 
                                            (_numSymbols + /* UNDEF */ 2) * sizeof(ELFSymbol);

    //printf("\n _totalELFSymbolNamesLength %d \n ",_totalELFSymbolNamesLength);
    sectionOffsetMap[_hashSectionName] =  sectionOffsetMap[_dynStrSectionName] + _totalELFSymbolNamesLength;   

    sectionOffsetMap[_dataSectionName] = sectionOffsetMap[_hashSectionName] + (sizeof(uint32_t) * (2 + nchain + nbucket));

    sectionOffsetMap[_dynamicSectionName] = sectionOffsetMap[_dataSectionName] + _dataSize;  

}

OMR::ELFSharedObjectGenerator::ELFSectionHeader * 
OMR::ELFSharedObjectGenerator::initializeSection(uint32_t shName, uint32_t shType, uint32_t shFlags, ELFAddress shAddress,
                                                 ELFOffset shOffset,  uint32_t shSize, uint32_t shLink, uint32_t shInfo, uint32_t shAddralign, uint32_t shEntsize)
{
    ELFSectionHeader * shdr = static_cast<ELFSectionHeader *>(_rawAllocator.allocate(sizeof(ELFSectionHeader)));
    
    shdr->sh_name = shName;
    shdr->sh_type = shType;
    shdr->sh_flags = shFlags;//SHF_ALLOC | SHF_EXECINSTR;
    shdr->sh_addr = shAddress;//shAddress;
    shdr->sh_offset = shOffset;
    shdr->sh_size = shSize;
    shdr->sh_link = shLink;
    shdr->sh_info = shInfo;
    shdr->sh_addralign = shAddralign;
    shdr->sh_entsize = shEntsize;
    return shdr;               
}


void
OMR::ELFSharedObjectGenerator::buildSectionHeaders(void)
{    
    uint32_t shNameOffset = 0;
    initializeZeroSection();
    shNameOffset += sizeof(_zeroSectionName);

    _AotCDSection = initializeSection(shNameOffset,
						   SHT_PROGBITS,
                           SHF_ALLOC | SHF_WRITE | SHF_EXECINSTR, 
                           (ELFAddress) sectionOffsetMap[_textSectionName],
                           (ELFOffset) sectionOffsetMap[_textSectionName],
                           _codeSize,
                           0,
                           0,
                           16,
                           0);
    shNameOffset += sizeof(_textSectionName);

    _shStrTabSection = initializeSection(shNameOffset, 
                            SHT_STRTAB,
                            0,
                            (ELFAddress) sectionOffsetMap[_shStrTabSectionName],
                            (ELFOffset) sectionOffsetMap[_shStrTabSectionName], 
                            shStrTabNameLength,
                            0,
                            0,
                            1,
                            0);
    shNameOffset += sizeof(_shStrTabSectionName);

    _dynSymSection = initializeSection(shNameOffset,
                            SHT_DYNSYM,
                            0, 
                            (ELFAddress) sectionOffsetMap[_dynSymSectionName],
                            sectionOffsetMap[_dynSymSectionName],
                            sectionOffsetMap[_dynStrSectionName] - sectionOffsetMap[_dynSymSectionName],
                            4, //Index of dynStrTab
                            1,
                            TR::Compiler->target.is64Bit() ? 8 : 4,
                            sizeof(ELFSymbol));
    shNameOffset += sizeof(_dynSymSectionName);

    _dynStrSection = initializeSection(shNameOffset,
                            SHT_STRTAB,
                            0,
                            (ELFAddress) sectionOffsetMap[_dynStrSectionName],
                            sectionOffsetMap[_dynStrSectionName], 
                            _totalELFSymbolNamesLength,
                            0,
                            0,
                            1,
                            0);
    shNameOffset += sizeof(_dynStrSectionName);    

    _hashSection = initializeSection(shNameOffset,
                          SHT_HASH,
                          SHF_ALLOC,
                          (ELFAddress) sectionOffsetMap[_hashSectionName], 
                          sectionOffsetMap[_hashSectionName], 
                          sizeof(uint32_t) * (2 + nchain + nbucket),
                          3, //.dynsym section index
                          0,
                          TR::Compiler->target.is64Bit() ? 8 : 4,
                          0);
    shNameOffset += sizeof(_hashSectionName);

    _dataSection = initializeSection(shNameOffset,
                          SHT_PROGBITS,
                          SHF_ALLOC | SHF_WRITE,
                          (ELFAddress) sectionOffsetMap[_dataSectionName] + (ELFAddress) 0x200000,
                          sectionOffsetMap[_dataSectionName],
                          _dataSize,
                          0,
                          0,
                          TR::Compiler->target.is64Bit() ? 8 : 4,
                          0);
    shNameOffset += sizeof(_dataSectionName);

    _dynamicSection = initializeSection(shNameOffset,
                            SHT_DYNAMIC,
                            0,
                            (ELFAddress) sectionOffsetMap[_dynamicSectionName], 
                            sectionOffsetMap[_dynamicSectionName], 
                            sizeof(ELFDynamic) * numOfDynamicEntries,
                            2, 
                            0,
                            TR::Compiler->target.is64Bit() ? 8 : 4,
                            sizeof(ELFDynamic));
    shNameOffset += sizeof(_dynamicSectionName);
}

bool 
OMR::ELFSharedObjectGenerator::emitELFSO(const char * filename)
{
    //_symbols = symbols;
    //_relocations = relocations;
    //TR::Compiler->aotLoadStoreDriver->_methodNameToHeaderMap;
    
    //printf("\n In emitAOTELF\n");

    char temp[] = "[Dummy data in data segment]";
    _dataSize = sizeof(temp);
    //printf("\n Size of data = %d", _dataSize);

    initializeHashValues(_numSymbols);
    //printf("\n After initializeHashValues\n");
    
    initializeSectionNames();
    //printf("\n After initializeSectionNames\n");
    
    initializeSectionOffsets();

    buildProgramHeaders();

    buildSectionHeaders();
    //printf("\n Before emitELFFile\n");
    bool val = emitELFFile(filename);
    //printf("\n Last\n");
    return val;
}


bool
OMR::ELFSharedObjectGenerator::emitELFFile(const char * filename)
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

    writeSectionHeaderToFile(elfFile, _textSection);

   // if (_relaSection)
   // {
   //     writeSectionHeaderToFile(elfFile, _relaSection);
   // }

    writeSectionHeaderToFile(elfFile, _shStrTabSection);

    writeSectionHeaderToFile(elfFile, _dynSymSection);

    writeSectionHeaderToFile(elfFile, _dynStrSection);

    writeSectionHeaderToFile(elfFile, _hashSection);

    writeSectionHeaderToFile(elfFile, _dataSection);
    
    writeSectionHeaderToFile(elfFile, _dynamicSection);

    writeSectionNameToFile(elfFile, _zeroSectionName, sizeof(_zeroSectionName));

    writeSectionNameToFile(elfFile, _textSectionName, sizeof(_textSectionName));
    
    //if (_relaSection)
   // {
   //     writeSectionNameToFile(elfFile, _relaSectionName, sizeof(_relaSectionName));
  //  } 
    writeSectionNameToFile(elfFile, _shStrTabSectionName, sizeof(_shStrTabSectionName));
    
    writeSectionNameToFile(elfFile, _dynSymSectionName, sizeof(_dynSymSectionName));

    writeSectionNameToFile(elfFile, _dynStrSectionName, sizeof(_dynStrSectionName));

    writeSectionNameToFile(elfFile, _hashSectionName, sizeof(_hashSectionName));

    writeSectionNameToFile(elfFile, _dataSectionName, sizeof(_dataSectionName));

    writeSectionNameToFile(elfFile, _dynamicSectionName, sizeof(_dynamicSectionName));
    
    processAllSymbols(elfFile);

    calculateHashValues();

    writeSysVHashTable(elfFile);

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
OMR::ELFSharedObjectGenerator::writeProgramHeaderToFile(::FILE *fp)
{
    fwrite(_programHeaderLoadRX, sizeof(uint8_t), sizeof(ELFProgramHeader), fp);
    fwrite(_programHeaderLoadRW, sizeof(uint8_t), sizeof(ELFProgramHeader), fp);
    fwrite(_programHeaderDynamic, sizeof(uint8_t), sizeof(ELFProgramHeader), fp);
}

void 
OMR::ELFSharedObjectGenerator::processAllSymbols(::FILE *fp)
{
    //printf("\n In writeAOTELFSymbolsToFile\n");
    ELFSymbol * elfSym = static_cast<ELFSymbol*>(_rawAllocator.allocate(sizeof(ELFSymbol)));
    char ELFSymbolNames[_totalELFSymbolNamesLength];
    //uint8_t hash_index = 0;
    /* Writing the UNDEF symbol*/
    writeSymbolToFile(fp, elfSym, 0, 0, 0, 0, 0, 0);

    ELFSymbolNames[0] = 0; //first bit needs to be 0, corresponding to the name of the UNDEF symbol
    char * names = ELFSymbolNames + 1; 
    //hash_index++;

    TR_UNIMPLEMENTED();
    /* Writing the DYNAMIC symbol*/

    char const * dynamicSymbolName = "_DYNAMIC";
    memcpy(names, dynamicSymbolName, strlen(dynamicSymbolName) + 1);
    
    writeSymbolToFile(fp,
                      elfSym, 
                      names - ELFSymbolNames, 
                      ELF_ST_INFO(STB_LOCAL,STT_OBJECT), 
                      ELF_ST_VISIBILITY(STV_DEFAULT), 
                      1, 
                      sectionOffsetMap[_dynamicSectionName], 
                      0);


    names += (strlen(dynamicSymbolName) + 1);

    fwrite(ELFSymbolNames, sizeof(uint8_t), _totalELFSymbolNamesLength, fp);

    _rawAllocator.deallocate(elfSym);
    //printf("\n Out writeAOTELFSymbolsToFile\n");
}

//ELFSymbol *elfSym can be alllocate and deallocate din the constructors/destructor
void 
OMR::ELFSharedObjectGenerator::writeSymbolToFile(::FILE *fp, ELFSymbol *elfSym, uint32_t st_name, unsigned char st_info, unsigned char st_other, ELFSection st_shndx, ELFAddress st_value, uint64_t st_size)
{
    elfSym->st_name = st_name;
    elfSym->st_info = st_info;
    elfSym->st_other = st_other;
    elfSym->st_shndx = st_shndx;
    elfSym->st_value = st_value;
    elfSym->st_size = st_size;
    fwrite(elfSym, sizeof(uint8_t), sizeof(ELFSymbol), fp);
}

void
OMR::ELFSharedObjectGenerator::writeDynamicSectionEntries(::FILE *fp)
{
    //printf(" \n In writeDynamicSectionEntries \n");
    ELFDynamic * dynEntry = static_cast<ELFDynamic*>(_rawAllocator.allocate(sizeof(ELFDynamic)));
    //.dynsym Entry
    writeDynamicEntryToFile(fp, dynEntry, DT_SYMTAB, 0, sectionOffsetMap[_dynSymSectionName], 0);
    //.dynstr Entry
    writeDynamicEntryToFile(fp, dynEntry, DT_STRTAB, 0, sectionOffsetMap[_dynStrSectionName], 0);
    //.hash Entry
    writeDynamicEntryToFile(fp, dynEntry, DT_HASH, 0, sectionOffsetMap[_hashSectionName], 0);

    writeDynamicEntryToFile(fp, dynEntry, DT_STRSZ, _totalELFSymbolNamesLength, 0, 1);

    writeDynamicEntryToFile(fp, dynEntry, DT_SYMENT, sizeof(ELFSymbol) * (_numSymbols+2), 0, 1);
    //NULL Entry
    writeDynamicEntryToFile(fp, dynEntry, DT_NULL, 0, 0, 0);
    //.rela.text Entry
   
    _rawAllocator.deallocate(dynEntry);
    //printf(" \n Out writeDynamicSectionEntries \n");
}

void
OMR::ELFSharedObjectGenerator::writeDynamicEntryToFile(::FILE *fp, ELFDynamic * dynEntry, Elf64_Sxword tag, uint32_t value, uint64_t ptr, uint32_t flag)
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
OMR::ELFSharedObjectGenerator::elfHashSysV(const char* symbolName)
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
OMR::ELFSharedObjectGenerator::calculateHashValues()
{
    TR_UNIMPLEMENTED();
}

void 
OMR::ELFSharedObjectGenerator::writeSysVHashTable(::FILE *fp)
{
    for(uint32_t i = 0; i < nbucket; i++) 
        {
        uint32_t flag = 0, temp = 0;
        for(uint32_t j = 1; j < nchain; j++) 
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

   /*  for(int i = 0; i < nchain; i++) 
        {
        printf("\n hash_array[%d] = %u mod_array[%d] = %u chain_array[%u] = %u  ",i,hash_array[i],i,mod_array[i],i,chain_array[i]);
        }
    for(int i = 0; i < nbucket; i++) 
        {
        printf("\n bucket_array[%d] = %u",i,bucket_array[i]);
       } */
    
    uint32_t written = 0;
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
OMR::ELFSharedObjectGenerator::getNoOfBuckets(uint32_t numSymbols)
{
    static const size_t elfBuckets[] = {0, 1, 3, 17, 37, 67, 97, 131, 197, 263, 521, 1031, 2053, 4099, 8209, 16411, 32771};
    size_t n = sizeof(elfBuckets) / sizeof(elfBuckets[0]);
    for(uint32_t i = 0; i < n; i++)
    {
        if(elfBuckets[i] > numSymbols)
        {
            //printf("\n elfBuckets[i] = [%d]",elfBuckets[i]);
            //printf("\n elfBuckets[i-1] = [%d]",elfBuckets[i-1]);
            return elfBuckets[i-1];     
        }
    } 
    return 0;
}

void 
OMR::ELFSharedObjectGenerator::initializeHashValues(uint32_t numSymbols)
{
    nbucket = (uint32_t)getNoOfBuckets(numSymbols + 2);
    nchain = numSymbols + 2;
    bucket_array = static_cast<uint32_t*>(_rawAllocator.allocate(sizeof(uint32_t) * nbucket));
    chain_array = static_cast<uint32_t*>(_rawAllocator.allocate(sizeof(uint32_t) * nchain));
    hash_array = static_cast<uint32_t*>(_rawAllocator.allocate(sizeof(uint32_t) * nchain));
    mod_array = static_cast<uint32_t*>(_rawAllocator.allocate(sizeof(uint32_t) * nchain));
    
    for(uint32_t i = 0; i < nchain; i++) {
        chain_array[i] = 0;
        hash_array[i] = 0;
        mod_array[i] = 0;
        }
    for(uint32_t i = 0; i < nbucket; i++) {
        bucket_array[i] = 0;
        }
}

#endif //LINUX
