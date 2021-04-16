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

#ifndef ELF_ELFSHOGENERATOR_INCL
#define ELF_ELFSHOGENERATOR_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */

#ifndef ELF_SHOGENERATOR_CONNECTOR
#define ELF_SHOGENERATOR_CONNECTOR
//namespace OMR { namespace ELF {class ELFSharedObjectGenerator; } }
//namespace OMR { typedef OMR::ELF::ELFSharedObjectGenerator ELFSharedObjectGeneratorConnector; }
namespace ELF { class ELFSharedObjectGenerator;  }
namespace ELF { typedef ELFSharedObjectGenerator ELFSharedObjectGeneratorConnector; }
namespace ELF { typedef ELFSharedObjectGenerator AOTStorageInterfaceConnector; }

//namespace ELF { class AOTStorageInterface;  }
//namespace ELF { typedef AOTStorageInterface AOTStorageInterfaceConnector; }
#else
   #error ELF::ELFGenerator expected to be a primary connector, but another connector is already defined

#endif //ELF_SHOGENERATOR_CONNECTOR

#if defined(LINUX)

#include <elf.h>
#include <string>
#include <map>

#include "compiler/env/jittypes.h"
#include "env/TRMemory.hpp"
#include "env/TypedAllocator.hpp"
#include "env/RawAllocator.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "infra/Annotations.hpp"

#include "codegen/OMRELFGenerator.hpp"
#include "env/OMRAOTStorageInterface.hpp"

#include "env/OMRAOTAdapter.hpp"
#include "env/OMRAOTMethodHeader.hpp"




namespace TR {class ELFGenerator; 
              class ELFSharedObjectGenerator; 
              class CodeCacheManager; 
              class AOTMethodHeader; 
              class AOTStorageInterface; 
              }

namespace ELF{

//namespace ELF{
class ELFGenerator;
//class AOTStorageInterface;

class OMR_EXTENSIBLE ELFSharedObjectGenerator : public OMR::ELFSharedObjectGeneratorConnector, public OMR::AOTStorageInterfaceConnector
{

public:
    ELFSharedObjectGenerator(TR::RawAllocator rawAllocator,
                            uint8_t const * codeStart, size_t codeSize);

    ELFSharedObjectGenerator(TR::RawAllocator rawAllocator);

    ~ELFSharedObjectGenerator() throw()
    {
    }


  typedef bool (*StrComparator)(const char *, const char*);
  typedef TR::typed_allocator<std::pair<const char * const, TR::AOTMethodHeader *>, TR::Region &> methodHeaderAllocator;
  typedef std::map<const char *, TR::AOTMethodHeader*,StrComparator> NameToHeaderMap;
  NameToHeaderMap ELFDataMap;

protected:

    /**
     * Initializes header for Shared Object ELF file
    */
    virtual void initialize(void);
    
    /**
     * Initializes header struct members for Shared Object ELF
    */
    virtual void initializeELFHeader(void);

    /**
     * Initializes ELF Program Header, required for executable ELF
    */
    virtual ELFProgramHeader * initializeProgramHeader(uint32_t type, ELFOffset offset, ELFAddress vaddr, ELFAddress paddr, 
                                                uint32_t filesz, uint32_t memsz, uint32_t flags, uint32_t align);
    void writeCodeSegmentToFile(::FILE *fp);

    void buildProgramHeaders();

    virtual void buildSectionHeaders(void);

    virtual void initializeSectionOffsets(void);

    virtual void initializeSectionNames(void);

    void writeProgramHeaderToFile(::FILE *fp);
    
    ELFSectionHeader * initializeSection(uint32_t shName, uint32_t shType, uint32_t shFlags, ELFAddress shAddress,
                                                 ELFOffset shOffset,  uint32_t shSize, uint32_t shLink, uint32_t shInfo, uint32_t shAddralign, uint32_t shEntsize);

public:

    /**
     * This function is called when it is time to write symbols to file.
     * This function calls helper functions that initializes section headers and
     * writes to file.
     * @param[in] filename the name of the file to write to
     * @param[in] symbols the TR::CodeCacheSymbol*
     * @param[in] numSymbols the number of symbols not including UNDEF
     * @param[in] totalELFSymbolNamesLength the sum of symbol name lengths + 1 for UNDEF
     * @param[in] relocations the TR::CodeCacheRelocationInfo
     * @param[in] numRelocations the total number of relocations
     * @return bool whether emitting ELF file succeeded
    */

     bool emitAOTELF(const char * filename,
               // TR::CodeCacheSymbol *symbols,
                 uint32_t numSymbols,
                uint32_t totalELFSymbolNamesLength);//,
               // TR::CodeCacheRelocationInfo *relocations,
                //uint32_t numRelocations);

    void processAllSymbols(::FILE *fp);

    void  writeSymbolToFile(::FILE *fp, ELFSymbol *elfSym, uint32_t st_name, unsigned char st_info, unsigned char st_other, ELFSection st_shndx, ELFAddress st_value, uint64_t st_size);

    bool emitAOTELFFile(const char * filename);

    void writeDynamicSectionEntries(::FILE *fp);

    void writeDynamicEntryToFile(::FILE *fp, ELFDynamic * dynEntry, Elf64_Sxword tag, uint32_t value, uint64_t ptr, uint32_t flag);

    uint32_t elfHashSysV(const char* symbolName);

    void writeHashSectionToFile(::FILE *fp);

    //SystemVHashTable *hashTable;

    void initializeHashValues(uint32_t numSymbols);

    size_t getNoOfBuckets(uint32_t numSymbols);

    //using OMR::AOTStorageInterface::aotint;

    ELFSectionHeader *_AotCDSection;
    char              _AotCDSectionName[7];
    
    uint32_t numOfSections;
    uint32_t numOfProgramHeaders;
    uint32_t numOfDynamicEntries;
    uint32_t shStrTabNameLength;

    uint8_t *_codeSegmentStart;

    uint32_t nbucket;
    uint32_t nchain;
    uint32_t *bucket_array;
    uint32_t *chain_array;
    uint32_t *hash_array;
    uint32_t *mod_array;
    const char* _key;
    void *_handle;

    TR::AOTStorageInterface* self();

   uint8_t* loadEntry(const char* key);
   
   void storeEntry(const char* key, TR::AOTMethodHeader* hdr);

   void consolidateCompiledCode(uint32_t methodCount, char * filename);
   void consolidateBuffers(uint32_t methodCount, char * filename);
   std::pair<uint32_t, uint32_t> calculateAggregateSize();
   std::pair<uint32_t, uint32_t> calculateAggregateBufferSize();
   std::map<const char *,uint32_t> sectionOffsetMap;
   //void consolidateCompiledCode(uint8_t *ptrStart);

   void storeEntries(const char* fileName, uint8_t *codeStart, uint32_t codeSize, uint32_t totalMethodNameLength, uint32_t methodCount);

   void dynamicLoading(const char*  methodName);


}; //class ELFSharedObjectGenerator

} //namespace ELF

#endif //LINUX

#endif //ifndef OMR_ELFSHOGENERATOR_INCL
