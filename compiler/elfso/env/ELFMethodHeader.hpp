/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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
#ifndef ELF_ELFAOT_METHOD_HEADER
#define ELF_ELFAOT_METHOD_HEADER

#ifndef ELF_AOTMETHODHEADER_CONNECTOR
#define ELF_AOTMETHODHEADER_CONNECTOR
namespace ELF { class AOTMethodHeader; } 
namespace ELF { typedef AOTMethodHeader AOTMethodHeaderConnector; }
#else
#error ELF::AOTMethodHeader expected to be a primary connector, but a OMR connector is already defined
#endif

#include "infra/Annotations.hpp"
#include "env/jittypes.h"
#include "env/OMRAOTMethodHeader.hpp"

namespace TR { class AOTMethodHeader;}

namespace ELF
{

class OMR_EXTENSIBLE AOTMethodHeader : public OMR::AOTMethodHeaderConnector
   {
   /** 
    * at compile time, the constructor runs with four arguments, 
    * compiledCodeStart,  compiledCodeSize,  and relocationsStart, relocationsSize
    * at load time we don't know anything, so we run constructor with no 
    * parameters and the values from cache are derived.
    * This is one possible implementation, for a cache with contiguous
    * code and relocations data stored along them
    */
public:
   TR::AOTMethodHeader* self();
   AOTMethodHeader(){};
   AOTMethodHeader(uint8_t* compiledCodeStart, uint32_t compiledCodeSize, uint8_t* relocationsStart, uint32_t relocationsSize):
      compiledCodeStart(compiledCodeStart),
      compiledCodeSize(compiledCodeSize),
      relocationsStart(relocationsStart),
      relocationsSize(relocationsSize)
      {};
   // This constructor could be used for deserialization
   AOTMethodHeader(uint8_t* rawData);
   
   uint8_t* compiledCodeStart;
   uint32_t compiledCodeSize;
   uint8_t* relocationsStart;
   uint32_t relocationsSize;
   uint8_t* newCompiledCodeStart;

   //uintptr_t sizeOfSerializedVersion();
   //uint8_t* serialize(uint8_t* buffer);

   
   };

}

#endif
