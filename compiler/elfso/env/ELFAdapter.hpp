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

#ifndef ELF_AOTADAPTER_INCL
#define ELF_AOTADAPTER_INCL
#include "env/TRMemory.hpp"
#include "env/TypedAllocator.hpp"
#include "infra/Annotations.hpp"
#include "env/jittypes.h"
#include "env/OMRAOTAdapter.hpp"
#include <map>
#ifndef ELF_AOTADAPTER_CONNECTOR
#define ELF_AOTADAPTER_CONNECTOR
namespace ELF { class AOTAdapter; }
namespace ELF { typedef AOTAdapter AOTAdapterConnector; }
#endif

namespace TR 
   {
   class AOTAdapter;
   class AOTStorageInterface;
   class AOTMethodHeader;
   class ELFSharedObjectGenerator;
   }

namespace ELF
{

/**
 * @brief OMRAotAdapter, an extensible class that is designed to dispatch 
 * the interaction between compiler components to perform persisting of 
 * relocatable code. The benefit of having this class is in decoupling AOT-related
 * classes and removing indirect responsibility by introducing new separation of
 * responsibilities: the RelocationRuntime class is only concerned with managing 
 * the generation and application of relocations, without knowing about persisting
 * methods implemented in the current runtime environment. Similarly, Shared Cache
 * or other options for persisting code could implement a simplistic interface
 * (that, obviously can be extended) that would have two functions: receive a message
 * that contains a serialized version of the AOTMethod header, as well as send one.
 * The AOTAdapter will register the header and make it available to RelocationRuntime
 * and hence, the compiler, as well as to runtime environment, since AOTAdapter is
 * planned to be alive longer than a compilation thread.
 */

class OMR_EXTENSIBLE AOTAdapter : public OMR::AOTAdapterConnector
   {
public:
   TR_ALLOC(TR_Memory::AOTAdapter)
   AOTAdapter();
   AOTAdapter(const AOTAdapter &src);
   /**
    * @brief The self() method, required by the extensible classes hierarchy
    * 
    * @return TR::AOTAdapter* 
    */
   TR::AOTAdapter* self();

   void prepareAndEmit(uint32_t methodCount, char * filename);
   void storeAOTCodeAndData(uint32_t methodCount, char * filename);
   //void *getMethodCode(const char *methodName);
   void loadFile(const char *filename);


private:
  
   /**
    * @brief The storeAOTMethodAndDataInTheCache accepts a method name and performs a 
    * lookup of an AOTMethodHeader in the map. Later, the AOTMethodHeader is serialized 
    * and is sent to the cache. 
    * 
    * @param methodNam
    */
   void storeAOTMethodAndDataInELFSO(const char *methodName);

   /**
    * @brief A pointer to the CodeCache manager—this class manages memory that contains 
    * executable code, which is necessary if the AOT module is used to load code
    */
   TR::CodeCacheManager*    _codeCacheManager;

};
}
#endif // !defined(OMR_AOTADAPTER_INCL)
