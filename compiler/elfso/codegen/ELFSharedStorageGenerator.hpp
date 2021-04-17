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
    
    /**
     * Initializes header struct members for Shared Object ELF
    */
   
    /**
     * Initializes ELF Program Header, required for executable ELF
    */

    void writeCodeSegmentToFile(::FILE *fp);

    void buildProgramHeaders();

    virtual void buildSectionHeaders(void);

    virtual void initializeSectionOffsets(void);

    virtual void initializeSectionNames(void);
  

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

     bool emitELFSO(const char * filename);//,
               // TR::CodeCacheRelocationInfo *relocations,
                //uint32_t numRelocations);

    void processAllSymbols(::FILE *fp);

    bool emitELFFile(const char * filename);

    void writeDynamicSectionEntries(::FILE *fp);

    void writeHashSectionToFile(::FILE *fp);

    void dynamicLoading(const char*  methodName);
    //SystemVHashTable *hashTable;

    //using OMR::AOTStorageInterface::aotint;

    ELFSectionHeader *_AotCDSection;
    char              _AotCDSectionName[7];
    
    //uint8_t *_codeSegmentStart;

    const char* _key;
    void *_handle;
    
    TR::AOTStorageInterface* self();

   uint8_t* loadEntry(const char* key);
   
   void storeEntry(const char* key, TR::AOTMethodHeader* hdr);

   void consolidateCompiledCode(uint32_t methodCount, char * filename);
   void consolidateBuffers(uint32_t methodCount, char * filename);

   std::pair<uint32_t, uint32_t> calculateAggregateSize();
   std::pair<uint32_t, uint32_t> calculateAggregateBufferSize();
   
   //void consolidateCompiledCode(uint8_t *ptrStart);

   void storeEntries(const char* fileName, uint8_t *codeStart, uint32_t codeSize, uint32_t totalMethodNameLength, uint32_t methodCount);

   


}; //class ELFSharedObjectGenerator

} //namespace ELF

#endif //LINUX

#endif //ifndef OMR_ELFSHOGENERATOR_INCL
