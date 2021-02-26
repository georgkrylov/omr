/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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
#include "env/AOTMethodHeader.hpp"
#include <stdlib.h>
#include <string.h>

TR::AOTMethodHeader* OMR::AOTMethodHeader::self()
   {
   return reinterpret_cast<TR::AOTMethodHeader*> (this);
   }

void OMR::AOTMethodHeader::serialize(uint8_t* buffer)
   {
   uintptr_t allocSize = self()->sizeOfSerializedVersion();

   uint8_t* ptr = buffer;

   // Copy the size of the header to the buffer
   memcpy(ptr,&allocSize,sizeof(uintptr_t));

   // Skip the bytes just copied
   ptr += sizeof(uintptr_t);

   // Copy the size of compiled code to the buffer
   memcpy(ptr,&self()->compiledCodeSize,sizeof(uint32_t));
   
   // Skip the bytes just copied
   ptr += sizeof(uint32_t);   

   // Copy the compiled code to the buffer
   memcpy(ptr,self()->compiledCodeStart,self()->compiledCodeSize);

   // Skip the bytes just copied
   ptr += self()->compiledCodeSize;

   // Copy the size of relocations data to the buffer
   memcpy(ptr,&self()->relocationsSize,sizeof(uint32_t));

   // Skip the bytes just copied
   ptr += sizeof(uint32_t);
   // Copy the relocations data to the buffer   
   memcpy(ptr,self()->relocationsStart,self()->relocationsSize);
   // Now the buffer contains all the data relevant to the
   // method header.
   }

OMR::AOTMethodHeader::AOTMethodHeader(uint8_t* rawData)
   {
   // This is an examplar implementation of a deserializing
   // code for a raw data received from the SharedCache message
   // The first sizeof(uintptr_t) bytes
   // stores the size of the AOTMethodHeader , we can
   // just skip them here, they are used by the cache
   rawData += sizeof(uintptr_t);

   // The next sizeof(uint32_t) bytes contain the 
   // size of compiled code
   self()->compiledCodeSize = *((uint32_t*) rawData);
   rawData += sizeof(uint32_t);

   // Right after the compliedCodeSize, the codeSize of bytes
   // are the code itself, we should record that as a part
   // of a header
   self()->compiledCodeStart = rawData;

   // We should skip size of compiled code in bytes
   // to get to next element of the header
   rawData += compiledCodeSize;

   // That is relocation size, occupying the next uint32_t bytes
   self()->relocationsSize = *((uint32_t*) rawData); 

   // We should skip the sizeof(uint32_t) in bytes to get
   // to the relocations information
   rawData += sizeof(uint32_t);

   // We take note of the relocationsStart to use the 
   // relocations related information in future
   self()->relocationsStart = rawData;   
   }

uintptr_t OMR::AOTMethodHeader::sizeOfSerializedVersion()
   {
   // An examplar computation of size  of serialized version
   // of an AOTMethodHeader, required for memory allocation purposes
   // The size should generally be composed of 
   // A uintptr_t to hold whole message size,
   // 2 elements of type uint32_t to store the size of the compiled code and
   // relocation data
   // compiledCodeSize of bytes (uint8_t) for compiledcode,
   // RelocationsSize of bytes (uint8_t) for relocation data.
   return sizeof(uintptr_t) + 2*sizeof(uint32_t)+self()->compiledCodeSize+self()->relocationsSize;
   }
