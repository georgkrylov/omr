/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef OMR_RELOCATION_RUNTIME_INCL
#define OMR_RELOCATION_RUNTIME_INCL

#include "omrcfg.h"
#include "omr.h"
#include <assert.h>
#include "codegen/Relocation.hpp"
#include "runtime/Runtime.hpp"
#include "env/OMRCPU.hpp" 

#ifndef OMR_RELOCATION_RUNTIME_CONNECTOR
#define OMR_RELOCATION_RUNTIME_CONNECTOR
namespace OMR { class RelocationRuntime; }
namespace OMR { typedef OMR::RelocationRuntime RelocationRuntimeConnector; }
#endif


namespace TR 
{
   class RelocationRecord;
   class RelocationTarget;
   class RelocationRecordBinaryTemplate;
   class AOTMethodHeader;
   class RelocationRuntime;
   class JitConfig;
} // namespace TR

namespace OMR
{

class OMR_EXTENSIBLE RelocationRuntime 
   {
protected:
   TR::RelocationRuntime* self();
public:
   TR_ALLOC(TR_Memory::Relocation)
   void * operator new(size_t, TR::JitConfig *);
   RelocationRuntime(TR::JitConfig *jitCfg);
   TR::RelocationTarget *reloTarget()                           { return _reloTarget; }
   TR_Memory *trMemory()                                       { return _trMemory; }
   TR::AOTMethodHeader *aotMethodHeaderEntry()                  { return _aotMethodHeaderEntry; }
   TR::Compilation* comp()                                    { return _comp; }
   void * relocateMethodAndData(TR::AOTMethodHeader *cacheEntry, void* code);
private:
   typedef enum 
      {
      RelocationNoError = 1,
      } AOTRelocationStatus;
   AOTRelocationStatus  _relocationStatus;
protected:
   TR_Memory *_trMemory;
   TR::RelocationTarget *_reloTarget;
   TR::AOTMethodHeader *_aotMethodHeaderEntry;
   TR::Compilation *_comp;
   };

} //namespace OMR

#endif   // OMR_RELOCATION_RUNTIME_INCL
