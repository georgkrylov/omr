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
                            ELFGenerator(rawAllocator, codeStart, codeSize)
                            {
                                initialize();
                            }

OMR::ELFSharedObjectGenerator::ELFSharedObjectGenerator(TR::RawAllocator rawAllocator):
                            ELFGenerator(rawAllocator,0,0)
                            {
                                initialize();
                            }

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
    ELFEHeader *hdr =
    static_cast<ELFEHeader *>(_rawAllocator.allocate(sizeof(ELFEHeader),
    std::nothrow));
    _header = hdr;

    initializeELFHeader();
    
    initializeELFHeaderForPlatform();

}

void 
OMR::ELFSharedObjectGenerator::initializeTextSection(uint32_t shName, ELFAddress shAddress,
                                                 ELFOffset shOffset, uint32_t shSize)
{

    ELFSectionHeader * shdr = static_cast<ELFSectionHeader *>(_rawAllocator.allocate(sizeof(ELFSectionHeader)));
    
    shdr->sh_name = shName;
    shdr->sh_type = SHT_PROGBITS;
    shdr->sh_flags = SHF_ALLOC | SHF_EXECINSTR;
    shdr->sh_addr = shAddress;//shAddress;
    shdr->sh_offset = shOffset;
    shdr->sh_size = shSize;
    shdr->sh_link = 0;
    shdr->sh_info = 0;
    shdr->sh_addralign = 16;
    shdr->sh_entsize = 0;

    _textSection = shdr;
    strcpy(_textSectionName, ".text");
}

void 
OMR::ELFSharedObjectGenerator::initializeDataSection(uint32_t shName, ELFAddress shAddress,
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
OMR::ELFSharedObjectGenerator::initializeDynSymSection(uint32_t shName, ELFAddress shAddress, ELFOffset shOffset, uint32_t shSize, uint32_t shLink)
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
OMR::ELFSharedObjectGenerator::initializeStrTabSection(uint32_t shName, ELFAddress shAddress, ELFOffset shOffset, uint32_t shSize)
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
OMR::ELFSharedObjectGenerator::initializeDynStrSection(uint32_t shName, ELFAddress shAddress, ELFOffset shOffset, uint32_t shSize)
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
OMR::ELFSharedObjectGenerator::initializeRelaSection(uint32_t shName, ELFAddress shAddress, ELFOffset shOffset, uint32_t shSize)
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
OMR::ELFSharedObjectGenerator::initializeDynamicSection(uint32_t shName, ELFAddress shAddress, ELFOffset shOffset, uint32_t shSize)
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
OMR::ELFSharedObjectGenerator::initializeHashSection(uint32_t shName, ELFAddress shAddress, ELFOffset shOffset, uint32_t shSize)
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
    shdr->sh_addralign = TR::Compiler->target.is64Bit() ? 8 : 4;  ;    
    shdr->sh_entsize = 0;

    _hashSection = shdr;
    strcpy(_hashSectionName, ".hash");
}

void
OMR::ELFSharedObjectGenerator::initializePHdr(void)
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

    _programHeaderLoadRX->p_type = PT_LOAD; //should be loaded in memory
    _programHeaderLoadRX->p_offset = 0; //offset of program header from the first byte of file to be loaded
    _programHeaderLoadRX->p_vaddr = 0; //(ELFAddress) _codeStart; //virtual address to load into
    _programHeaderLoadRX->p_paddr = 0; //(ELFAddress) _codeStart; //physical address to load into
    _programHeaderLoadRX->p_filesz = (Elf64_Xword) (dynamicSectionStartOffset );//_codeSize; //in-file size
    _programHeaderLoadRX->p_memsz = (Elf64_Xword) (dynamicSectionStartOffset );//_codeSize; //in-memory size
    _programHeaderLoadRX->p_flags = PF_X | PF_R; // should add PF_W if we get around to loading patchable code
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
}

