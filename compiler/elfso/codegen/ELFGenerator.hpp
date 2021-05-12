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

#ifndef TR_SHOGENERATOR_INCL
#define TR_SHOGENERATOR_INCL

#if defined(LINUX)

#include "codegen/ELFSharedStorageGenerator.hpp"

class TR_Memory;

namespace TR{

class OMR_EXTENSIBLE ELFGenerator : public OMR::ELFGeneratorConnector
    {
    public:

    ELFGenerator(TR::RawAllocator rawAllocator, uint8_t const * codeStart, size_t codeSize) : 
        OMR::ELFGeneratorConnector(rawAllocator, codeStart, codeSize) { };

    }; //class ELFGenerator

class OMR_EXTENSIBLE ELFExecutableGenerator : public OMR::ELFExecutableGeneratorConnector
    {
    public:

    ELFExecutableGenerator(TR::RawAllocator rawAllocator, uint8_t const * codeStart, size_t codeSize) : 
        OMR::ELFExecutableGeneratorConnector(rawAllocator, codeStart, codeSize) { };

    }; //class ELFGenerator

class OMR_EXTENSIBLE ELFRelocatableGenerator : public OMR::ELFRelocatableGeneratorConnector
    {
    public:

    ELFRelocatableGenerator(TR::RawAllocator rawAllocator, uint8_t const * codeStart, size_t codeSize) : 
        OMR::ELFRelocatableGeneratorConnector(rawAllocator, codeStart, codeSize) { };

    };

} //namespace TR

#endif //LINUX

#endif //ifndef TR_ELFSHOGENERATOR_INCL
