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

#include "env/CompilerEnv.hpp"
#include <fstream>
#include "runtime/CodeCacheManager.hpp"
#include "compile/Compilation.hpp"
#include "runtime/CodeCache.hpp"
#include "runtime/AOTRelocationRuntime.hpp"
#include <string.h>

#include "env/AOTMethodHeader.hpp"
#include "env/AOTAdapter.hpp"
#include "env/AOTStorageInterface.hpp"
#include "codegen/ELFGenerator.hpp"

ELF::AOTAdapter::AOTAdapter():
OMR::AOTAdapterConnector()//_methodNameToHeaderMap(str_comparator)
{}

TR::AOTAdapter *
ELF::AOTAdapter::self()
    {
    return static_cast<TR::AOTAdapter *>(this);
    }

void ELF::AOTAdapter::prepareAndEmit(char * filename)
{
    _storage->self()->consolidateBuffers(filename);
}


void ELF::AOTAdapter::storeAOTCodeAndData(char * filename)
{
    self()->prepareAndEmit(filename);
}

void ELF::AOTAdapter::loadFile(const char *filename)
    {
    _storage->dynamicLoading(filename);
    }

void ELF::AOTAdapter::registerAOTMethodHeader(const char*  methodName,TR::AOTMethodHeader* header)
    {
    _methodNameToHeaderMap[methodName]=header;
    _storage->storeEntry(methodName, header);
    }
    