void
OMR::ELFSharedObjectGenerator::initializeELFHeader(void)
{
    _header->e_type = ET_DYN;           
    _header->e_entry = (ELFAddress) _textSegmentStart; //no associated entry point for shared object ELF files
    _header->e_phoff = sizeof(ELFEHeader); //program header for shared object files
    _header->e_shoff = sizeof(ELFEHeader) + (sizeof(ELFProgramHeader)*3) + _codeSize; //start of the section header table in bytes from the first byte of the ELF file
    _header->e_phentsize = sizeof(ELFProgramHeader); //no program headers in shared object elf
    _header->e_phnum = 3;
    _header->e_shnum = 8; //6
    _header->e_shstrndx = 3; //index of section header string table
}

void
OMR::ELFSharedObjectGenerator::initializeSectionNames(void)
{ 
    _zeroSectionName[0] = 0;
    strcpy(_shStrTabSectionName, ".shstrtab");
    strcpy(_textSectionName, ".text");
    //strcpy(_relaSectionName, ".rela.text");
    strcpy(_dynSymSectionName, ".dynsym");
    strcpy(_dynStrSectionName, ".dynstr");
    strcpy(_hashSectionName, ".hash");
    strcpy(_dataSectionName, ".data");
    strcpy(_dynamicSectionName, ".dynamic");
    
    shStrTabNameLength = sizeof(_zeroSectionName) +
                                  sizeof(_shStrTabSectionName) +
                                  sizeof(_textSectionName) +
                                  //sizeof(_relaSectionName) +
                                  sizeof(_dynSymSectionName) +
                                  sizeof(_dynStrSectionName) +
                                  sizeof(_hashSectionName) +
                                  sizeof(_dataSectionName) +
                                  sizeof(_dynamicSectionName);

}

void
OMR::ELFSharedObjectGenerator::initializeSectionOffsets(void)
{ 
    dynamicSectionStartOffset = 0;
    
    textSectionStartOffset = sizeof(ELFEHeader) + (sizeof(ELFProgramHeader)*3);
    //dataSectionStartOffset = sizeof(ELFEHeader) + (sizeof(ELFProgramHeader)*2) + _codeSize;
    
    dynsymSectionStartOffset /* symbolsStartOffset */ = sizeof(ELFEHeader) + (sizeof(ELFProgramHeader)*3) + _codeSize +   
                                  (sizeof(ELFSectionHeader) * /* # shdr */ 8) +
                                  shStrTabNameLength;
    
    dynstrSectionStartOffset /*symbolNamesStartOffset */ = dynsymSectionStartOffset + 
                                      (_numSymbols + /* UNDEF */ 2) * sizeof(ELFSymbol);
    
    hashSectionStartOffset =  dynstrSectionStartOffset + _totalELFSymbolNamesLength;   
    
    //uint32_t relaStartOffset = symbolNamesStartOffset + _totalELFSymbolNamesLength;
    dataSectionStartOffset = hashSectionStartOffset + (sizeof(uint32_t) * (2 + nchain + nbucket));
    
    dynamicSectionStartOffset = dataSectionStartOffset + _dataSize;  
}

