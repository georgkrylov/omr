/*******************************************************************************
 * Copyright (c) 2000, 2022 IBM Corp. and others
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

#ifndef TR_RELOCATION_RECORD_INCL
#define TR_RELOCATION_RECORD_INCL

#include "runtime/OMRRelocationRecord.hpp"

namespace TR
{
/** NOTE: This class is supposed to be inherited by RelocationRecord in consuming projects
 * In the proper OMR extensible classes technology way, the file name should be RelocationRecord.hpp
 * However, calling it this name shadows includes in production projects (e.g., in OpenJ9),
 * so renaming this file happen when both projects are ready.
 * See also: https://github.com/eclipse/omr/issues/5469
 */
class OMR_EXTENSIBLE RelocationRecord: public OMR::RelocationRecordConnector
   {
public:
   RelocationRecord(TR::RelocationRuntime *reloRuntime, TR::RelocationRecordBinaryTemplate *record)
   : OMR::RelocationRecordConnector(reloRuntime,record)
      { }

   RelocationRecord(): OMR::RelocationRecordConnector()
      { }


   };

class OMR_EXTENSIBLE RelocationRecordBinaryTemplate: public OMR::RelocationRecordBinaryTemplateConnector
   {
public:
   RelocationRecordBinaryTemplate()
   : OMR::RelocationRecordBinaryTemplateConnector()
      { };
   };

class RelocationRecordGroup: public OMR::RelocationRecordGroup
   {
public:
   RelocationRecordGroup(TR::RelocationRecordBinaryTemplate *groupData)
    : OMR::RelocationRecordGroup(groupData)
      { };
   };

} // namespace TR

#endif /* TR_RELOCATION_RECORD_INCL */
