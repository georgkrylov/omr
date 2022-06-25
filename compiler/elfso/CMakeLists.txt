###############################################################################
# Copyright (c) 2017, 2020 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at http://eclipse.org/legal/epl-2.0
# or the Apache License, Version 2.0 which accompanies this distribution
# and is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following Secondary
# Licenses when the conditions for such availability set forth in the
# Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
# version 2 with the GNU Classpath Exception [1] and GNU General Public
# License, version 2 with the OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
#############################################################################
include(OmrCompilerSupport)

macro(add_aotelfso_subdirectory dir)
	include("${CMAKE_CURRENT_LIST_DIR}/${dir}/CMakeLists.txt")
endmacro(add_aotelfso_subdirectory)

set(CORE_ELF_OBJECTS "")

if(OMR_ARCH_X86 OR OMR_ARCH_S390)
	set(OMR_ENHANCED_WARNINGS OFF)
else()
	set(OMR_WARNINGS_AS_ERRORS OFF)
	set(OMR_ENHANCED_WARNINGS OFF)
endif()

macro(aotelf_library libraryname)
	list(APPEND CORE_ELF_OBJECTS ${ARGN})
endmacro(aotelf_library)

add_aotelfso_subdirectory(env)
add_aotelfso_subdirectory(codegen)


create_omr_compiler_library(
	NAME aotelf
	OBJECTS ${CORE_ELF_OBJECTS}
	DEFINES
)
target_link_libraries(aotelf PUBLIC dl)
make_compiler_target(aotelf INTERFACE COMPILER jitbuilder)