void
OMR::ELFSharedObjectGenerator::buildSectionHeaders(void)
{    
    shStrTabNameLength = 0;
    dynamicSectionStartOffset = 0;
    shStrTabNameLength = sizeof(_zeroSectionName) +
                                  sizeof(_shStrTabSectionName) +
                                  sizeof(_textSectionName) +
                                  //sizeof(_relaSectionName) +
                                  sizeof(_dynSymSectionName) +
                                  sizeof(_dynStrSectionName) +
                                  sizeof(_hashSectionName) +
                                  sizeof(_dataSectionName) +
                                  sizeof(_dynamicSectionName);
                                  

    /* offset calculations */
    uint32_t trailerStartOffset = sizeof(ELFEHeader) + (sizeof(ELFProgramHeader)*3) +_codeSize;
    uint32_t symbolsStartOffset = trailerStartOffset +
                                  (sizeof(ELFSectionHeader) * /* # shdr */ 8) +
                                  shStrTabNameLength;
    uint32_t symbolNamesStartOffset = symbolsStartOffset + 
                                      (_numSymbols + /* UNDEF */ 2) * sizeof(ELFSymbol);
    //uint32_t relaStartOffset = symbolNamesStartOffset + _totalELFSymbolNamesLength;
    uint32_t shNameOffset = 0;
    dynamicSectionStartOffset = dataSectionStartOffset + _dataSize;
    initializeZeroSection();
    shNameOffset += sizeof(_zeroSectionName);

    initializeTextSection(shNameOffset,
                          (ELFAddress) textSectionStartOffset,
                          sizeof(ELFEHeader) + (sizeof(ELFProgramHeader)*3),
                          _codeSize);
    shNameOffset += sizeof(_textSectionName);

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
OMR::ELFSharedObjectGenerator::emitELF(const char * filename,
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
OMR::ELFSharedObjectGenerator::emitAOTELF(const char * filename,
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
    

    char temp[] = "[Dummy data in data segment]";
    _dataSize = sizeof(temp);

    initializeHashValues(_numSymbols);
    
    initializeSectionNames();
    
    initializeSectionOffsets();

    initializePHdr();

    buildSectionHeaders();
    bool val = emitAOTELFFile(filename);
    return val;
}

void 
OMR::ELFSharedObjectGenerator::writeCodeSegmentToFile(::FILE *fp)
{
    fwrite(static_cast<const void *>(_textSegmentStart), sizeof(uint8_t), _codeSize, fp);
}

bool
OMR::ELFSharedObjectGenerator::emitAOTELFFile(const char * filename)
{
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

    writeSectionHeaderToFile(elfFile, _dynSymSection);

    writeSectionHeaderToFile(elfFile, _shStrTabSection);

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
    writeSectionNameToFile(elfFile, _dynSymSectionName, sizeof(_dynSymSectionName));

    writeSectionNameToFile(elfFile, _shStrTabSectionName, sizeof(_shStrTabSectionName));

    writeSectionNameToFile(elfFile, _dynStrSectionName, sizeof(_dynStrSectionName));

    writeSectionNameToFile(elfFile, _hashSectionName, sizeof(_hashSectionName));

    writeSectionNameToFile(elfFile, _dataSectionName, sizeof(_dataSectionName));

    writeSectionNameToFile(elfFile, _dynamicSectionName, sizeof(_dynamicSectionName));
    
    writeAOTELFSymbolsToFile(elfFile);

    writeHashSectionToFile(elfFile);

    writeDataSegmentToFile(elfFile);

    writeDynamicSectionEntries(elfFile);
    
    //writeELFSymbolsToFile(elfFile);
   // if(_relaSection)
   // {
   //     writeRelaEntriesToFile(elfFile);
   // }
    char str[] = "ELFEnd";
    fwrite(str , 1 , sizeof(str) , elfFile );
    fclose(elfFile);
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
OMR::ELFSharedObjectGenerator::writeAOTELFSymbolsToFile(::FILE *fp)
{
    ELFSymbol * elfSym = static_cast<ELFSymbol*>(_rawAllocator.allocate(sizeof(ELFSymbol)));
    char ELFSymbolNames[_totalELFSymbolNamesLength];
    //uint8_t hash_index = 0;
    /* Writing the UNDEF symbol*/
    elfSym->st_name = 0;
    elfSym->st_info = 0;
    elfSym->st_other = 0;
    elfSym->st_shndx = 0;
    elfSym->st_value = 0;
    elfSym->st_size = 0;
    fwrite(elfSym, sizeof(uint8_t), sizeof(ELFSymbol), fp);
    //hash_index++;

    ELFSymbolNames[0] = 0; //first bit needs to be 0, corresponding to the name of the UNDEF symbol
    char * names = ELFSymbolNames + 1; //the rest of the array will contain method names
   // TR::CodeCacheSymbol *sym = _symbols; 
    
    /* Writing the DYNAMIC symbol*/
    char const * dynamicSymbolName = "_DYNAMIC";
    memcpy(names, dynamicSymbolName, strlen(dynamicSymbolName) + 1);
    
    elfSym->st_name = names - ELFSymbolNames;
    elfSym->st_info = ELF_ST_INFO(STB_LOCAL,STT_OBJECT);
    elfSym->st_other = ELF_ST_VISIBILITY(STV_DEFAULT);
    elfSym->st_shndx = 2;
    elfSym->st_value = dynamicSectionStartOffset;
    elfSym->st_size = 0;

    names += (strlen(dynamicSymbolName) + 1);
    fwrite(elfSym, sizeof(uint8_t), sizeof(ELFSymbol), fp);
    //hash_array[hash_index] = elfHashSysV(dynamicSymbolName);
    //mod_array[hash_index] = hash_array[hash_index] % nbucket;
    //hash_index++;
    const uint8_t* rangeStart; //relocatable elf files need symbol offset from segment base
   // if (_relaSection)
   // {
    //    rangeStart = _codeStart;    
   // } 
  //  else 
  //  {
        rangeStart = 0;
  //  }
    
    //values that are unchanged are being kept out of the while loop
    elfSym->st_other = ELF_ST_VISIBILITY(STV_DEFAULT);
    elfSym->st_info = ELF_ST_INFO(STB_GLOBAL,STT_FUNC);
    /* this while loop re-uses the ELFSymbol and writes
     
       CodeCacheSymbol info into file */
    uint32_t symbolDefStart = textSectionStartOffset;
    TR::AOTMethodHeader* hdr;

      for( auto it = TR::Compiler->aotAdapter->_methodNameToHeaderMap.begin(); it != TR::Compiler->aotAdapter->_methodNameToHeaderMap.end(); ++it )
        {
        char const * methodName = it->first;
        hdr = it->second;

        memcpy(names, methodName, strlen(methodName) + 1);
        elfSym->st_name = names - ELFSymbolNames;
        elfSym->st_shndx = hdr->self()->compiledCodeStart ? 1 : SHN_UNDEF;
        //elfSym->st_value = hdr->self()->newCompiledCodeStart ? (ELFAddress)(hdr->self()->newCompiledCodeStart - rangeStart) : 0;
        elfSym->st_value = (ELFAddress) symbolDefStart;
        elfSym->st_size = hdr->self()->compiledCodeSize;

        fwrite(elfSym, sizeof(uint8_t), sizeof(ELFSymbol), fp);
        //hash_array[hash_index] = elfHashSysV(methodName);
        //mod_array[hash_index] = hash_array[hash_index] % nbucket;
        //hash_index++;
        symbolDefStart += hdr->self()->compiledCodeSize;
        names += (strlen(methodName) + 1);
        }

    fwrite(ELFSymbolNames, sizeof(uint8_t), _totalELFSymbolNamesLength, fp);

    _rawAllocator.deallocate(elfSym);
}

void
OMR::ELFSharedObjectGenerator::writeDynamicSectionEntries(::FILE *fp)
{
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
}

void
OMR::ELFSharedObjectGenerator::writeDynamicEntryToFile(::FILE *fp, ELFDynamic * dynEntry, Elf64_Sxword tag, uint32_t value, uint64_t ptr, uint32_t flag)
{
    dynEntry->d_tag = tag;
    if(flag == 1){
        dynEntry->d_un.d_val = value; 
        }
    else{
        dynEntry->d_un.d_ptr = ptr;
        }
    fwrite(dynEntry, sizeof(uint8_t), sizeof(ELFDynamic), fp);
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
OMR::ELFSharedObjectGenerator::writeHashSectionToFile(::FILE *fp)
{
    uint8_t hash_index = 1;

    char const * dynamicSymbolName = "_DYNAMIC";
    hash_array[hash_index] = elfHashSysV(dynamicSymbolName);
    mod_array[hash_index] = hash_array[hash_index] % nbucket;
    hash_index++;

    for( auto it = TR::Compiler->aotAdapter->_methodNameToHeaderMap.begin(); it != TR::Compiler->aotAdapter->_methodNameToHeaderMap.end(); ++it )
        {
        char const * methodName = it->first;
        hash_array[hash_index] = elfHashSysV(methodName);
        mod_array[hash_index] = hash_array[hash_index] % nbucket;
        hash_index++;
        }

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
OMR::ELFSharedObjectGenerator::getNoOfBuckets(uint32_t numSymbols)
{
    static const size_t elfBuckets[] = {0, 1, 3, 17, 37, 67, 97, 131, 197, 263, 521, 1031, 2053, 4099, 8209, 16411, 32771};
    size_t n = sizeof(elfBuckets) / sizeof(elfBuckets[0]);
    for(int i = 0; i < n; i++)
    {
        if(elfBuckets[i] > numSymbols)
        {
            return elfBuckets[i-1];     
        }
    } 
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
}

#endif //LINUX
