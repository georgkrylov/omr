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


 ELF::ELFSharedObjectGenerator::ELFSharedObjectGenerator(TR::RawAllocator rawAllocator):
                            OMR::ELFSharedObjectGeneratorConnector(rawAllocator, 0, 0),  
                            OMR::AOTStorageInterfaceConnector(),
                            ELFDataMap(str_comparator)
                            {} 

void 
ELF::ELFSharedObjectGenerator::writeCodeSegmentToFile(::FILE *fp)
{
    fwrite(static_cast<const void *>(_codeSegmentStart), sizeof(uint8_t), _codeSize, fp);
}

void
ELF::ELFSharedObjectGenerator::buildProgramHeaders()
{
    _programHeaderLoadRX = initializeProgramHeader(  PT_LOAD, //should be loaded in memory
                                            0, //offset of program header from the first byte of file to be loaded
                                            0, //(ELFAddress) _codeStart; //virtual address to load into
                                            0, //(ELFAddress) _codeStart; //physical address to load into
                                            (Elf64_Xword) (sectionOffsetMap[_dataSectionName] ),//_codeSize; //in-file size
                                            (Elf64_Xword) (sectionOffsetMap[_dataSectionName] ),//_codeSize; //in-memory size
                                            PF_X | PF_R | PF_W, // should add PF_W if we get around to loading patchable code
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
ELF::ELFSharedObjectGenerator::initializeSectionNames(void)
{ 
    //printf("\n In initializeSectionNames\n");
    _zeroSectionName[0] = 0;
    shStrTabNameLength = 0;

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
    sectionOffsetMap[_AotCDSectionName] = sizeof(ELFEHeader) + (sizeof(ELFProgramHeader) * numOfProgramHeaders);
    //printf("\n textSectionStartOffset %d \n ",textSectionStartOffset);
    
    sectionOffsetMap[_shStrTabSectionName] = sizeof(ELFEHeader) + ( sizeof(ELFProgramHeader) * numOfProgramHeaders ) + _codeSize +   
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

void
ELF::ELFSharedObjectGenerator::buildSectionHeaders(void)
{    
    uint32_t shNameOffset = 0;
    initializeZeroSection();
    shNameOffset += sizeof(_zeroSectionName);

    _AotCDSection = initializeSection(shNameOffset,
						   SHT_PROGBITS,
                           SHF_ALLOC | SHF_WRITE | SHF_EXECINSTR, 
                           (ELFAddress) sectionOffsetMap[_AotCDSectionName],
                           (ELFOffset) sectionOffsetMap[_AotCDSectionName],
                           _codeSize,
                           0,
                           0,
                           16,
                           0);
    shNameOffset += sizeof(_AotCDSectionName);

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
ELF::ELFSharedObjectGenerator::emitELFSO(const char * filename)
{
    //_symbols = symbols;
    //_relocations = relocations;
    //TR::Compiler->aotAdapter->_methodNameToHeaderMap;
    
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
ELF::ELFSharedObjectGenerator::emitELFFile(const char * filename)
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

    writeSectionHeaderToFile(elfFile, _shStrTabSection);

    writeSectionHeaderToFile(elfFile, _dynSymSection);

    writeSectionHeaderToFile(elfFile, _dynStrSection);

    writeSectionHeaderToFile(elfFile, _hashSection);

    writeSectionHeaderToFile(elfFile, _dataSection);
    
    writeSectionHeaderToFile(elfFile, _dynamicSection);

    writeSectionNameToFile(elfFile, _zeroSectionName, sizeof(_zeroSectionName));

    writeSectionNameToFile(elfFile, _AotCDSectionName, sizeof(_AotCDSectionName));

    writeSectionNameToFile(elfFile, _shStrTabSectionName, sizeof(_shStrTabSectionName));
    
    writeSectionNameToFile(elfFile, _dynSymSectionName, sizeof(_dynSymSectionName));

    writeSectionNameToFile(elfFile, _dynStrSectionName, sizeof(_dynStrSectionName));

    writeSectionNameToFile(elfFile, _hashSectionName, sizeof(_hashSectionName));

    writeSectionNameToFile(elfFile, _dataSectionName, sizeof(_dataSectionName));

    writeSectionNameToFile(elfFile, _dynamicSectionName, sizeof(_dynamicSectionName));
    
    processAllSymbols(elfFile);

    writeHashSectionToFile(elfFile);

    writeDataSegmentToFile(elfFile);

    writeDynamicSectionEntries(elfFile);
    
    char str[] = "ELFEnd";
    fwrite(str , 1 , sizeof(str) , elfFile );
    fclose(elfFile);

    return true;
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

    uint32_t symbolDefStart = sectionOffsetMap[_AotCDSectionName];
    TR::AOTMethodHeader* hdr;

     // for( auto it = ELFDataMap.begin(); it != ELFDataMap.end(); ++it )
      for( auto it = ELFDataMap.begin(); it != ELFDataMap.end(); ++it )
        {
        char const * methodName = it->first;
        hdr = it->second;

        //printf("\n hdr->self()->newCompiledCodeStart = [ %p ]", hdr->self()->newCompiledCodeStart);
        memcpy(names, methodName, strlen(methodName) + 1);
        unsignedInt st_name = names - ELFSymbolNames;

        uint32_t AotCDBufferSize = hdr->self()->sizeOfSerializedVersion();
  
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
                      sectionOffsetMap[_dynamicSectionName], 
                      0);


    names += (strlen(dynamicSymbolName) + 1);

    fwrite(ELFSymbolNames, sizeof(uint8_t), _totalELFSymbolNamesLength, fp);

    _rawAllocator.deallocate(elfSym);
    //printf("\n Out writeAOTELFSymbolsToFile\n");
}

void
ELF::ELFSharedObjectGenerator::writeDynamicSectionEntries(::FILE *fp)
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
ELF::ELFSharedObjectGenerator::writeHashSectionToFile(::FILE *fp)
{
    uint32_t hash_index = 1;

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


 TR::AOTStorageInterface* ELF::ELFSharedObjectGenerator::self() //suspicious behaviour .. not sure about correctness
   {
   return reinterpret_cast<TR::AOTStorageInterface*> (this);
   }

void 
ELF::ELFSharedObjectGenerator::storeEntry(const char* key, TR::AOTMethodHeader* hdr)
   {
     // printf("\n In ELF storeEntry  ");
     // printf("\n CompiledCodeStart = [ %p ]",hdr->self()->compiledCodeStart);
    //  printf("\n CompiledCodeSize = [ %d ]",hdr->self()->compiledCodeSize);
    //  printf("\n RelocationsDataStart = [ %p ]",hdr->self()->relocationsStart);
    //  printf("\n RelocationsDataSize = [ %d ]",hdr->self()->relocationsSize);
      //printf("\n Method Name = %s\n", key);
      //_key = key;
      ELFDataMap[key] = hdr;
   }

void 
ELF::ELFSharedObjectGenerator::consolidateCompiledCode(uint32_t methodCount, char * filename)
    {
    TR::AOTMethodHeader* hdr;
    TR::AOTStorageInterface* interface;
    uint32_t totalMethodNameLength = 0;
    uint8_t *ptrStart, *ptrEnd; 
    std::pair<uint32_t, uint32_t> total_CodeSizeMethodNameLength;
    
   /* for( auto it = ELFDataMap.begin(); it != ELFDataMap.end(); ++it )
        {
         const char *methodName;
         methodName = it->first;
         printf ("\n  methodName = %s\n", methodName);
        } */
   
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

    //printf("\n Total Buffer size = [ %d ]\n", total_BufferMethodNameLength.first);
    //printf("\n Total totalMethodNameLength = [ %d ]\n", total_BufferMethodNameLength.second);
    uint8_t* compiledCodeBuffer = (uint8_t*) malloc (total_BufferMethodNameLength.first);
    ptrStart = ptrEnd = compiledCodeBuffer;

      for( auto it = ELFDataMap.begin(); it != ELFDataMap.end(); ++it )
        {
        hdr = it->second;
        hdr->self()->serialize(ptrEnd);
        ptrEnd += hdr->sizeOfSerializedVersion();
        
        }
    //printf("\n Consolidation done!\n");
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
      printf("\n In fileName , methodCount , totalMethodNameLength = [%s] [%u] [%u] \n",fileName, methodCount, totalMethodNameLength);
      //_codeSegmentStart = codeStart;
      setCodeSegmentDetails(codeStart, codeSize, methodCount, totalMethodNameLength );
      initialize();
      emitELFSO(fileName);                                 
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
