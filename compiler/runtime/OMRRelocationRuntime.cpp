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

#include "omrcfg.h"
#include "omr.h"
#include "runtime/CodeCache.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/jittypes.h"
#include "env/CompilerEnv.hpp"
#include "runtime/CodeCache.hpp"
#include "runtime/CodeCacheConfig.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "env/FrontEnd.hpp"
#include "infra/Monitor.hpp"
#include "env/PersistentInfo.hpp"
#include "env/VMAccessCriticalSection.hpp"
#include "env/CompilerEnv.hpp"
#include "runtime/TRRelocationRuntime.hpp"
#include "runtime/TRRelocationRecord.hpp"
#include "control/Recompilation.hpp"
#include "runtime/TRRelocationTarget.hpp"
#include "codegen/CodeGenerator.hpp"
#include "compile/ResolvedMethod.hpp"
#include "env/AOTMethodHeader.hpp"

OMR::RelocationRuntime::RelocationRuntime(TR::JitConfig* t)
   {
#if defined(TR_HOST_X86)
   #if defined(TR_HOST_64BIT)
   _reloTarget =  (new (PERSISTENT_NEW) TR::RelocationTarget(self()));
   #else
   TR_UNIMPLEMENTED();
   #endif
#else
   TR_UNIMPLEMENTED();
#endif

   if (_reloTarget == NULL)
      {
      TR_ASSERT(0, "Was unable to create a reloTarget, cannot use Relocations");
      }
   }

TR::RelocationRuntime * OMR::RelocationRuntime::self()
   {
   return static_cast<TR::RelocationRuntime*>(this);
   }

void *
OMR::RelocationRuntime::relocateMethodAndData(TR::AOTMethodHeader *cacheEntry, void * newCodeStart)
   {
   _aotMethodHeaderEntry = (TR::AOTMethodHeader*)(cacheEntry);
   if (newCodeStart)
      {
      _relocationStatus = AOTRelocationStatus::RelocationNoError;
      }

   if (_relocationStatus == RelocationNoError)
      {
      TR::RelocationRecordBinaryTemplate * binaryReloRecords = reinterpret_cast<TR::RelocationRecordBinaryTemplate *>(_aotMethodHeaderEntry->relocationsStart);
      TR::RelocationRecordGroup reloGroup(binaryReloRecords);
      if ( _aotMethodHeaderEntry->relocationsSize != 0)
         reloGroup.applyRelocations(self(),self()->reloTarget(), (uint8_t*) newCodeStart);
      }
   return newCodeStart;
   }
