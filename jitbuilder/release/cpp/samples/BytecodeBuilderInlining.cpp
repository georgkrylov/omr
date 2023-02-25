/*******************************************************************************
 * Copyright (c) 2016, 2023 IBM Corp. and others
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
#include "BytecodeBuilderInlining.hpp"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
/* Un comment to enable debug output */
/* #define RFIB_DEBUG_OUTPUT */
static void
printString(int64_t stringPointer)
   {
#define PRINTSTRING_LINE LINETOSTR(__LINE__)
   char *strPtr = (char *)stringPointer;
   fprintf(stderr, "%s", strPtr);
   }
static void
printInt32(int32_t value)
   {
#define PRINTINT32_LINE LINETOSTR(__LINE__)
   fprintf(stderr, "%d", value);
   }
BBInliningMethod::BBInliningMethod(OMR::JitBuilder::TypeDictionary *types, int32_t inlineDepth)
    : MethodBuilder(types),
      _inlineDepth(inlineDepth)
   {
   defineStuff();
   }
BBInliningMethod::BBInliningMethod(BBInliningMethod *callerMB)
    : MethodBuilder(callerMB),
      _inlineDepth(callerMB->_inlineDepth - 1)
   {
   defineStuff();
   }
void BBInliningMethod::defineStuff()
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);
   DefineName("fil");
   if (_inlineDepth == 0)
      {
      DefineParameter("n", Int32);
      DefineReturnType(Int32);
      }
   DefineFunction((char *)"printString",
                  (char *)__FILE__,
                  (char *)PRINTSTRING_LINE,
                  (void *)&printString,
                  NoType,
                  1,
                  Int64);
   DefineFunction((char *)"printInt32",
                  (char *)__FILE__,
                  (char *)PRINTINT32_LINE,
                  (void *)&printInt32,
                  NoType,
                  1,
                  Int32);
   }

static const char *prefix = "bim(";
static const char *middle = ") = ";
static const char *suffix = "\n";

bool BBInliningMethod::buildIL()
   {

   if (_inlineDepth == 1)
      {
      OMR::JitBuilder::VirtualMachineState *vmState = new OMR::JitBuilder::VirtualMachineState();
      setVMState(vmState);
      OMR::JitBuilder::BytecodeBuilder *b = OrphanBytecodeBuilder(0, "doncare");
      AppendBytecodeBuilder(b);
      int dontcare = GetNextBytecodeFromWorklist();
      OMR::JitBuilder::IlValue *exampleParameter = b->ConstInt32(3);
      OMR::JitBuilder::IlValue *result;
      // memory leak here, but just an example
      BBInliningMethod *inlineBim = new BBInliningMethod(this);
      result = b->Call(inlineBim, 1, exampleParameter);
      b->Call("printString", 1,
                 b->ConstInt64((int64_t)prefix));
      b->Call("printString", 1,
                 b->ConstInt64((int64_t)middle));
      b->Call("printInt32", 1,
                 result);
      b->Call("printString", 1,
                 b->ConstInt64((int64_t)suffix));
      b->Return();
      }
   else
      {
      OMR::JitBuilder::BytecodeBuilder *b = OrphanBytecodeBuilder(0, "dontcare");
      AppendBytecodeBuilder(b);
      int dontcare = GetNextBytecodeFromWorklist();
      b->Return(
          b->Sub(
              b->Load("n"),
              b->ConstInt32(2)));
      }

   return true;
   }
int main(int argc, char *argv[])
   {
   int32_t inliningDepth = 1; // by default, inline one level of calls
   printf("Step 1: initialize JIT\n");
   bool initialized = initializeJit();
   if (!initialized)
      {
      fprintf(stderr, "FAIL: could not initialize JIT\n");
      exit(-1);
      }
   printf("Step 2: define relevant types\n");
   OMR::JitBuilder::TypeDictionary types;
   printf("Step 3: compile method builder, inlining one level\n");
   BBInliningMethod method(&types, inliningDepth);
   void *entry = 0;
   int32_t rc = compileMethodBuilder(&method, &entry);
   if (rc != 0)
      {
      fprintf(stderr, "FAIL: compilation error %d\n", rc);
      exit(-2);
      }
   printf("Step 4: invoke compiled code\n");
   BytecodeBuilderInliningCallerType *bim = (BytecodeBuilderInliningCallerType *)entry;
   bim();
   printf("Step 5: shutdown JIT\n");
   shutdownJit();
   printf("PASS\n");
   }
