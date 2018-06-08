/**
 * Copyright (C) 2016-2017 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

// Copyright 2011 – 2011 Xilinx, Inc. All rights reserved.
//
// This file contains confidential and proprietary information
// of Xilinx, Inc. and is protected under U.S. and
// international copyright and other intellectual property
// laws.
//
// DISCLAIMER
// This disclaimer is not a license and does not grant any
// rights to the materials distributed herewith. Except as
// otherwise provided in a valid license issued to you by
// Xilinx, and to the maximum extent permitted by applicable
// law: (1) THESE MATERIALS ARE MADE AVAILABLE "AS IS" AND
// WITH ALL FAULTS, AND XILINX HEREBY DISCLAIMS ALL WARRANTIES
// AND CONDITIONS, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING
// BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, NON-
// INFRINGEMENT, OR FITNESS FOR ANY PARTICULAR PURPOSE; and
// (2) Xilinx shall not be liable (whether in contract or tort,
// including negligence, or under any other theory of
// liability) for any loss or damage of any kind or nature
// related to, arising under or in connection with these
// materials, including for any direct, or any indirect,
// special, incidental, or consequential loss or damage
// (including loss of data, profits, goodwill, or any type of
// loss or damage suffered as a result of any action brought
// by a third party) even if such damage or loss was
// reasonably foreseeable or Xilinx had been advised of the
// possibility of the same.
//
// CRITICAL APPLICATIONS
// Xilinx products are not designed or intended to be fail-
// safe, or for use in any application requiring fail-safe
// performance, such as life-support or safety devices or
// systems, Class III medical devices, nuclear facilities,
// applications related to the deployment of airbags, or any
// other applications that could lead to death, personal
// injury, or severe property or environmental damage
// (individually and collectively, "Critical
// Applications"). Customer assumes the sole risk and
// liability of any use of Xilinx products in Critical
// Applications, subject only to applicable laws and
// regulations governing limitations on product liability.
//
// THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS
// PART OF THIS FILE AT ALL TIMES.

#ifndef DEBUG_NOLLVM
#include "llvm/Config/config.h"
#include "clang/CodeGen/CodeGenAction.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/Tool.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/DiagnosticOptions.h"
#include "clang/Frontend/FrontendDiagnostic.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Parse/ParseAST.h"
#include "clang/Frontend/ASTUnit.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/AST/Type.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/Expr.h"
#include "clang/AST/Mangle.h"
#include "clang/AST/Decl.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Rewrite/Rewriter.h"

#include "llvm/Module.h"
#include "llvm/Linker.h"
#include "llvm/Constants.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Function.h"
#include "llvm/GlobalVariable.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Config/config.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/TargetSelect.h"
//#include "llvm/Target/TargetSelect.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/PassManager.h"
#include "llvm/Assembly/PrintModulePass.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/FormattedStream.h"


#include "clang/Basic/AddressSpaces.h"  //SPIR address spaces

//JIT
#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/JIT.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#endif

//Xilinx OpenCL
#include <CL/opencl.h>
#include <stdlib.h>
#include <stdio.h>

#include <time.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>
#include <iterator>
#include <vector>

#ifndef DEBUG_NOLLVM
using namespace clang;
using namespace clang::driver;
#endif

#include "generateclc.h"

//-----------------------------------------------------------------------------------------------------------------
//generate clc.c stub implementation

bool get_implementation_type(const clang::Type *t,std::string &converted){
  //remove address space attributes from pointers
  //convert from "float16" to "__spir_float16_t"
  //convert "size_t" to "__spir_size_t"
  //convert "event_t" to "__spir_event_t"


  std::stringstream conv;
  //builtin type (non vector or pointer)
  //return __spir_bool_t etc

  if(const clang::TypedefType *TT = dyn_cast<clang::TypedefType>(t)){
      std::string temp1=TT->getDecl()->getIdentifier()->getName().str();
      if(temp1==std::string("size_t")){
        converted=std::string("__spir_size_t");
        return true;
      }else if(temp1==std::string("event_t")){
        converted=std::string("__spir_event_t");
        return true;
      }
  }
  if(t->isEventT()) {
    converted=std::string("__spir_event_t");
    return true;
  }
  if(const clang::OpenCLPipeType *PT = dyn_cast<clang::OpenCLPipeType>(t)) {
    std::string elemtype;
    std::stringstream as;
    get_implementation_type(PT->getElementType().getTypePtr(), elemtype);
    as << "__attribute__((address_space(" << LangAS::opencl_pipe << "))) ";
    converted += as.str() + elemtype +" *";
    return true;
  }
  const clang::BuiltinType *BT = dyn_cast<clang::BuiltinType>(t->getCanonicalTypeInternal());
  if(isa<BuiltinType>(t->getCanonicalTypeInternal())){
    switch(BT->getKind()){
      case BuiltinType::Void:                   converted=std::string("void"); return true;break;
      case BuiltinType::Bool:                   converted=std::string("__spir_bool_t"); return true;break;
      case BuiltinType::Char_S:                 converted=std::string("__spir_char_t"); return true;break;
      case BuiltinType::Char_U:                 converted=std::string("__spir_char_t"); return true;break;
      case BuiltinType::SChar:                  converted=std::string("__spir_char_t"); return true;break;
      case BuiltinType::Short:                  converted=std::string("__spir_short_t"); return true;break;
      case BuiltinType::Int:                    converted=std::string("__spir_int_t"); return true;break;
      case BuiltinType::Long:                   converted=std::string("__spir_long_t"); return true;break;
      case BuiltinType::LongLong:               converted=std::string("invalid"); return false;break;
      case BuiltinType::Int128:                 converted=std::string("invalid"); return false;break;
      case BuiltinType::UChar:                  converted=std::string("__spir_uchar_t"); return true;break;
      case BuiltinType::UShort:                 converted=std::string("__spir_ushort_t"); return true;break;
      case BuiltinType::UInt:                   converted=std::string("__spir_uint_t"); return true;break;
      case BuiltinType::ULong:                  converted=std::string("__spir_ulong_t"); return true;break;
      case BuiltinType::ULongLong:              converted=std::string("invalid"); return true;break;
      case BuiltinType::UInt128:                converted=std::string("invalid"); return true;break;
      case BuiltinType::Half:                   converted=std::string("__spir_half_t"); return true;break;
      case BuiltinType::Float:                  converted=std::string("__spir_float_t"); return true;break;
      case BuiltinType::Double:                 converted=std::string("__spir_double_t"); return true;break;
      case BuiltinType::LongDouble:             converted=std::string("invalid"); return true;break;
      case BuiltinType::WChar_S:
      case BuiltinType::WChar_U:
      case BuiltinType::Char16:
      case BuiltinType::Char32:
      case BuiltinType::NullPtr:
      case BuiltinType::Overload:
      case BuiltinType::BoundMember:
      case BuiltinType::Dependent:
      case BuiltinType::UnknownAny:
      case BuiltinType::ObjCId:
      case BuiltinType::ObjCClass:
      case BuiltinType::ObjCSel:                std::cout << "error\n"; exit(1); return true;break;

    }
  }
  //vector
  if(const clang::VectorType *VT = dyn_cast<clang::VectorType>(t->getCanonicalTypeInternal())){
    bool typesuccess;
    std::string typestring;
    conv << VT->getNumElements();
    typesuccess=get_implementation_type(VT->getElementType().getTypePtr(),typestring);
    //strip trailing "_t"
    typestring=typestring.substr(0,typestring.length()-2);
    //add vector count
    typestring=typestring+conv.str();
    //add "_t"
    typestring=typestring + std::string("_t");
    converted=(typestring);
    return true;
  }
  //pointer
  if(const clang::PointerType *PT = dyn_cast<clang::PointerType>(t->getCanonicalTypeInternal())){
    //return pointeetype
    bool typesuccess;
    std::string typestring;
    std::stringstream addressspacestring;
    QualType QT = PT->getPointeeType();
    if (QT.getAddressSpace() != 0) {
      addressspacestring << " __attribute__((address_space(" << QT.getAddressSpace() << "))) ";
    }
    typesuccess=get_implementation_type(PT->getPointeeType().getTypePtr(),typestring);
    if(QT.isConstQualified()) converted=std::string("const ");
    converted += addressspacestring.str() + typestring + std::string(" *");
    return true;
  }
  return(false);
}



bool get_implementation_vectorbasetype(const clang::Type *t,std::string &converted){
  std::stringstream conv;
  //builtin type (non vector or pointer)
  //return __spir_bool_t etc
  const clang::BuiltinType *BT = dyn_cast<clang::BuiltinType>(t->getCanonicalTypeInternal());
  if(isa<BuiltinType>(t->getCanonicalTypeInternal())){
    switch(BT->getKind()){
      case BuiltinType::Void:                   converted=std::string(""); return true;break;
      case BuiltinType::Bool:                   converted=std::string("__spir_bool_t"); return true;break;
      case BuiltinType::Char_S:                 converted=std::string("__spir_char_t"); return true;break;
      case BuiltinType::Char_U:                 converted=std::string("__spir_char_t"); return true;break;
      case BuiltinType::SChar:                  converted=std::string("__spir_char_t"); return true;break;
      case BuiltinType::Short:                  converted=std::string("__spir_short_t"); return true;break;
      case BuiltinType::Int:                    converted=std::string("__spir_int_t"); return true;break;
      case BuiltinType::Long:                   converted=std::string("__spir_long_t"); return true;break;
      case BuiltinType::LongLong:               converted=std::string("invalid"); return false;break;
      case BuiltinType::Int128:                 converted=std::string("invalid"); return false;break;
      case BuiltinType::UChar:                  converted=std::string("__spir_uchar_t"); return true;break;
      case BuiltinType::UShort:                 converted=std::string("__spir_ushort_t"); return true;break;
      case BuiltinType::UInt:                   converted=std::string("__spir_uint_t"); return true;break;
      case BuiltinType::ULong:                  converted=std::string("__spir_ulong_t"); return true;break;
      case BuiltinType::ULongLong:              converted=std::string("invalid"); return true;break;
      case BuiltinType::UInt128:                converted=std::string("invalid"); return true;break;
      case BuiltinType::Half:                   converted=std::string("__spir_half_t"); return true;break;
      case BuiltinType::Float:                  converted=std::string("__spir_float_t"); return true;break;
      case BuiltinType::Double:                 converted=std::string("__spir_double_t"); return true;break;
      case BuiltinType::LongDouble:             converted=std::string("invalid"); return true;break;
      case BuiltinType::WChar_S:
      case BuiltinType::WChar_U:
      case BuiltinType::Char16:
      case BuiltinType::Char32:
      case BuiltinType::NullPtr:
      case BuiltinType::Overload:
      case BuiltinType::BoundMember:
      case BuiltinType::Dependent:
      case BuiltinType::UnknownAny:
      case BuiltinType::ObjCId:
      case BuiltinType::ObjCClass:
      case BuiltinType::ObjCSel:                std::cout << "error\n"; exit(1); return true;break;

    }
  }
  //vector
  if(const clang::VectorType *VT = dyn_cast<clang::VectorType>(t->getCanonicalTypeInternal())){
    //return element type
    bool typesuccess;
    std::string typestring;
    conv << VT->getNumElements();
    typesuccess=get_implementation_vectorbasetype(VT->getElementType().getTypePtr(),typestring);
    converted=(typestring);
    return true;
  }
  //pointer
  if(const clang::PointerType *PT = dyn_cast<clang::PointerType>(t->getCanonicalTypeInternal())){
    //return pointeetype
    bool typesuccess;
    std::string typestring;
    std::string addressspacestring("");
    QualType QT=PT->getPointeeType();
    if(QT.getAddressSpace()!=0) conv << QT.getAddressSpace();
    typesuccess=get_implementation_vectorbasetype(PT->getPointeeType().getTypePtr(),typestring);
    converted=(typestring);
    return true;
  }

  return(false);
}

bool get_implementation_vectornumelements(const clang::Type *t,std::string &converted){
  std::stringstream conv;
  //builtin type (non vector or pointer)
  //return "1"
  const clang::BuiltinType *BT = dyn_cast<clang::BuiltinType>(t->getCanonicalTypeInternal());
  if(isa<BuiltinType>(t->getCanonicalTypeInternal())){
    switch(BT->getKind()){
      case BuiltinType::Void:                   converted=std::string(""); return true;break;
      case BuiltinType::Bool:                   converted=std::string("1"); return true;break;
      case BuiltinType::Char_S:                 converted=std::string("1"); return true;break;
      case BuiltinType::Char_U:                 converted=std::string("1"); return true;break;
      case BuiltinType::SChar:                  converted=std::string("1"); return true;break;
      case BuiltinType::Short:                  converted=std::string("1"); return true;break;
      case BuiltinType::Int:                    converted=std::string("1"); return true;break;
      case BuiltinType::Long:                   converted=std::string("1"); return true;break;
      case BuiltinType::LongLong:               converted=std::string("1"); return false;break;
      case BuiltinType::Int128:                 converted=std::string("1"); return false;break;
      case BuiltinType::UChar:                  converted=std::string("1"); return true;break;
      case BuiltinType::UShort:                 converted=std::string("1"); return true;break;
      case BuiltinType::UInt:                   converted=std::string("1"); return true;break;
      case BuiltinType::ULong:                  converted=std::string("1"); return true;break;
      case BuiltinType::ULongLong:              converted=std::string("invalid"); return true;break;
      case BuiltinType::UInt128:                converted=std::string("invalid"); return true;break;
      case BuiltinType::Half:                   converted=std::string("1"); return true;break;
      case BuiltinType::Float:                  converted=std::string("1"); return true;break;
      case BuiltinType::Double:                 converted=std::string("1"); return true;break;
      case BuiltinType::LongDouble:             converted=std::string("invalid"); return true;break;
      case BuiltinType::WChar_S:
      case BuiltinType::WChar_U:
      case BuiltinType::Char16:
      case BuiltinType::Char32:
      case BuiltinType::NullPtr:
      case BuiltinType::Overload:
      case BuiltinType::BoundMember:
      case BuiltinType::Dependent:
      case BuiltinType::UnknownAny:
      case BuiltinType::ObjCId:
      case BuiltinType::ObjCClass:
      case BuiltinType::ObjCSel:                std::cout << "error\n"; exit(1); return true;break;

    }
  }
  //vector
  if(const clang::VectorType *VT = dyn_cast<clang::VectorType>(t->getCanonicalTypeInternal())){
    //return number of elements
    bool typesuccess;
    std::string typestring;
    conv << VT->getNumElements();
    converted=conv.str();
    return true;
  }
  //return pointee number of elements
  if(const clang::PointerType *PT = dyn_cast<clang::PointerType>(t->getCanonicalTypeInternal())){
    bool typesuccess;
    std::string typestring;
    std::string addressspacestring("");
    QualType QT=PT->getPointeeType();
    if(QT.getAddressSpace()!=0) conv << QT.getAddressSpace();
    typesuccess=get_implementation_vectornumelements(PT->getPointeeType().getTypePtr(),typestring);
    converted=(typestring);
    return true;
  }
  return(false);
}

const clang::BuiltinType *get_builtintype_vectorbasetype(const clang::Type *t){
  std::stringstream conv;
  //builtin type (non vector or pointer)
  //return __spir_bool_t etc
  const clang::BuiltinType *BT = dyn_cast<clang::BuiltinType>(t->getCanonicalTypeInternal());
  if(isa<BuiltinType>(t->getCanonicalTypeInternal())){
    return BT;
  }
  //vector
  if(const clang::VectorType *VT = dyn_cast<clang::VectorType>(t->getCanonicalTypeInternal())){
    //return element type
    bool typesuccess;
    return(get_builtintype_vectorbasetype(VT->getElementType().getTypePtr()));
  }
  //pointer
  if(const clang::PointerType *PT = dyn_cast<clang::PointerType>(t->getCanonicalTypeInternal())){
    //return pointeetype
    bool typesuccess;
    return(get_builtintype_vectorbasetype(PT->getPointeeType().getTypePtr()));
  }
  return NULL;
}

int get_builtintype_size(const clang::BuiltinType *BT){
  switch(BT->getKind()){
    case BuiltinType::Void:                    return -1;break;
    case BuiltinType::Bool:                    return -1;break;
    case BuiltinType::Char_S:                  return 1;break;
    case BuiltinType::Char_U:                  return 1;break;
    case BuiltinType::SChar:                   return 1;break;
    case BuiltinType::Short:                   return 2;break;
    case BuiltinType::Int:                     return 4;break;
    case BuiltinType::Long:                    return 8; break;
    case BuiltinType::LongLong:                return 16;;break;
    case BuiltinType::Int128:                  return 16;break;
    case BuiltinType::UChar:                   return 1;break;
    case BuiltinType::UShort:                  return 2;break;
    case BuiltinType::UInt:                    return 4;break;
    case BuiltinType::ULong:                   return 8;break;
    case BuiltinType::ULongLong:               return 16;break;
    case BuiltinType::UInt128:                 return 16;break;
    case BuiltinType::Half:                    return 2;break;
    case BuiltinType::Float:                   return 4;break;
    case BuiltinType::Double:                  return 8;break;
    case BuiltinType::LongDouble:              return 16; break;
    case BuiltinType::WChar_S:
    case BuiltinType::WChar_U:
    case BuiltinType::Char16:
    case BuiltinType::Char32:
    case BuiltinType::NullPtr:
    case BuiltinType::Overload:
    case BuiltinType::BoundMember:
    case BuiltinType::Dependent:
    case BuiltinType::UnknownAny:
    case BuiltinType::ObjCId:
    case BuiltinType::ObjCClass:
    case BuiltinType::ObjCSel:                return -1;
  }
  return -1;
}



//Generate code into string output to apply function f, domain : elements in from, range : elements of to
void createunopvectormap(
    std::string f,
    clang::ParmVarDecl *from,
    const clang::Type *returntype,
    std::string to,
    std::string &output){
  const clang::Type *fromtype=from->getType().getTypePtr();

  //std::string fromtypestring;
  //get_implementation_type(fromtype,fromtypestring);

  std::string tovectorbasetype;
  get_implementation_vectorbasetype(fromtype,tovectorbasetype);

  //return type
  std::string returntypestring;
  get_implementation_type(returntype,returntypestring);
  std::string returnvectorbasetypestring;
  get_implementation_vectorbasetype(returntype,returnvectorbasetypestring);

  std::string fromname=from->getName();

  if(const clang::VectorType *BT = dyn_cast<clang::VectorType>(fromtype->getCanonicalTypeInternal())){
    for(unsigned int element=0;element<BT->getNumElements();element++){
      std::stringstream conv;
      conv << std::hex << element;
      output += std::string("  {\n");
      output += std::string("    ") + tovectorbasetype + std::string(" inelement;\n");
      output += std::string("    ") + returnvectorbasetypestring + std::string(" outelement;\n");
      output += std::string("    ") + tovectorbasetype + std::string(" temp0element;\n");
      output += std::string("    inelement=") + fromname + std::string(".s") + conv.str() + std::string(";\n");
      output += std::string("    ") + f + std::string("\n");
      output += std::string("    ") + to + std::string(".s") + conv.str() + std::string("=outelement;\n");
      output += std::string("  }\n");
    }
  }
  else{
    output += std::string("    ") + tovectorbasetype + std::string(" inelement;\n");
    output += std::string("    ") + returnvectorbasetypestring + std::string(" outelement;\n");
    output += std::string("    ") + tovectorbasetype + std::string(" temp0element;\n");
    output += std::string("    inelement=") + fromname + std::string(";\n");
    output += std::string("    ") + f + std::string("\n");
    output += std::string("    ") + to + std::string("=outelement;\n");
  }
}


//Generate code into string output to foldl function f
// carry = f(carry,element)
void createunopvectormap_fold(
    std::string f,
    std::string carryinitialvalue,
    clang::ParmVarDecl *from,
    std::string &output){
  const clang::Type *fromtype=from->getType().getTypePtr();

  //std::string fromtypestring;
  //get_implementation_type(fromtype,fromtypestring);

  std::string tovectorbasetype;
  get_implementation_vectorbasetype(fromtype,tovectorbasetype);

  std::string fromname=from->getName();

  output += std::string("  int carry = ")+carryinitialvalue+std::string(";\n");

  if(const clang::VectorType *BT = dyn_cast<clang::VectorType>(fromtype->getCanonicalTypeInternal())){
    for(unsigned int element=0;element<BT->getNumElements();element++){
      std::stringstream conv;
      conv << std::hex << element;
      output += std::string("  {\n");
      output += std::string("    ") + tovectorbasetype + std::string(" inelement;\n");
      output += std::string("    ") + tovectorbasetype + std::string(" temp0element;\n");
      output += std::string("    inelement=") + fromname + std::string(".s") + conv.str() + std::string(";\n");
      output += std::string("    ") + f + std::string("\n");
      output += std::string("  }\n");
    }
  }
  else{
    output += std::string("    ") + tovectorbasetype + std::string(" inelement;\n");
    output += std::string("    ") + tovectorbasetype + std::string(" temp0element;\n");
    output += std::string("    inelement=") + fromname + std::string(";\n");
    output += std::string("    ") + f + std::string("\n");
  }

}

//Generate code into string output to apply function f, domain : elements in from0 from1 pairwise, range : elements of to
//f must be of the form int f(int,int)  int2 f(int2,int2) int4 f(int4,int4) etc
void createbinopvectormap(std::string f,clang::ParmVarDecl *param0, clang::ParmVarDecl *param1, std::string to, const clang::Type *rtp,std::string &output){
  const clang::Type *param0type=param0->getType().getTypePtr();
  const clang::Type *param1type=param1->getType().getTypePtr();

  //param types
  std::string param0vectorbasetypestring;
  get_implementation_vectorbasetype(param0type,param0vectorbasetypestring);
  std::string param1vectorbasetypestring;
  get_implementation_vectorbasetype(param1type,param1vectorbasetypestring);

  //return type
  std::string returntypestring;
  get_implementation_vectorbasetype(rtp,returntypestring);

  if(param0vectorbasetypestring != returntypestring){ 
    printf("Mismatch return type and parameter type %s\n",f.c_str());
  }


  std::string from0name=param0->getName();
  std::string from1name=param1->getName();

  std::string tovectorbasetypestring=returntypestring;

  if(const clang::VectorType *BT = dyn_cast<clang::VectorType>(param0type->getCanonicalTypeInternal())){
    for(unsigned int element=0;element<BT->getNumElements();element++){
      std::stringstream conv;
      conv << std::hex << element;
      output += std::string("  {\n");
      output += std::string("    ") + param0vectorbasetypestring + std::string(" in0;\n");
      output += std::string("    ") + param1vectorbasetypestring + std::string(" in1;\n");
      output += std::string("    ") + tovectorbasetypestring + std::string(" outelement;\n");
      output += std::string("    ") + param0vectorbasetypestring + std::string(" temp0element;\n");
      output += std::string("    ") + param1vectorbasetypestring + std::string(" temp1element;\n");
      output += std::string("    in0=") + from0name + std::string(".s") + conv.str() + std::string(";\n");
      output += std::string("    in1=") + from1name + std::string(".s") + conv.str() + std::string(";\n");
      output += std::string("    ") + f + std::string("\n");
      output += std::string("    ") + to + std::string(".s") + conv.str() + std::string("=outelement;\n");
      output += std::string("  }\n");
    }
  }
  else{
    output += std::string("    ") + param0vectorbasetypestring + std::string(" in0;\n");
    output += std::string("    ") + param1vectorbasetypestring + std::string(" in1;\n");
    output += std::string("    ") + tovectorbasetypestring + std::string(" outelement;\n");
    output += std::string("    ") + param0vectorbasetypestring + std::string(" temp0element;\n");
    output += std::string("    ") + param1vectorbasetypestring + std::string(" temp1element;\n");
    output += std::string("    in0=") + from0name + std::string(";\n");
    output += std::string("    in1=") + from1name + std::string(";\n");
    output += std::string("    ") + f + std::string("\n");
    output += std::string("    ") + to + std::string("=outelement;\n");
  }
}

//Generate code into string output to apply function f, domain : elements in from0 from1 pairwise, range : elements of to
//handle step(gentype edge gentype x) or (float edge, gentypef x) or (double edge, gentype x) => step(vector,vector) or step(scalar,vector)

void createbinopvectormap2(std::string f,clang::ParmVarDecl *param0, clang::ParmVarDecl *param1,std::string to,std::string &output){
  const clang::Type *param0type=param0->getType().getTypePtr();
  const clang::Type *param1type=param1->getType().getTypePtr();

  //param types
  std::string param0vectorbasetypestring;
  bool param0Vector = get_implementation_vectorbasetype(param0type,param0vectorbasetypestring);
  std::string param1vectorbasetypestring;
  bool param1Vector = get_implementation_vectorbasetype(param1type,param1vectorbasetypestring);

  std::string from0name=param0->getName();
  std::string from1name=param1->getName();

  std::string tovectorbasetypestring=param0vectorbasetypestring;

	int param0Elements = 0;
	int param1Elements = 0;

	//	std::cout << "IN THE BINARY VECTOR MAP " << std::endl;
	if(	const clang::VectorType *param0ElemCount = dyn_cast<clang::VectorType>(param0type->getCanonicalTypeInternal()))
		param0Elements = param0ElemCount->getNumElements();
	//		std::cout << "PARAM 0 vector= " << param0ElemCount->getNumElements() << std::endl;
	if (	const clang::VectorType *param1ElemCount = dyn_cast<clang::VectorType>(param1type->getCanonicalTypeInternal()) )
		param1Elements = param1ElemCount->getNumElements();
	//		std::cout << "PARAM 1 vector = " << param1ElemCount->getNumElements() << std::endl;

	if(param0Elements!= param1Elements){
		if(const clang::VectorType *BT = dyn_cast<clang::VectorType>(param1type->getCanonicalTypeInternal())){
			for(unsigned int element=0;element<BT->getNumElements();element++){
				std::stringstream conv;
				conv << std::hex << element;
				output += std::string("  {\n");
				output += std::string("    ") + tovectorbasetypestring + std::string(" in0;\n");
				output += std::string("    ") + tovectorbasetypestring + std::string(" in1;\n");
				output += std::string("    ") + tovectorbasetypestring + std::string(" outelement;\n");
				output += std::string("    ") + param0vectorbasetypestring + std::string(" temp0element;\n");
				output += std::string("    ") + param1vectorbasetypestring + std::string(" temp1element;\n");
				output += std::string("    in0=") + from0name + std::string(";\n");
				output += std::string("    in1=") + from1name + std::string(".s") + conv.str() + std::string(";\n");
				output += std::string("    ") + f + std::string("\n");
				output += std::string("    ") + to + std::string(".s") + conv.str() + std::string("=outelement;\n");
				output += std::string("  }\n");
			}
		}
	}
	else{
		if(const clang::VectorType *BT = dyn_cast<clang::VectorType>(param0type->getCanonicalTypeInternal())){
			for(unsigned int element=0;element<BT->getNumElements();element++){
				std::stringstream conv;
				conv << std::hex << element;
				output += std::string("  {\n");
				output += std::string("    ") + tovectorbasetypestring + std::string(" in0;\n");
				output += std::string("    ") + tovectorbasetypestring + std::string(" in1;\n");
				output += std::string("    ") + tovectorbasetypestring + std::string(" outelement;\n");
				output += std::string("    ") + param0vectorbasetypestring + std::string(" temp0element;\n");
				output += std::string("    ") + param1vectorbasetypestring + std::string(" temp1element;\n");
				output += std::string("    in0=") + from0name + std::string(".s") + conv.str() + std::string(";\n");
				output += std::string("    in1=") + from1name + std::string(".s") + conv.str() + std::string(";\n");
				output += std::string("    ") + f + std::string("\n");
				output += std::string("    ") + to + std::string(".s") + conv.str() + std::string("=outelement;\n");
				output += std::string("  }\n");
			}
		}
		else{
			output += std::string("    ") + tovectorbasetypestring + std::string(" in0;\n");
			output += std::string("    ") + tovectorbasetypestring + std::string(" in1;\n");
			output += std::string("    ") + tovectorbasetypestring + std::string(" outelement;\n");
			output += std::string("    ") + param0vectorbasetypestring + std::string(" temp0element;\n");
			output += std::string("    ") + param1vectorbasetypestring + std::string(" temp1element;\n");
			output += std::string("    in0=") + from0name + std::string(";\n");
			output += std::string("    in1=") + from1name + std::string(";\n");
			output += std::string("    ") + f + std::string("\n");
			output += std::string("    ") + to + std::string("=outelement;\n");
		}
	}
}

//Generate code into string output to apply function f, domain : elements in from0, from1 is a scalar, range : elements of to
//gentype fmin(gentype x, double y)
void createbinopvectormap3(std::string f,clang::ParmVarDecl *param0, clang::ParmVarDecl *param1,std::string to,std::string &output){
  const clang::Type *param0type=param0->getType().getTypePtr();
  const clang::Type *param1type=param1->getType().getTypePtr();

  //param types
  std::string param0vectorbasetypestring;
  bool param0Vector = get_implementation_vectorbasetype(param0type,param0vectorbasetypestring);
  std::string param1vectorbasetypestring;
  bool param1Vector = get_implementation_vectorbasetype(param1type,param1vectorbasetypestring);

  std::string from0name=param0->getName();
  std::string from1name=param1->getName();

  std::string tovectorbasetypestring=param0vectorbasetypestring;

	int param0Elements = 0;
	int param1Elements = 0;

	//	std::cout << "IN THE BINARY VECTOR MAP " << std::endl;
	if(	const clang::VectorType *param0ElemCount = dyn_cast<clang::VectorType>(param0type->getCanonicalTypeInternal()))
		param0Elements = param0ElemCount->getNumElements();
	//		std::cout << "PARAM 0 vector= " << param0ElemCount->getNumElements() << std::endl;
	if (	const clang::VectorType *param1ElemCount = dyn_cast<clang::VectorType>(param1type->getCanonicalTypeInternal()) )
		param1Elements = param1ElemCount->getNumElements();
	//		std::cout << "PARAM 1 vector = " << param1ElemCount->getNumElements() << std::endl;

	if(param0Elements!= param1Elements){
		if(const clang::VectorType *BT = dyn_cast<clang::VectorType>(param0type->getCanonicalTypeInternal())){
			for(unsigned int element=0;element<BT->getNumElements();element++){
				std::stringstream conv;
				conv << std::hex << element;
				output += std::string("  {\n");
				output += std::string("    ") + tovectorbasetypestring + std::string(" in0;\n");
				output += std::string("    ") + tovectorbasetypestring + std::string(" in1;\n");
				output += std::string("    ") + tovectorbasetypestring + std::string(" outelement;\n");
				output += std::string("    ") + param0vectorbasetypestring + std::string(" temp0element;\n");
				output += std::string("    ") + param1vectorbasetypestring + std::string(" temp1element;\n");
				output += std::string("    in1=") + from1name + std::string(";\n");
				output += std::string("    in0=") + from0name + std::string(".s") + conv.str() + std::string(";\n");
				output += std::string("    ") + f + std::string("\n");
				output += std::string("    ") + to + std::string(".s") + conv.str() + std::string("=outelement;\n");
				output += std::string("  }\n");
			}
		}
	}
	else{
		if(const clang::VectorType *BT = dyn_cast<clang::VectorType>(param0type->getCanonicalTypeInternal())){
			for(unsigned int element=0;element<BT->getNumElements();element++){
				std::stringstream conv;
				conv << std::hex << element;
				output += std::string("  {\n");
				output += std::string("    ") + tovectorbasetypestring + std::string(" in0;\n");
				output += std::string("    ") + tovectorbasetypestring + std::string(" in1;\n");
				output += std::string("    ") + tovectorbasetypestring + std::string(" outelement;\n");
				output += std::string("    ") + param0vectorbasetypestring + std::string(" temp0element;\n");
				output += std::string("    ") + param1vectorbasetypestring + std::string(" temp1element;\n");
				output += std::string("    in0=") + from0name + std::string(".s") + conv.str() + std::string(";\n");
				output += std::string("    in1=") + from1name + std::string(".s") + conv.str() + std::string(";\n");
				output += std::string("    ") + f + std::string("\n");
				output += std::string("    ") + to + std::string(".s") + conv.str() + std::string("=outelement;\n");
				output += std::string("  }\n");
			}
		}
		else{
			output += std::string("    ") + tovectorbasetypestring + std::string(" in0;\n");
			output += std::string("    ") + tovectorbasetypestring + std::string(" in1;\n");
			output += std::string("    ") + tovectorbasetypestring + std::string(" outelement;\n");
			output += std::string("    ") + param0vectorbasetypestring + std::string(" temp0element;\n");
			output += std::string("    ") + param1vectorbasetypestring + std::string(" temp1element;\n");
			output += std::string("    in0=") + from0name + std::string(";\n");
			output += std::string("    in1=") + from1name + std::string(";\n");
			output += std::string("    ") + f + std::string("\n");
			output += std::string("    ") + to + std::string("=outelement;\n");
		}
	}
}

//Generate code into string output to apply function f, domain : elements in from0, from1 is a scalar int or vector, range : elements of to
//gentype ldexp(gentype x, gentype y)
//gentype ldexp(gentype x, int y)
void createbinopvectormap4(std::string f,clang::ParmVarDecl *param0, clang::ParmVarDecl *param1,std::string to,std::string &output){
  const clang::Type *param0type=param0->getType().getTypePtr();
  const clang::Type *param1type=param1->getType().getTypePtr();

  //param types
  std::string param0vectorbasetypestring;
  std::string param1vectorbasetypestring;
  bool param0Vector = get_implementation_vectorbasetype(param0type,param0vectorbasetypestring);
  bool param1Vector = get_implementation_vectorbasetype(param1type,param1vectorbasetypestring);
  std::string from0name=param0->getName();
  std::string from1name=param1->getName();
  std::string tovectorbasetypestring=param0vectorbasetypestring;

  const clang::VectorType *BT0 = dyn_cast<clang::VectorType>(param0type->getCanonicalTypeInternal());
  const clang::VectorType *BT1 = dyn_cast<clang::VectorType>(param1type->getCanonicalTypeInternal());
  char vector0 = (BT0!=NULL);
  char vector1 = (BT1!=NULL);

  //gentype ldexp(gentype x, gentype y)
  if(vector0 && vector1){
    for(unsigned int element=0;element<BT0->getNumElements();element++){
      std::stringstream conv;
      conv << std::hex << element;
      output += std::string("  {\n");
      output += std::string("    ") + tovectorbasetypestring + std::string(" in0;\n");
      output += std::string("    ") + tovectorbasetypestring + std::string(" in1;\n");
      output += std::string("    ") + tovectorbasetypestring + std::string(" outelement;\n");
      output += std::string("    ") + param0vectorbasetypestring + std::string(" temp0element;\n");
      output += std::string("    ") + param1vectorbasetypestring + std::string(" temp1element;\n");
      output += std::string("    in0=") + from0name + std::string(".s") + conv.str() + std::string(";\n");
      output += std::string("    in1=") + from1name + std::string(".s") + conv.str() + std::string(";\n");
      output += std::string("    ") + f + std::string("\n");
      output += std::string("    ") + to + std::string(".s") + conv.str() + std::string("=outelement;\n");
      output += std::string("  }\n");
    }
  }
  //gentype ldexp(gentype x, int y)
  else if(vector0){
    for(unsigned int element=0;element<BT0->getNumElements();element++){
      std::stringstream conv;
      conv << std::hex << element;
      output += std::string("  {\n");
      output += std::string("    ") + tovectorbasetypestring + std::string(" in0;\n");
      output += std::string("    ") + tovectorbasetypestring + std::string(" in1;\n");
      output += std::string("    ") + tovectorbasetypestring + std::string(" outelement;\n");
      output += std::string("    ") + param0vectorbasetypestring + std::string(" temp0element;\n");
      output += std::string("    ") + param1vectorbasetypestring + std::string(" temp1element;\n");
      output += std::string("    in0=") + from0name + std::string(".s") + conv.str() + std::string(";\n");
      output += std::string("    in1=") + from1name + std::string(";\n");
      output += std::string("    ") + f + std::string("\n");
      output += std::string("    ") + to + std::string(".s") + conv.str() + std::string("=outelement;\n");
      output += std::string("  }\n");
    }
  }
  //int ldexp(int x, int y)
  else{
    output += std::string("    ") + param0vectorbasetypestring + std::string(" in0;\n");
    output += std::string("    ") + param1vectorbasetypestring + std::string(" in1;\n");
    output += std::string("    ") + tovectorbasetypestring + std::string(" outelement;\n");
    output += std::string("    ") + param0vectorbasetypestring + std::string(" temp0element;\n");
    output += std::string("    ") + param1vectorbasetypestring + std::string(" temp1element;\n");
    output += std::string("    in0=") + from0name + std::string(";\n");
    output += std::string("    in1=") + from1name + std::string(";\n");
    output += std::string("    ") + f + std::string("\n");
    output += std::string("    ") + to + std::string("=outelement;\n");
  }
}

//fract(gentype x, __global gentype *iptr)
void createbinopvectormapfract(
    std::string fout,
    std::string foutparam,
    clang::ParmVarDecl *param0,
    clang::ParmVarDecl *param1,
    const clang::Type *returntype,
    std::string to,
    std::string toparam,
    std::string &output){
  const clang::Type *param0type=param0->getType().getTypePtr();
  const clang::Type *param1type=param1->getType().getTypePtr();

  //param types
  std::string param0vectorbasetypestring;
  get_implementation_vectorbasetype(param0type,param0vectorbasetypestring);
  std::string param1vectorbasetypestring;
  get_implementation_vectorbasetype(param1type,param1vectorbasetypestring);
  std::string param1typestring;
  get_implementation_type(param1type,param1typestring);
  //get param1 pointer pointee type
  //eg int * returns int
  std::string param1pointeetypestring;
  if(const clang::PointerType *PT = dyn_cast<clang::PointerType>(param1type->getCanonicalTypeInternal())){
    //return pointeetype
    bool typesuccess;
    std::string typestring;
    std::stringstream addressspacestring;
    QualType QT = PT->getPointeeType();
    typesuccess=get_implementation_type(PT->getPointeeType().getTypePtr(),param1pointeetypestring);
  }

  //return type
  std::string returntypestring;
  get_implementation_type(returntype,returntypestring);
  std::string returnvectorbasetypestring;
  get_implementation_vectorbasetype(returntype,returnvectorbasetypestring);

  std::string param0name=param0->getName();
  std::string param1name=param1->getName();

  //declare temps
  output += returntypestring + std::string(" out;\n");
  output += param1pointeetypestring + std::string(" outparam;\n");


  if(const clang::VectorType *BT = dyn_cast<clang::VectorType>(param0type->getCanonicalTypeInternal())){
    for(unsigned int element=0;element<BT->getNumElements();element++){
      std::stringstream conv;
      conv << std::hex << element;
      output += std::string("  {\n");
      output += std::string("    ") + param0vectorbasetypestring + std::string(" in0;\n");
      output += std::string("    ") + returnvectorbasetypestring + std::string(" outelement;\n");
      output += std::string("    ") + param0vectorbasetypestring + std::string(" temp0element;\n");
      output += std::string("    ") + param1vectorbasetypestring + std::string(" temp1element;\n");
      output += std::string("    ") + returnvectorbasetypestring + std::string(" tempoutelement;\n");
      output += std::string("    ") + param1vectorbasetypestring + std::string(" outelementptr;\n");
      output += std::string("    in0=") + param0name + std::string(".s") + conv.str() + std::string(";\n");
      output += std::string("    ") + fout + std::string("\n");
      output += std::string("    ") + foutparam + std::string("\n");
      output += std::string("    ") + to + std::string(".s") + conv.str() + std::string("=outelement;\n");
      output += std::string("    ") + toparam + std::string(".s") + conv.str() + std::string("=outelementptr;\n");
      output += std::string("  }\n");
    }
  }
  else{
    output += std::string("    ") + param0vectorbasetypestring + std::string(" in0;\n");
    output += std::string("    ") + returnvectorbasetypestring + std::string(" outelement;\n");
    output += std::string("    ") + param0vectorbasetypestring + std::string(" temp0element;\n");
    output += std::string("    ") + param1vectorbasetypestring + std::string(" temp1element;\n");
    output += std::string("    ") + returnvectorbasetypestring + std::string(" tempoutelement;\n");
    output += std::string("    ") + param1vectorbasetypestring + std::string(" outelementptr;\n");
    output += std::string("    in0=") + param0name + std::string(";\n");
    output += std::string("    ") + fout + std::string("\n");
    output += std::string("    ") + foutparam + std::string("\n");
    output += std::string("    ") + to + std::string("=outelement;\n");
    output += std::string("    ") + toparam + std::string("=outelementptr;\n");
  }
}


void createtriopvectormap(std::string f,clang::ParmVarDecl *from0, clang::ParmVarDecl *from1,clang::ParmVarDecl *from2,std::string to,std::string &output){
  const clang::Type *from0type=from0->getType().getTypePtr();
  const clang::Type *from1type=from1->getType().getTypePtr();
  const clang::Type *from2type=from2->getType().getTypePtr();
  const clang::Type *totype=from0->getType().getTypePtr();

  //std::string fromtypestring;
  //get_implementation_type(fromtype,fromtypestring);
  bool from0vectortype=false;
  if(const clang::VectorType *VT = dyn_cast<clang::VectorType>(from0type->getCanonicalTypeInternal())) from0vectortype=true;
  bool from1vectortype=false;
  if(const clang::VectorType *VT = dyn_cast<clang::VectorType>(from1type->getCanonicalTypeInternal())) from1vectortype=true;
  bool from2vectortype=false;
  if(const clang::VectorType *VT = dyn_cast<clang::VectorType>(from2type->getCanonicalTypeInternal())) from2vectortype=true;

  std::string from0vectorbasetype;
  get_implementation_vectorbasetype(from0type,from0vectorbasetype);
  std::string from1vectorbasetype;
  get_implementation_vectorbasetype(from1type,from1vectorbasetype);
  std::string from2vectorbasetype;
  get_implementation_vectorbasetype(from2type,from2vectorbasetype);

  std::string tovectorbasetype;
  get_implementation_vectorbasetype(totype,tovectorbasetype);

  std::string from0name=from0->getName();
  std::string from1name=from1->getName();
  std::string from2name=from2->getName();

	if(const clang::VectorType *BT = dyn_cast<clang::VectorType>(from0type->getCanonicalTypeInternal())){
		for(unsigned int element=0;element<BT->getNumElements();element++){
			std::stringstream conv;
			conv << std::hex << element;
			output += std::string("  {\n");
			output += std::string("    ") + from0vectorbasetype + std::string(" in0;\n");
			output += std::string("    ") + from1vectorbasetype + std::string(" in1;\n");
			output += std::string("    ") + from2vectorbasetype + std::string(" in2;\n");
			output += std::string("    ") + from0vectorbasetype + std::string(" temp0element;\n");
			output += std::string("    ") + tovectorbasetype + std::string(" outelement;\n");
			//handle sgentype (scalar gentype) eg clamp
			if(from0vectortype)
				output += std::string("    in0=") + from0name + std::string(".s") + conv.str() + std::string(";\n");
			else output += std::string("    in0=") + from0name + std::string(";\n");
			if(from1vectortype)
				output += std::string("    in1=") + from1name + std::string(".s") + conv.str() + std::string(";\n");
			else output += std::string("    in1=") + from1name + std::string(";\n");
			if(from2vectortype)
				output += std::string("    in2=") + from2name + std::string(".s") + conv.str() + std::string(";\n");
			else output += std::string("    in2=") + from2name + std::string(";\n");
			output += std::string("    ") + f + std::string("\n");
			output += std::string("    ") + to + std::string(".s") + conv.str() + std::string("=outelement;\n");
			output += std::string("  }\n");
		}
	}
	else{
		output += std::string("    ") + tovectorbasetype + std::string(" in0;\n");
		output += std::string("    ") + tovectorbasetype + std::string(" in1;\n");
		output += std::string("    ") + tovectorbasetype + std::string(" in2;\n");
		output += std::string("    ") + from0vectorbasetype + std::string(" temp0element;\n");
		output += std::string("    ") + tovectorbasetype + std::string(" outelement;\n");
		output += std::string("    in0=") + from0name + std::string(";\n");
		output += std::string("    in1=") + from1name + std::string(";\n");
		output += std::string("    in2=") + from2name + std::string(";\n");
		output += std::string("    ") + f + std::string("\n");
			output += std::string("    ") + to + std::string("=outelement;\n");
	}
}

void createtriopvectormap2(std::string f,clang::ParmVarDecl *from0, clang::ParmVarDecl *from1,clang::ParmVarDecl *from2,std::string to,std::string &output){
  const clang::Type *from0type=from0->getType().getTypePtr();
  const clang::Type *from1type=from1->getType().getTypePtr();
  const clang::Type *from2type=from2->getType().getTypePtr();
  const clang::Type *totype=from0->getType().getTypePtr();

  //std::string fromtypestring;
  //get_implementation_type(fromtype,fromtypestring);
  bool from0vectortype=false;
  if(const clang::VectorType *VT = dyn_cast<clang::VectorType>(from0type->getCanonicalTypeInternal())) from0vectortype=true;
  bool from1vectortype=false;
  if(const clang::VectorType *VT = dyn_cast<clang::VectorType>(from1type->getCanonicalTypeInternal())) from1vectortype=true;
  bool from2vectortype=false;
  if(const clang::VectorType *VT = dyn_cast<clang::VectorType>(from2type->getCanonicalTypeInternal())) from2vectortype=true;

  std::string from0vectorbasetype;
  get_implementation_vectorbasetype(from0type,from0vectorbasetype);
  std::string from1vectorbasetype;
  get_implementation_vectorbasetype(from1type,from1vectorbasetype);
  std::string from2vectorbasetype;
  get_implementation_vectorbasetype(from2type,from2vectorbasetype);

  std::string tovectorbasetype;
  get_implementation_vectorbasetype(totype,tovectorbasetype);

  std::string from0name=from0->getName();
  std::string from1name=from1->getName();
  std::string from2name=from2->getName();

	if((from0vectortype == false) && (from2vectortype == true)){
		if(const clang::VectorType *BT = dyn_cast<clang::VectorType>(from2type->getCanonicalTypeInternal())){
			for(unsigned int element=0;element<BT->getNumElements();element++){
				std::stringstream conv;
				conv << std::hex << element;
				output += std::string("  {\n");
				output += std::string("    ") + from0vectorbasetype + std::string(" in0;\n");
				output += std::string("    ") + from1vectorbasetype + std::string(" in1;\n");
				output += std::string("    ") + from2vectorbasetype + std::string(" in2;\n");
				output += std::string("    ") + from0vectorbasetype + std::string(" temp0element;\n");
				output += std::string("    ") + tovectorbasetype + std::string(" outelement;\n");
				//handle sgentype (scalar gentype) eg clamp
				if(from0vectortype)
					output += std::string("    in0=") + from0name + std::string(".s") + conv.str() + std::string(";\n");
				else output += std::string("    in0=") + from0name + std::string(";\n");
				if(from1vectortype)
					output += std::string("    in1=") + from1name + std::string(".s") + conv.str() + std::string(";\n");
				else output += std::string("    in1=") + from1name + std::string(";\n");
				if(from2vectortype)
					output += std::string("    in2=") + from2name + std::string(".s") + conv.str() + std::string(";\n");
				else output += std::string("    in2=") + from2name + std::string(";\n");
				output += std::string("    ") + f + std::string("\n");
				output += std::string("    ") + to + std::string(".s") + conv.str() + std::string("=outelement;\n");
				output += std::string("  }\n");
			}
		}
	}else{
		if(const clang::VectorType *BT = dyn_cast<clang::VectorType>(from0type->getCanonicalTypeInternal())){
			for(unsigned int element=0;element<BT->getNumElements();element++){
				std::stringstream conv;
				conv << std::hex << element;
				output += std::string("  {\n");
				output += std::string("    ") + from0vectorbasetype + std::string(" in0;\n");
				output += std::string("    ") + from1vectorbasetype + std::string(" in1;\n");
				output += std::string("    ") + from2vectorbasetype + std::string(" in2;\n");
				output += std::string("    ") + from0vectorbasetype + std::string(" temp0element;\n");
				output += std::string("    ") + tovectorbasetype + std::string(" outelement;\n");
				//handle sgentype (scalar gentype) eg clamp
				if(from0vectortype)
					output += std::string("    in0=") + from0name + std::string(".s") + conv.str() + std::string(";\n");
				else output += std::string("    in0=") + from0name + std::string(";\n");
				if(from1vectortype)
					output += std::string("    in1=") + from1name + std::string(".s") + conv.str() + std::string(";\n");
				else output += std::string("    in1=") + from1name + std::string(";\n");
				if(from2vectortype)
					output += std::string("    in2=") + from2name + std::string(".s") + conv.str() + std::string(";\n");
				else output += std::string("    in2=") + from2name + std::string(";\n");
				output += std::string("    ") + f + std::string("\n");
				output += std::string("    ") + to + std::string(".s") + conv.str() + std::string("=outelement;\n");
				output += std::string("  }\n");
			}
		}
		else{
			output += std::string("    ") + tovectorbasetype + std::string(" in0;\n");
			output += std::string("    ") + tovectorbasetype + std::string(" in1;\n");
			output += std::string("    ") + tovectorbasetype + std::string(" in2;\n");
			output += std::string("    ") + from0vectorbasetype + std::string(" temp0element;\n");
			output += std::string("    ") + tovectorbasetype + std::string(" outelement;\n");
			output += std::string("    in0=") + from0name + std::string(";\n");
			output += std::string("    in1=") + from1name + std::string(";\n");
			output += std::string("    in2=") + from2name + std::string(";\n");
			output += std::string("    ") + f + std::string("\n");
			output += std::string("    ") + to + std::string("=outelement;\n");
		}
	}
}

//gentype remquo(gentype x, gentype y, __global int *iptr)
void createtriopvectormapremquo(
    std::string fout,
    std::string foutparam,
    clang::ParmVarDecl *param0,
    clang::ParmVarDecl *param1,
    clang::ParmVarDecl *param2,
    const clang::Type *returntype,
    std::string to,
    std::string toparam,
    std::string &output){
  const clang::Type *param0type=param0->getType().getTypePtr();
  const clang::Type *param1type=param0->getType().getTypePtr();
  const clang::Type *param2type=param2->getType().getTypePtr();

  //param types
  std::string param0vectorbasetypestring;
  get_implementation_vectorbasetype(param0type,param0vectorbasetypestring);
  std::string param1vectorbasetypestring;
  get_implementation_vectorbasetype(param1type,param1vectorbasetypestring);
  std::string param2vectorbasetypestring;
  get_implementation_vectorbasetype(param2type,param2vectorbasetypestring);
  //get param2 pointer pointee type
  //eg int * returns int
  std::string param2pointeetypestring;
  if(const clang::PointerType *PT = dyn_cast<clang::PointerType>(param2type->getCanonicalTypeInternal())){
    //return pointeetype
    bool typesuccess;
    std::string typestring;
    std::stringstream addressspacestring;
    QualType QT = PT->getPointeeType();
    typesuccess=get_implementation_type(PT->getPointeeType().getTypePtr(),param2pointeetypestring);
  }

  //return type
  std::string returntypestring;
  get_implementation_type(returntype,returntypestring);
  std::string returnvectorbasetypestring;
  get_implementation_vectorbasetype(returntype,returnvectorbasetypestring);

  std::string param0name=param0->getName();
  std::string param1name=param1->getName();

  //declare temps
  output += returntypestring + std::string(" out;\n");
  output += param2pointeetypestring + std::string(" outparam;\n");


  if(const clang::VectorType *BT = dyn_cast<clang::VectorType>(param0type->getCanonicalTypeInternal())){
    for(unsigned int element=0;element<BT->getNumElements();element++){
      std::stringstream conv;
      conv << std::hex << element;
      output += std::string("  {\n");
      output += std::string("    ") + param0vectorbasetypestring + std::string(" in0;\n");
      output += std::string("    ") + param1vectorbasetypestring + std::string(" in1;\n");
      output += std::string("    ") + returnvectorbasetypestring + std::string(" outelement;\n");
      output += std::string("    ") + param2vectorbasetypestring + std::string(" outelementptr;\n");
      output += std::string("    in0=") + param0name + std::string(".s") + conv.str() + std::string(";\n");
      output += std::string("    in1=") + param1name + std::string(".s") + conv.str() + std::string(";\n");
      output += std::string("    ") + fout + std::string("\n");
      output += std::string("    ") + foutparam + std::string("\n");
      output += std::string("    ") + to + std::string(".s") + conv.str() + std::string("=outelement;\n");
      output += std::string("    ") + toparam + std::string(".s") + conv.str() + std::string("=outelementptr;\n");
      output += std::string("  }\n");
    }
  }
  else{
    output += std::string("    ") + param0vectorbasetypestring + std::string(" in0;\n");
    output += std::string("    ") + param1vectorbasetypestring + std::string(" in1;\n");
    output += std::string("    ") + returnvectorbasetypestring + std::string(" outelement;\n");
    output += std::string("    ") + param2vectorbasetypestring + std::string(" outelementptr;\n");
    output += std::string("    in0=") + param0name + std::string(";\n");
    output += std::string("    ") + fout + std::string("\n");
    output += std::string("    ") + foutparam + std::string("\n");
    output += std::string("    ") + to + std::string("=outelement;\n");
    output += std::string("    ") + toparam + std::string("=outelementptr;\n");
  }
}




ClcImplementationASTConsumer::ClcImplementationASTConsumer(MangleContext *m,clang::Preprocessor *pre) : outstring_conversions(6) {
  manglecontext=m;
  preprocessor=pre;
}

ClcImplementationASTConsumer::~ClcImplementationASTConsumer(){
}

void ClcImplementationASTConsumer::setSema(Sema *sem){
  sema=sem;
}


bool ClcImplementationASTConsumer::HandleTopLevelDecl(DeclGroupRef d){
  //printf("declgroupref\n");
  DeclGroupRef::iterator it=d.begin();
  //std::vector<FunctionDecl *> addfunctiondecl;
  while(it!=d.end()){
    if(clang::FunctionDecl *fdecl=dyn_cast<clang::FunctionDecl>(*it)){

      enum {math, relational, conversions, integer, commonfns, geometric, native, async, other} builtinfile;
      int builtinfilesplit=0;
      builtinfile=other;

      ASTContext *context=&(fdecl->getASTContext());

      //get builtin information
      std::string mangle_raw_string;
      llvm::raw_string_ostream mangle_raw_ostream(mangle_raw_string);
      manglecontext->mangleName(fdecl,mangle_raw_ostream);
      IdentifierInfo *newbuiltinii = preprocessor->getIdentifierInfo(std::string("__builtin")+mangle_raw_ostream.str());

      //scope
      Scope *functionscope=sema->getScopeForContext(fdecl);

      //print fdecl, sub function name for mangled name as per builtin lookup
      //add in __spir_rt_info_t first parameter
      std::string pstring;
      //print return type
      const clang::Type *returntype;
      std::string rqtstring;
      {
        const clang::FunctionType *AFT= fdecl->getType()->getAs<clang::FunctionType>();
        clang::QualType rqt=AFT->getResultType();
        const clang::Type *rtp=rqt.getTypePtr();
        returntype=rtp;
        std::string temp;
        bool success=get_implementation_type(rtp,temp);
        if(!success){
          std::cout << "error\n"; exit(1);
        }
        rqtstring += temp;
      }
      //std::string rqtstring=rqt.getAsString();

      //mangled builtin name
      //std::string builtinname=newbuiltinii->getName().str();
      std::string nstring=mangle_raw_ostream.str();//builtinname.substr(9,builtinname.length()-9);

      //parameters
      pstring=std::string("(__spir_rt_info_t *rt_info");
      llvm::raw_string_ostream pout(pstring);
      if(fdecl->getNumParams()!=0) pstring+=std::string(",");
      for(unsigned int i=0;i<fdecl->getNumParams();i++){
        std::string temp1;
        bool success=get_implementation_type(fdecl->getParamDecl(i)->getType().getTypePtr(),temp1);
        if(!success){
          std::cout << "error\n"; exit(1);
        }
        //pstring += std::string("<");
        pstring +=temp1;
        //pstring += std::string(">");
        pstring +=std::string(" ")+ fdecl->getParamDecl(i)->getNameAsString();


        //remove all attributes from parameters
        //fdecl->getParamDecl(i)->print(pout);
        //pout.flush();
        //fdecl->getParamDecl(i)->dump();
        if(i!=fdecl->getNumParams()-1) pstring+=std::string(",");
      }

      pstring+=std::string(")");
      pstring+=std::string("\n{\n");

      //CONVERT
      {
        std::string fdeclname =fdecl->getNameInfo().getName().getAsString();
        //
        //conversions and type casting
        //
        //Section 6.2 Conversions and Type Casting
        //Section 6.2.3 Explicit Conversions
        //convert_ucharn_rte_sat(charn)
        if((fdeclname.substr(0,8)==std::string("convert_"))){
          builtinfile=conversions;
          //check for rounding mode set
          //by default conversion to integer is RTZ = trunc
          //conversion to float is RTE
          std::string fdeclnamestr=fdeclname;
          //RTE
          bool extraround=false;
          std::string roundstring,fproundstring;
          std::size_t rtefound = fdeclnamestr.find(std::string("_rte"));
          if(rtefound!=std::string::npos){
            extraround=true;
            roundstring=std::string("rte");
            fproundstring=std::string("FE_TONEAREST");
            builtinfilesplit=1;
          }
          //RTZ
          std::size_t rtzfound = fdeclnamestr.find(std::string("_rtz"));
          if(rtzfound!=std::string::npos){
            extraround=true;
            roundstring=std::string("rtz");
            fproundstring=std::string("FE_TOWARDZERO");
            builtinfilesplit=2;
          }
          //RTP
          std::size_t rtpfound = fdeclnamestr.find(std::string("_rtp"));
          if(rtpfound!=std::string::npos){
            extraround=true;
            roundstring=std::string("rtp");
            fproundstring=std::string("FE_UPWARD");
            builtinfilesplit=3;
          }
          //RTN
          std::size_t rtnfound = fdeclnamestr.find(std::string("_rtn"));
          if(rtnfound!=std::string::npos){
            extraround=true;
            roundstring=std::string("rtn");
            fproundstring=std::string("FE_DOWNWARD");
            builtinfilesplit=4;
          }

          //find _sat
          const clang::FunctionType *AFT= fdecl->getType()->getAs<clang::FunctionType>();
          clang::QualType rqt=AFT->getResultType();
          const clang::Type *rtp=rqt.getTypePtr();
          returntype=rtp;
          const clang::BuiltinType *returnbasetype = get_builtintype_vectorbasetype(returntype);
          std::string returnbasetypestr;
          get_implementation_vectorbasetype(returntype,returnbasetypestr);
          int returnsize=get_builtintype_size(returnbasetype);
          //arg0 type
          const clang::Type *arg0type = fdecl->getParamDecl(0)->getType().getTypePtr();
          const clang::BuiltinType *arg0basetype = get_builtintype_vectorbasetype(arg0type);
          std::string arg0basetypestr;
          get_implementation_vectorbasetype(arg0type,arg0basetypestr);
          int arg0size=get_builtintype_size(arg0basetype);

          std::size_t satfound = fdeclnamestr.find(std::string("_sat"));
          if(satfound!=std::string::npos){
            std::string unopstring("");
            //Conformance test derived implementations
            //  conversions to/from float with saturation
            //Conversions to floating point : conformance test derived
            if(returnbasetype->isFloatingPoint()) {
              //conversion to float with saturation not part of OpenCL
              printf("Error : attempting to generate builtin for conversion to floating with saturation \n");
              exit(1);
            }
            //conversion from float
            else if(arg0basetype->isFloatingPoint()){
              pstring += rqtstring + std::string(" out;\n");
              if(extraround){
                unopstring=std::string("convertfromfloatsat_impl(")+roundstring+std::string("(inelement),&outelement)");
              }else{
                unopstring=std::string("convertfromfloatsat_impl(inelement,&outelement)");
              }
              createunopvectormap(unopstring+std::string(";"),fdecl->getParamDecl(0),returntype,std::string("out"),pstring);
              pstring += std::string("return(out);\n");
            }
            //Conversions to all types except floating point : xilinx derived
            else{
              //conformance test derived
             //S->U
              if(arg0basetype->isSignedInteger() && returnbasetype->isUnsignedInteger()){
                //signed source and unsigned dest
                //saturate source min to 0
                //MIN range
                unopstring+=std::string("(temp0element<0) ? 0 : temp0element");
                //MAX range
                if(arg0size>returnsize || arg0basetype->isFloatingPoint()){
                  //MAX range
                  std::string arg0maxoutput("0x");
                  for(unsigned int sm=0;sm<returnsize;sm++) arg0maxoutput+=std::string("FF");
                  std::string arg0max;
                  arg0max=arg0maxoutput;
                  arg0max=std::string("((")+arg0basetypestr+std::string(")")+arg0max+std::string(")");
                  unopstring=std::string("(temp0element>=")+arg0max+std::string(" ) ? ")+arg0maxoutput+std::string(": (") + unopstring + std::string(")");
                }
              }
              else if(arg0basetype->isSignedInteger() &&
                      (returnbasetype->isSignedInteger()||returnbasetype->isFloatingPoint())){
                //S->S
                //signed source and signed dest
                //if (Integer to Integer && sizeof source > sizeof dest ) or (float to int) then saturate to min of dest in source type
                if((arg0basetype->isSignedInteger() && returnbasetype->isSignedInteger() && arg0size>returnsize) ||
                   (returnbasetype->isSignedInteger() && arg0basetype->isFloatingPoint()) ){
                  //MIN range
                  /*
                  for(unsigned int sm=0;sm<(arg0size-returnsize)-1;sm++) arg0min+=std::string("FF");
                  arg0min+=std::string("80");
                  for(unsigned int sm=1;sm<(returnsize)-1;sm++) arg0min+=std::string("00");
                  */
                  std::string arg0minoutput("0x");
                  std::string arg0min;
                  arg0minoutput+=std::string("80");
                  for(unsigned int sm=0;sm<(returnsize-1);sm++) arg0minoutput+=std::string("00");
                  arg0minoutput=std::string("((")+returnbasetypestr+std::string(")")+arg0minoutput+std::string(")");
                  arg0min=std::string("(")+std::string("(")+arg0basetypestr+std::string(")")+arg0minoutput+std::string(")");
                  unopstring+=std::string("(temp0element<")+arg0min+std::string(" ) ? ")+arg0minoutput+std::string(": temp0element");
                  //MAX range
                  std::string arg0max("0x7F");
                  for(unsigned int sm=0;sm<(returnsize-1);sm++) arg0max+=std::string("FF");
                  arg0max=std::string("((")+arg0basetypestr+std::string(")")+arg0max+std::string(")");
                  std::string arg0maxoutput("0x7F");
                  for(unsigned int sm=0;sm<(returnsize-1);sm++) arg0maxoutput+=std::string("FF");
                  arg0maxoutput=std::string("((")+returnbasetypestr+std::string(")")+arg0maxoutput+std::string(")");
                  unopstring=std::string("(temp0element>=")+arg0max+std::string(" ) ? ")+arg0maxoutput+std::string(": (")+unopstring + std::string(")");
                }else{
                  unopstring=std::string("temp0element");
                }
              }
              else if((arg0basetype->isUnsignedInteger()) &&
                      (returnbasetype->isSignedInteger() || returnbasetype->isFloatingPoint())){
                //U->S
                //unsigned source signed dest
                if(arg0size>=returnsize){
                  //MAX range
                  std::string arg0maxoutput("0x");
                  arg0maxoutput+=std::string("7F");
                  for(unsigned int sm=1;sm<returnsize;sm++) arg0maxoutput+=std::string("FF");
                  std::string arg0max;
                  arg0max=arg0maxoutput;
                  arg0max=std::string("((")+arg0basetypestr+std::string(")")+arg0maxoutput+std::string(")");
                  unopstring=std::string("(temp0element>=")+arg0max+std::string(" ) ? ")+arg0maxoutput+std::string(": temp0element");
                }else{
                  unopstring=std::string("temp0element");
                }
              }
              else{
                //U->U
                if(arg0size>returnsize){
                  //MAX range
                  std::string arg0maxoutput("0x");
                  for(unsigned int sm=0;sm<returnsize;sm++) arg0maxoutput+=std::string("FF");
                  std::string arg0max;
                  arg0max=arg0maxoutput;
                  arg0max=std::string("((")+arg0basetypestr+std::string(")")+arg0max+std::string(")");
                  unopstring=std::string("(temp0element>=")+arg0max+std::string(" ) ? ")+arg0maxoutput+std::string(": temp0element");
                }else{
                  unopstring=std::string("temp0element");
                }
              }
              //compare in source type
              pstring += rqtstring + std::string(" out;\n");
              if(extraround && arg0basetype->isFloatingPoint()){
                //F ->
                createunopvectormap(std::string("temp0element=")+roundstring+std::string("(inelement); outelement=")+unopstring+std::string(";"),fdecl->getParamDecl(0),returntype,std::string("out"),pstring);
              }else{
                //!F ->
                createunopvectormap(std::string("temp0element=")+std::string("(inelement); outelement=")+unopstring+std::string(";"),fdecl->getParamDecl(0),returntype,std::string("out"),pstring);
              }
              pstring += std::string("return(out);\n");
            }
          }
          //non saturating case
          else{
            //Conversions to floating point : conformance test derived
            if(returnbasetype->isFloatingPoint()) {
              if(arg0basetype->isFloatingPoint()){
                //float->float
                pstring += rqtstring + std::string(" out;\n");
                createunopvectormap(std::string("outelement=inelement;"),fdecl->getParamDecl(0),returntype,std::string("out"),pstring);
                pstring += std::string("return(out);\n");
              }
              else{
                //nonfloat->float
                pstring += rqtstring + std::string(" out;\n");
                //std::string unopstring("");
                //unopstring=std::string("convert2float_impl(inelement)");
                //if(extraround) createunopvectormap(std::string("outelement=")+roundstring+std::string("(")+unopstring+std::string(");"),fdecl->getParamDecl(0),returntype,std::string("out"),pstring);
                //else createunopvectormap(std::string("outelement=")+unopstring+std::string(";"),fdecl->getParamDecl(0),returntype,std::string("out"),pstring);
                //pstring += std::string("return(out);\n");
                if(!extraround) createunopvectormap(std::string("outelement=hls_convert_float_with_rounding(inelement,FE_TONEAREST);"),fdecl->getParamDecl(0),returntype,std::string("out"),pstring);
                else createunopvectormap(std::string("outelement=hls_convert_float_with_rounding(inelement,")+fproundstring+std::string(");"),fdecl->getParamDecl(0),returntype,std::string("out"),pstring);
                pstring += std::string("return(out);\n");
              }
            }
            else{
              const clang::Type *arg0type = fdecl->getParamDecl(0)->getType().getTypePtr();
              const clang::BuiltinType *arg0basetype = get_builtintype_vectorbasetype(arg0type);
              pstring += rqtstring + std::string(" out;\n");
              //F ->
              if(extraround && arg0basetype->isFloatingPoint()){
                createunopvectormap(std::string("outelement=")+roundstring+std::string("(inelement);"),fdecl->getParamDecl(0),returntype,std::string("out"),pstring);
              }else{
                //!F->
                createunopvectormap(std::string("outelement=inelement;"),fdecl->getParamDecl(0),returntype,std::string("out"),pstring);
              }
              pstring += std::string("return(out);\n");
            }
          }
        }

        //Section 6.2.4 Reinterpreting Data As Another Type
        if(fdeclname.substr(0,3)==std::string("as_")){
          builtinfile=conversions;
          builtinfilesplit=5;
          pstring += "  " + rqtstring + std::string(" out;\n");

          std::string input_base_type_str;
          std::string output_base_type_str;

          get_implementation_vectorbasetype(fdecl->getParamDecl(0)->getType().getTypePtr(), input_base_type_str);
          get_implementation_vectorbasetype(returntype, output_base_type_str);

          int input_elements=1;
          int output_elements=1;

          if(const clang::VectorType *VT=dyn_cast<clang::VectorType>(fdecl->getParamDecl(0)->getType().getTypePtr()->getCanonicalTypeInternal())){
            input_elements=VT->getNumElements();
          }
          if(const clang::VectorType *VT = dyn_cast<clang::VectorType>(returntype->getCanonicalTypeInternal())) {
            output_elements = VT->getNumElements();
          }

          if (input_elements == 1 && output_elements == 1) {
            pstring += std::string("  out = *("+rqtstring+"*)&f;\n");
          }
          else if (input_elements == output_elements ||
                   (input_elements==4 && output_elements==3) ||
                   (output_elements==4 && input_elements==3)) {
            for (int i=0; i<output_elements; i++) {
              char idx;
              if (i > 9) idx = 'a' + (i-10);
              else       idx = '0' + i;
              std::stringstream ss;
              if (i < input_elements) {
                ss << "  " << input_base_type_str << " tmp_in" << idx << " = f.s" << idx << ";\n";
                ss << "  " << output_base_type_str << " tmp_out" << idx << " = *(" << output_base_type_str << "*)&tmp_in" << idx << ";\n";
                if (output_elements == 1)
                  ss << "  out = tmp_out" << idx << ";\n";
                else
                  ss << "  out.s" << idx << " = tmp_out" << idx << ";\n";
              }
              else {
                ss << "  out.s3 = 0;\n";
              }
              pstring += ss.str();
            }
          }
          else if (input_elements > output_elements) {
            int input_size;
            std::string tmp_in_type;
            if (input_base_type_str.find("char") != std::string::npos) {
              input_size = 8;
              tmp_in_type = "__spir_uchar_t";
            } 
            else if (input_base_type_str.find("short") != std::string::npos) {
              input_size = 16;
              tmp_in_type = "__spir_ushort_t";
            } 
            else if (input_base_type_str.find("int") != std::string::npos) {
              input_size = 32;
              tmp_in_type = "__spir_uint_t";
            } 
            else if (input_base_type_str.find("float") != std::string::npos) {
              input_size = 32;
              tmp_in_type = "__spir_uint_t";
            } 
            else
              assert(0 && "unhandled input size");

            for (int i=0; i<input_elements; i++) {
              char idx;
              if (i > 9) idx = 'a' + (i-10);
              else       idx = '0' + i;
              std::stringstream ss;
              if (input_base_type_str != tmp_in_type) {
                ss << "  " << input_base_type_str << " tmp_in" << idx << "_elem = f.s" << idx << ";\n";
                ss << "  " << tmp_in_type << " tmp_in" << idx << " = *(" << tmp_in_type << "*)&tmp_in" << idx << "_elem;\n";
              } else {
                ss << "  " << tmp_in_type << " tmp_in" << idx << " = f.s" << idx << ";\n";
              }
              pstring += ss.str();
            }

            int output_size;
            std::string tmp_out_type;
            if (output_base_type_str.find("short") != std::string::npos) {
              output_size = 16;
              tmp_out_type = "__spir_ushort_t";
            } 
            else if (output_base_type_str.find("int") != std::string::npos) {
              output_size = 32;
              tmp_out_type = "__spir_uint_t";
            } 
            else if (output_base_type_str.find("float") != std::string::npos) {
              output_size = 32;
              tmp_out_type = "__spir_uint_t";
            } 
            else if (output_base_type_str.find("long") != std::string::npos) {
              output_size = 64;
              tmp_out_type = "__spir_ulong_t";
            } 
            else
              assert(0 && "unhandled output size");

            int in_per_out = output_size / input_size;
            for (int i=0; i<output_elements; i++) {
              std::stringstream ss;
              ss << "  " << tmp_out_type << " tmp_out" << i << " = 0;\n";
              for (int j=in_per_out-1; j>=0; j--) {
                int k = i*in_per_out + j;
                char idx;
                if (k > 9) idx = 'a' + (k-10);
                else       idx = '0' + k;
                ss << "  tmp_out" << i << " |= tmp_in" << idx << ";\n";
                if (j) ss << "  tmp_out" << i << " <<= " << input_size << ";\n";
              }
              if (output_elements == 1)
                ss << "  out = " << "*(" << output_base_type_str << "*)&tmp_out" << i << ";\n";
              else
                ss << "  out.s" << i << " = " << "*(" << output_base_type_str << "*)&tmp_out" << i << ";\n";
              pstring += ss.str();
            }
          }
          else if (input_elements < output_elements) {
            int input_size;
            std::string tmp_in_type;
            if (input_base_type_str.find("short") != std::string::npos) {
              input_size = 16;
              tmp_in_type = "__spir_ushort_t";
            } 
            else if (input_base_type_str.find("int") != std::string::npos) {
              input_size = 32;
              tmp_in_type = "__spir_uint_t";
            } 
            else if (input_base_type_str.find("float") != std::string::npos) {
              input_size = 32;
              tmp_in_type = "__spir_uint_t";
            } 
            else if (input_base_type_str.find("long") != std::string::npos) {
              input_size = 64;
              tmp_in_type = "__spir_ulong_t";
            } 
            else
              assert(0 && "unhandled input size");

            // extract input elements into unsigned types
            for (int i=0; i<input_elements; i++) {
              char idx = '0' + i;
              std::stringstream ss;
              if (input_base_type_str != tmp_in_type) {
                if (input_elements == 1)
                  ss << "  " << input_base_type_str << " tmp_in" << idx << "_elem = f;\n";
                else
                  ss << "  " << input_base_type_str << " tmp_in" << idx << "_elem = f.s" << idx << ";\n";
                ss << "  " << tmp_in_type << " tmp_in" << idx << " = *(" << tmp_in_type << "*)&tmp_in" << idx << "_elem;\n";
              } else {
                if (input_elements == 1)
                  ss << "  " << tmp_in_type << " tmp_in" << idx << " = f;\n";
                else
                  ss << "  " << tmp_in_type << " tmp_in" << idx << " = f.s" << idx << ";\n";
              }
              pstring += ss.str();
            }
            
            // output temporaries
            int output_size;
            std::string tmp_out_type, out_mask;
            if (output_base_type_str.find("char") != std::string::npos) {
              output_size = 8;
              tmp_out_type = "__spir_uchar_t";
              out_mask = "0xff";
            } 
            else if (output_base_type_str.find("short") != std::string::npos) {
              output_size = 16;
              tmp_out_type = "__spir_ushort_t";
              out_mask = "0xffff";
            } 
            else if (output_base_type_str.find("int") != std::string::npos) {
              output_size = 32;
              tmp_out_type = "__spir_uint_t";
              out_mask = "0xffffffff";
            } 
            else if (output_base_type_str.find("float") != std::string::npos) {
              output_size = 32;
              tmp_out_type = "__spir_uint_t";
              out_mask = "0xffffffff";
            } 
            else
              assert(0 && "unhandled output size");

            // get the bits
            for (int i=0; i<input_elements; i++) {
              int out_per_in = input_size / output_size;
              for (int j=0; j<out_per_in; j++) {
                int k = i*out_per_in + j;
                char idx;
                if (k > 9) idx = 'a' + (k-10);
                else       idx = '0' + k;
                std::stringstream ss;
                ss << "  " << tmp_out_type << " tmp_out" << idx << " = ";
                ss << "(" << tmp_out_type << ")((tmp_in" << i << " >>" << j*output_size << ") & " << out_mask << ");\n";
                pstring += ss.str();
              }
            }

            // copy result to return value
            for (int i=0; i<output_elements; i++) {
              std::stringstream ss;
              char idx;
              if (i > 9) idx = 'a' + (i-10);
              else       idx = '0' + i;
              ss << "  " << output_base_type_str << " tmp_out_elem" << idx << " = ";
              ss << "*(" << output_base_type_str << "*)&" << "tmp_out" << idx << ";\n";
              ss << "  out.s" << idx << " = tmp_out_elem" << idx << ";\n";
              pstring += ss.str();
            }
          }
          else {
            pstring += std::string("  // default\n");
            pstring += std::string("  out = *("+rqtstring+"*)&f;\n");
          }
          pstring += std::string("  return out;\n");
        }

        //
        //math builtins
        //
        //unary operations
        //
        //ACOS,ACOSH,ASIN,ASINH,ATAN,ATANH,CBRT,CEIL,COS,COSH,ERFC,ERF,EXP,EXP2,EXPM1,FABS,FLOOR,LOG,LOG10,
        //RINT,SIN,SINH,SQRT,TAN,TANH,TGAMMA,TRUNC
        //RSQRT via _impl
        //EXP10 via _impl
        //ILOGB via _reference
        //ROOTN via _impl
        //half functions
        //HALF_COS, HALF_EXP, HALF_EXP2, HALF_EXP10, HALF_LOG, HALF_LOG10
        //HALF_LOG2 via _impl
        //HALF_RECIP via _impl
        //HALF_RSQRT via impl
        //HALF_TAN
        //unary operations with existing math.h implementations
        const char *unopmathc[] = {"acos","acosh","asin","asinh","atan","atanh","cbrt","ceil","cos","cosh","erfc","erf",
                                   "expm1","fabs","floor","lgamma", "tgamma",
                                   "log","log10","rint","round","sin","sinh","sqrt","tan","tanh","exp2","exp",
                                   "trunc","exp10","ilogb","rsqrt","log2","log1p","logb",
                                   "half_cos","half_sin","half_exp","half_exp2","half_exp10","half_log","half_log2","half_log10",
                                   "half_recip","half_rsqrt","half_sqrt","half_tan","asinpi","acospi","atanpi","recip","cospi","sinpi","tanpi",
        };
/*
        const char *unopmathcimpl[] = {"acosf","acoshf","asinf","asinhf","atanf","atanhf","cbrtf","ceilf","cos","coshf","erfc","erf",
          "expm1f","fabsf","floorf",
          "logf","log10f","rintf","roundf","sin","sinhf","sqrtf","tan","tanhf","exp2f","expf",
          "truncf","exp10f_impl","reference_ilogb","rsqrt_impl","log2_impl","log1p_impl",
          "cos","sin","expf","exp2f","exp10f_impl","logf","log2_impl","log10f",
          "half_recip_impl","rsqrt_impl","sqrtf","tan"};
*/
//replacing with all single precision floating point versions
        const char *unopmathcimpl[] = {"hls_acos","hls_acosh","hls_asin","hls_asinh","hls_atan","hls_atanh","hls_cbrt","hls_ceil","hls_cos","hls_cosh",
                                       "erfc_impl","erf_impl","hls_expm1","hls_fabs","hls_floor","hls_lgamma", "tgamma_impl",
                                       "hls_log","hls_log10","hls_rint","hls_round","hls_sin","hls_sinh","hls_sqrt","hls_tan","hls_tanh","hls_exp2","hls_exp",
                                       "hls_trunc","hls_exp10","hls_ilogb","hls_rsqrt","hls_log2","hls_log1p","hls_logb",
                                       "hls_cos","hls_sin","hls_exp","hls_exp2","hls_exp10","hls_log","hls_log2","hls_log10",
                                       "half_recip_impl","hls_rsqrt","hls_sqrt","hls_tan","hls_asinpi","hls_acospi","hls_atanpi","hls_recip","hls_cospi","hls_sinpi","hls_tanpi"
        };
        std::vector<std::string> unopmath(unopmathc,unopmathc+sizeof(unopmathc)/sizeof(unopmathc[0]));
        std::vector<std::string> unopmathimpl(unopmathcimpl,unopmathcimpl+sizeof(unopmathcimpl)/sizeof(unopmathcimpl[0]));
        std::vector<std::string>::iterator itimpl=unopmathimpl.begin();
        for(std::vector<std::string>::iterator it=unopmath.begin(); it!=unopmath.end(); it++,itimpl++){
          if(fdeclname==(*it)){
            builtinfile=math;
            pstring += rqtstring + std::string(" out;\n");
            createunopvectormap(std::string("outelement=")+(*itimpl)+std::string("(inelement);"),fdecl->getParamDecl(0),returntype,std::string("out"),pstring);
            pstring += std::string("return(out);\n");
          }
        }

        //ACOSPI, ASINPI, ATANPI
        /*
        {
          const char *unopmathpic[] = {"acospi","asinpi","atanpi"};
          const char *unopmathpicimpl[] = {"acos_impl","asin_impl","atanf"};
          std::vector<std::string> unopmathpi(unopmathpic,unopmathpic+sizeof(unopmathpic)/sizeof(unopmathpic[0]));
          std::vector<std::string> unopmathpiimpl(unopmathpicimpl,unopmathpicimpl+sizeof(unopmathpicimpl)/sizeof(unopmathpicimpl[0]));
          itimpl=unopmathpiimpl.begin();
          for(std::vector<std::string>::iterator it=unopmathpi.begin();it!=unopmathpi.end();it++,itimpl++){
            if(fdeclname==(*it)){
              builtinfile=math;
              pstring += rqtstring + std::string(" out;\n");
              createunopvectormap(std::string("outelement=")+(*itimpl)+std::string("(inelement)/M_PI;"),fdecl->getParamDecl(0),returntype,std::string("out"),pstring);
              pstring += std::string("return(out);\n");
            }
          }
        }
        //COSPI, SINPI, TANPI
        {
          const char *unopmathpic[] = {"cospi","sinpi","tanpi"};
          const char *unopmathpicimpl[] = {"cos","sin","tan"};
          std::vector<std::string> unopmathpi(unopmathpic,unopmathpic+sizeof(unopmathpic)/sizeof(unopmathpic[0]));
          std::vector<std::string> unopmathpiimpl(unopmathpicimpl,unopmathpicimpl+sizeof(unopmathpicimpl)/sizeof(unopmathpicimpl[0]));
          itimpl=unopmathpiimpl.begin();
          for(std::vector<std::string>::iterator it=unopmathpi.begin();it!=unopmathpi.end();it++,itimpl++){
            if(fdeclname==(*it)){
              builtinfile=math;
              pstring += rqtstring + std::string(" out;\n");
              createunopvectormap(std::string("outelement=")+(*itimpl)+std::string("(inelement*M_PI);"),fdecl->getParamDecl(0),returntype,std::string("out"),pstring);
              pstring += std::string("return(out);\n");
            }
          }
        }
        */

        //NAN
        //do not place nancodes into the significand of the resulting NaN
        if(fdeclname==std::string("nan")){
          builtinfile=math;
          pstring += rqtstring + std::string(" out;\n");
          createunopvectormap(std::string("outelement=NAN;"),fdecl->getParamDecl(0),returntype,std::string("out"),pstring);
          pstring += std::string("return(out);\n");
        }


        //binary operations
        //ATAN2 COPYSIGN NEXTAFTER POW
        //POWN implemented with POWF
        //POWR reference_
        //FMOD _impl
        //ROOTN _impl
        //HALF_DIVIDE _impl
        {
          const char *unopmathc[] = {"atan2","copysign","nextafter","pow","remainder","powr","fmod","rootn",
                                     "half_divide","half_powr","pown","fdim","maxmag","minmag","atan2pi","hypot"};
          const char *unopmathcimpl[] = {"hls_atan2","hls_copysign","hls_nextafter","hls_pow","hls_remainder","hls_powr","hls_fmod","hls_rootn",
                                         "half_divide_impl","hls_powr","hls_pown","hls_fdim","hls_maxmag","hls_minmag","hls_atan2pi","hls_hypot"};
          std::vector<std::string> unopmath(unopmathc,unopmathc+sizeof(unopmathc)/sizeof(unopmathc[0]));
          std::vector<std::string> unopmathimpl(unopmathcimpl,unopmathcimpl+sizeof(unopmathcimpl)/sizeof(unopmathcimpl[0]));
          std::vector<std::string>::iterator itimpl=unopmathimpl.begin();
          for(std::vector<std::string>::iterator it=unopmath.begin();it!=unopmath.end();it++,itimpl++){
            if(fdeclname==(*it)){
              builtinfile=math;
              pstring += rqtstring + std::string(" out;\n");
              createbinopvectormap(std::string("outelement=")+(*itimpl)+std::string("(in0,in1);"),fdecl->getParamDecl(0),fdecl->getParamDecl(1),std::string("out"),returntype,pstring);
              pstring += std::string("return(out);\n");
            }
          }
        }
        //LDEXP
        if(fdeclname==std::string("ldexp")){
          builtinfile=math;
          pstring += rqtstring + std::string(" out;\n");
          //handle
          //gentype ldexp(gentype x, gentype y)
          //gentype ldexp(gentype x, int y)
          createbinopvectormap4(std::string("outelement=hls_ldexp(in0,in1);"),fdecl->getParamDecl(0),fdecl->getParamDecl(1),std::string("out"),pstring);
          pstring += std::string("return(out);\n");
        }
 

        /*
        //ATAN2PI
        {
          const char *unopmathc[] = {"atan2pi"};
          const char *unopmathcimpl[] = {"atan2_impl"};
          std::vector<std::string> unopmath(unopmathc,unopmathc+sizeof(unopmathc)/sizeof(unopmathc[0]));
          std::vector<std::string> unopmathimpl(unopmathcimpl,unopmathcimpl+sizeof(unopmathcimpl)/sizeof(unopmathcimpl[0]));
          std::vector<std::string>::iterator itimpl=unopmathimpl.begin();
          for(std::vector<std::string>::iterator it=unopmath.begin();it!=unopmath.end();it++,itimpl++){
            if(fdeclname==(*it)){
              builtinfile=math;
              pstring += rqtstring + std::string(" out;\n");
              createbinopvectormap(std::string("outelement=")+(*itimpl)+std::string("(in0,in1)/M_PI;"),fdecl->getParamDecl(0),fdecl->getParamDecl(1),std::string("out"),returntype,pstring);
//              createbinopvectormap(std::string("outelement=")+(it->substr(0,it->length()-2))+"f"+std::string("(in0,in1)/M_PI;"),fdecl->getParamDecl(0),fdecl->getParamDecl(1),std::string("out"),pstring);
              pstring += std::string("return(out);\n");
            }
          }
        }
        */
        //FMIN
        if(fdeclname==std::string("fmin")){
          //y if y<x otherwise returnes x. If one arg is Nan returns other arg if both Nan, returns Nan
					const clang::Type *arg0type = fdecl->getParamDecl(0)->getType().getTypePtr();
          const clang::BuiltinType *arg0basetype = get_builtintype_vectorbasetype(arg0type);
					const clang::Type *arg1type = fdecl->getParamDecl(1)->getType().getTypePtr();
          const clang::BuiltinType *arg1basetype = get_builtintype_vectorbasetype(arg1type);
					bool arg0vectortype = false;
					bool arg1vectortype = false;
					if(const clang::VectorType *VT = dyn_cast<clang::VectorType>(arg0type->getCanonicalTypeInternal())) arg0vectortype=true;
					if(const clang::VectorType *VT = dyn_cast<clang::VectorType>(arg1type->getCanonicalTypeInternal())) arg1vectortype=true;
          builtinfile=math;
          pstring += rqtstring + std::string(" out;\n");
					if(arg0vectortype == arg1vectortype)
						createbinopvectormap(std::string("outelement= hls_fmin(in0,in1);"),fdecl->getParamDecl(0),fdecl->getParamDecl(1),std::string("out"),returntype,pstring);
					else
						createbinopvectormap3(std::string("outelement= hls_fmin(in0,in1);"),fdecl->getParamDecl(0),fdecl->getParamDecl(1),std::string("out"),pstring);

          pstring += std::string("return(out);\n");

        }
        //FMAX
        if(fdeclname==std::string("fmax")){
          //x if y<x otherwise returnes y. If one arg is Nan returns other arg if both Nan, returns Nan
					const clang::Type *arg0type = fdecl->getParamDecl(0)->getType().getTypePtr();
          const clang::BuiltinType *arg0basetype = get_builtintype_vectorbasetype(arg0type);
					const clang::Type *arg1type = fdecl->getParamDecl(1)->getType().getTypePtr();
          const clang::BuiltinType *arg1basetype = get_builtintype_vectorbasetype(arg1type);
					bool arg0vectortype = false;
					bool arg1vectortype = false;
					if(const clang::VectorType *VT = dyn_cast<clang::VectorType>(arg0type->getCanonicalTypeInternal())) arg0vectortype=true;
					if(const clang::VectorType *VT = dyn_cast<clang::VectorType>(arg1type->getCanonicalTypeInternal())) arg1vectortype=true;
          builtinfile=math;
          pstring += rqtstring + std::string(" out;\n");
					if(arg0vectortype == arg1vectortype)
						createbinopvectormap(std::string("outelement= hls_fmax(in0,in1);"),fdecl->getParamDecl(0),fdecl->getParamDecl(1),std::string("out"),returntype,pstring);
					else
						createbinopvectormap3(std::string("outelement= hls_fmax(in0,in1);"),fdecl->getParamDecl(0),fdecl->getParamDecl(1),std::string("out"),pstring);
          pstring += std::string("return(out);\n");
        }
        //FRACT
        if(fdeclname==std::string("fract")){
          //x if y<x otherwise returnes y. If one arg is Nan returns other arg if both Nan, returns Nan
          builtinfile=math;
          createbinopvectormapfract(
              //std::string("tempoutelement=in0 - floor(in0); outelement=(isnan(tempoutelement) ? 0x1.fffffep-1f : (tempoutelement <= 0x1.fffffep-1f ? tempoutelement : 0x1.fffffep-1f ));"),
              // std::string("outelementptr=floor(in0);"),
              std::string(""),
              std::string("outelement=hls_fract(in0,&outelementptr);"),
              fdecl->getParamDecl(0),fdecl->getParamDecl(1),returntype,
              std::string("out"),
              std::string("outparam"),
              pstring);
          pstring += std::string("(*g)=outparam;\n");
          pstring += std::string("return(out);\n");
        }
        //LGAMMA_R
        if(fdeclname==std::string("lgamma_r")){
          builtinfile=math;
          createbinopvectormapfract(
              std::string(""),
              std::string("outelement=hls_lgamma_r(in0,&outelementptr);"),
              fdecl->getParamDecl(0),fdecl->getParamDecl(1),returntype,
              std::string("out"),
              std::string("outparam"),
              pstring);
          pstring += std::string("(*g)=outparam;\n");
          pstring += std::string("return(out);\n");
        }
        //MODF
        if(fdeclname==std::string("modf")){
          //x if y<x otherwise returnes y. If one arg is Nan returns other arg if both Nan, returns Nan
          builtinfile=math;
          createbinopvectormapfract(
              std::string(""),
              std::string("outelement=hls_modf(in0,&outelementptr);"),
              fdecl->getParamDecl(0),fdecl->getParamDecl(1),returntype,
              std::string("out"),
              std::string("outparam"),
              pstring);
          pstring += std::string("(*g)=outparam;\n");
          pstring += std::string("return(out);\n");
        }
        //SINCOS
        if(fdeclname==std::string("sincos")){
          builtinfile=math;
          createbinopvectormapfract(
              std::string("outelement=hls_sin(in0);"),
              std::string("outelementptr=hls_cos(in0);"),fdecl->getParamDecl(0),fdecl->getParamDecl(1),returntype,
              std::string("out"),
              std::string("outparam"),
              pstring);
          pstring += std::string("(*g)=outparam;\n");
          pstring += std::string("return(out);\n");
        }
        //FREXP
        if(fdeclname==std::string("frexp")){
          builtinfile=math;
          createbinopvectormapfract(
              std::string("outelement=hls_frexp(in0,&outelementptr);"),
              std::string(""),fdecl->getParamDecl(0),fdecl->getParamDecl(1),returntype,
              std::string("out"),
              std::string("outparam"),
              pstring);
          pstring += std::string("(*g)=outparam;\n");
          pstring += std::string("return(out);\n");
        }
        /*
        //MAXMAG
        if(fdeclname==std::string("maxmag")){
          builtinfile=math;
          pstring += rqtstring + std::string(" out;\n");
          createbinopvectormap(
              std::string("outelement=hls_maxmag(in0,in1);"),
              fdecl->getParamDecl(0),fdecl->getParamDecl(1),std::string("out"),returntype,pstring);
          pstring += std::string("return(out);\n");
        }
        //MINMAG
        if(fdeclname==std::string("minmag")){
          builtinfile=math;
          pstring += rqtstring + std::string(" out;\n");
          createbinopvectormap(
              std::string("temp0element=fabsf(in0); temp1element=fabsf(in1); outelement=(temp0element<temp1element ? in0 : (temp1element<temp0element ? in1 : (isnan(in0) ? in1 : (isnan(in1) ? in0 : (in0 <= in1 ? in0 : in1 )))));"),
              fdecl->getParamDecl(0),fdecl->getParamDecl(1),std::string("out"),returntype,pstring);
          pstring += std::string("return(out);\n");
        }
        */


        //tri operatiorns
        //FMA
        if(fdeclname==std::string("fma")){
          builtinfile=math;
          pstring += rqtstring + std::string(" out;\n");
          createtriopvectormap(std::string("outelement=hls_fma(in0,in1,in2);"),
              fdecl->getParamDecl(0),fdecl->getParamDecl(1),fdecl->getParamDecl(2),std::string("out"),pstring);
          pstring += std::string("return(out);\n");
        }
        //MAD
        if(fdeclname==std::string("mad")){
          builtinfile=math;
          pstring += rqtstring + std::string(" out;\n");
          createtriopvectormap(std::string("outelement=hls_mad(in0,in1,in2);"),
              fdecl->getParamDecl(0),fdecl->getParamDecl(1),fdecl->getParamDecl(2),std::string("out"),pstring);
          pstring += std::string("return(out);\n");
        }
        //REMQUO
        if(fdeclname==std::string("remquo")){
          //x if y<x otherwise returnes y. If one arg is Nan returns other arg if both Nan, returns Nan
          builtinfile=math;
          createtriopvectormapremquo(
              //std::string("tempoutelement=in0 - floor(in0); outelement=(isnan(tempoutelement) ? 0x1.fffffep-1f : (tempoutelement <= 0x1.fffffep-1f ? tempoutelement : 0x1.fffffep-1f ));"),
              // std::string("outelementptr=floor(in0);"),
              std::string(""),
              std::string("outelement=hls_remquo(in0,in1,&outelementptr);"),
              fdecl->getParamDecl(0),fdecl->getParamDecl(1),fdecl->getParamDecl(2),returntype,
              std::string("out"),
              std::string("outparam"),
              pstring);
          pstring += std::string("(*h)=outparam;\n");
          pstring += std::string("return(out);\n");
        }

        if (fdeclname.find("native_") == 0) {
          std::vector<std::string> hlsmathf{
            "cos", "exp", "log", "sin", "tan", "sqrt"
          };
          for (const auto &suffix : hlsmathf) {
            if (fdeclname == "native_"+suffix) {
              builtinfile=native;
              pstring += rqtstring + std::string(" out;\n");
              createunopvectormap(std::string("outelement=(float)")+suffix+std::string("f((float)(inelement));"),fdecl->getParamDecl(0),returntype,std::string("out"),pstring);
              pstring += std::string("return(out);\n");
            }
          }
          if (fdeclname == "native_divide") {
              builtinfile=native;
              pstring += rqtstring + std::string(" out;\n");
              createbinopvectormap(std::string("outelement= in0/in1;"),fdecl->getParamDecl(0),fdecl->getParamDecl(1),std::string("out"),returntype,pstring);
              pstring += std::string("return(out);\n");
          }
//          if (fdeclname == "recip") {
//
//          }
//          if (fdeclname == "powr") {
//            builtinfile=native;
//            pstring += rqtstring + std::string(" out;\n");
//            createbinopvectormap(std::string("if (x>=0) outelement= powf(in0,in1);"),fdecl->getParamDecl(0),fdecl->getParamDecl(1),std::string("out"),returntype,pstring);
//            pstring += std::string("return(out);\n");
//          }
        }
        //
        //integer builtins
        //
        //unary operations
        //
        //ABS
        if(fdeclname==std::string("abs")){
          //return type
          const clang::FunctionType *AFT= fdecl->getType()->getAs<clang::FunctionType>();
          clang::QualType rqt=AFT->getResultType();
          const clang::Type *rtp=rqt.getTypePtr();
          returntype=rtp;
          const clang::BuiltinType *returnbasetype = get_builtintype_vectorbasetype(returntype);
          std::string returnbasetypestr;
          get_implementation_vectorbasetype(returntype,returnbasetypestr);
          //arg0 type
          const clang::Type *arg0type = fdecl->getParamDecl(0)->getType().getTypePtr();
          const clang::BuiltinType *arg0basetype = get_builtintype_vectorbasetype(arg0type);
          builtinfile=integer;
          pstring += rqtstring + std::string(" out;\n");
          if(arg0basetype->isSignedInteger()){
            unsigned int i;
            std::string typemax("0x7F");
            int arg0size=get_builtintype_size(arg0basetype);
            for(i=0;i<(arg0size-1);i++) typemax+=std::string("FF");
            std::string typemin("0x80");
            for(i=0;i<(arg0size-1);i++) typemin+=std::string("00");
            //cannot simply invert sign if<0 because -ve range is 1 greater than positive range
            createunopvectormap(std::string("outelement = (inelement== "+typemin+" ? (("+returnbasetypestr+")"+typemax+"+1) : (inelement<0 ? (-inelement) : inelement));"),
                                fdecl->getParamDecl(0),returntype,std::string("out"),pstring);

          }else{
            createunopvectormap(std::string("outelement=inelement;"),fdecl->getParamDecl(0),returntype,std::string("out"),pstring);
          }
          pstring += std::string("return(out);\n");
        }
        //CLZ
        if(fdeclname==std::string("clz")){
          builtinfile=integer;
          pstring += rqtstring + std::string(" out;\n");
          createunopvectormap(std::string("outelement=clz_impl(inelement);"),fdecl->getParamDecl(0),returntype,std::string("out"),pstring);
          pstring += std::string("return(out);\n");
        }

        //POPCOUNT
        if(fdeclname==std::string("popcount")){
          builtinfile=integer;
          pstring += rqtstring + std::string(" out;\n");
          createunopvectormap(std::string(
              "temp0element=0; for(unsigned int i=0;i<sizeof(inelement)*8;i++){ if(inelement & 0x1) temp0element++; inelement=inelement>>1; }; outelement=temp0element;"),
              fdecl->getParamDecl(0),returntype,std::string("out"),pstring);
          pstring += std::string("return(out);\n");
        }

        //binary operations
        //
        //ABS_DIFF
        if(fdeclname==std::string("abs_diff")){
          builtinfile=integer;
          pstring += rqtstring + std::string(" out;\n");
          createbinopvectormap(std::string("outelement=(in1>in0) ? in1-in0 : in0 - in1;"),
              fdecl->getParamDecl(0),fdecl->getParamDecl(1),std::string("out"),returntype,pstring);
          pstring += std::string("return(out);\n");
        }

        //ADD_SAT
        if(fdeclname==std::string("add_sat")){
          //Unsigned a Unsigned b : if result < a && result < b saturate
          //Signed a Signed
          builtinfile=integer;
          //arg0 type
          const clang::Type *arg0type = fdecl->getParamDecl(0)->getType().getTypePtr();
          const clang::BuiltinType *arg0basetype = get_builtintype_vectorbasetype(arg0type);
          std::string arg0basetypestr;
          get_implementation_vectorbasetype(arg0type,arg0basetypestr);
          int arg0size=get_builtintype_size(arg0basetype);
          if(arg0basetype->isSignedInteger()){
            //Signed
            unsigned int i;
            std::string typemax("0x7F");
            for(i=0;i<(arg0size-1);i++) typemax+=std::string("FF");
            std::string typemin("0x80");
            for(i=0;i<(arg0size-1);i++) typemin+=std::string("00");
            pstring += rqtstring + std::string(" out;\n");
            //clang/llvm bug ?
            //temp0element > in0 not working
            createbinopvectormap(std::string("temp0element = in0+in1; outelement=((in1>0 && ((in0>=0 && temp0element<0) ||temp0element<in0)) ? ")+
                                 typemax+
                                 std::string(" : (( in1<0 && ((temp0element>=0 && in0<0) || temp0element>=in0)) ? ")+
                                 typemin+
                                 std::string(" : temp0element ));"),
                                 fdecl->getParamDecl(0),fdecl->getParamDecl(1),std::string("out"),returntype,pstring);
            pstring += std::string("return(out);\n");
          }else{
            //Unsigned
            unsigned int i;
            std::string typemax("0xFF");
            for(i=0;i<(arg0size-1);i++) typemax+=std::string("FF");
            std::string typemin("0x00");
            for(i=0;i<(arg0size-1);i++) typemin+=std::string("00");
            pstring += rqtstring + std::string(" out;\n");
            createbinopvectormap(std::string("temp0element = in0+in1; outelement= (temp0element<in0) ? ")+typemax+std::string(" : temp0element;"),
                fdecl->getParamDecl(0),fdecl->getParamDecl(1),std::string("out"),returntype,pstring);
            pstring += std::string("return(out);\n");

          }
        }

        //HADD
        if(fdeclname==std::string("hadd")){
          //arg0 type
          builtinfile=integer;
          pstring += rqtstring + std::string(" out;\n");
            createbinopvectormap(std::string("temp0element = (in0>>1) + (in1>>1); if((in0&0x1)&&(in1&0x1)) { temp0element++;} outelement=temp0element;"),
                fdecl->getParamDecl(0),fdecl->getParamDecl(1),std::string("out"),returntype,pstring);
          pstring += std::string("return(out);\n");
        }
        //RHADD
        if(fdeclname==std::string("rhadd")){
          //arg0 type
          builtinfile=integer;
          pstring += rqtstring + std::string(" out;\n");
            createbinopvectormap(std::string("temp0element = (in0>>1) + (in1>>1); if((in0&0x1) || (in1&0x1)) temp0element++; outelement=temp0element;"),
                fdecl->getParamDecl(0),fdecl->getParamDecl(1),std::string("out"),returntype,pstring);
          pstring += std::string("return(out);\n");
        }
        //MAX
        if(fdeclname==std::string("max")){
          //integer version
          const clang::Type *arg0type = fdecl->getParamDecl(0)->getType().getTypePtr();
          const clang::BuiltinType *arg0basetype = get_builtintype_vectorbasetype(arg0type);
          if(arg0basetype->isInteger()){
            builtinfile=integer;
            pstring += rqtstring + std::string(" out;\n");
            createbinopvectormap(std::string("outelement=(in0<in1) ? in1 : in0;"),
                fdecl->getParamDecl(0),fdecl->getParamDecl(1),std::string("out"),returntype,pstring);
            pstring += std::string("return(out);\n");
          }
					if(arg0basetype->isFloatingPoint()){
            builtinfile=commonfns;
            pstring += rqtstring + std::string(" out;\n");
            createbinopvectormap3(std::string("outelement=(in0<in1) ? in1 : in0;"),
                fdecl->getParamDecl(0),fdecl->getParamDecl(1),std::string("out"),pstring);
            pstring += std::string("return(out);\n");
          }
        }
        //MIN
        if(fdeclname==std::string("min")){
          //integer version
          const clang::Type *arg0type = fdecl->getParamDecl(0)->getType().getTypePtr();
          const clang::BuiltinType *arg0basetype = get_builtintype_vectorbasetype(arg0type);
          if(arg0basetype->isInteger()){
            builtinfile=integer;
            pstring += rqtstring + std::string(" out;\n");
            createbinopvectormap(std::string("outelement=(in1<in0) ? in1 : in0;"),
                fdecl->getParamDecl(0),fdecl->getParamDecl(1),std::string("out"),returntype,pstring);
            pstring += std::string("return(out);\n");
          }
					if(arg0basetype->isFloatingPoint()){
            builtinfile=commonfns;
            pstring += rqtstring + std::string(" out;\n");
            createbinopvectormap3(std::string("outelement=(in0<in1) ? in0 : in1;"),
                fdecl->getParamDecl(0),fdecl->getParamDecl(1),std::string("out"),pstring);
            pstring += std::string("return(out);\n");
          }
        }
        //ROTATE
        if(fdeclname==std::string("rotate")){
          //arg0 width
          const clang::Type *arg0type = fdecl->getParamDecl(0)->getType().getTypePtr();
          const clang::BuiltinType *arg0basetype = get_builtintype_vectorbasetype(arg0type);
          builtinfile=integer;
          pstring += rqtstring + std::string(" out;\n");
            createbinopvectormap(std::string("outelement=rotate_impl(in0,in1);"),
                fdecl->getParamDecl(0),fdecl->getParamDecl(1),std::string("out"),returntype,pstring);
          pstring += std::string("return(out);\n");
        }

        //SUB_SAT
        if(fdeclname==std::string("sub_sat")){
          // Inspired the logic in sub_sat.cl from POCL
          builtinfile=integer;
          //arg0 type
          const clang::Type *arg0type = fdecl->getParamDecl(0)->getType().getTypePtr();
          const clang::BuiltinType *arg0basetype = get_builtintype_vectorbasetype(arg0type);
          std::string arg0basetypestr;
          get_implementation_vectorbasetype(arg0type,arg0basetypestr);
          int arg0size=get_builtintype_size(arg0basetype);
          if(arg0basetype->isSignedInteger()){
            //Signed
            unsigned int i;
            std::string typemax("0x7F");
            for(i=0;i<(arg0size-1);i++) typemax+=std::string("FF");
            std::string typemin("0x80");
            for(i=0;i<(arg0size-1);i++) typemin+=std::string("00");
            pstring += rqtstring + std::string(" out;\n");
            /*
            createbinopvectormap(("temp0element = ((in0^in1) >= 0) ? in0 - in1 : ((in0 >= 0) ? (in0 > (in1 + ") +
                                 typemax + ") ? " + typemax + " : in0 - in1) : (in0 < (in1 + " + typemin + ") ? " + typemin + " : in0 - in1));",
                                 fdecl->getParamDecl(0),fdecl->getParamDecl(1),std::string("out"),returntype,pstring);
             */
            createbinopvectormap(std::string("temp0element = in0-in1; outelement=(in0>=0 && in1<0 && temp0element<0) ? ")+
                                 typemax+
                                 std::string(" : ( (in0<0 && in1>0 && temp0element>=0) ? ")+
                                 typemin+
                                 std::string(" : temp0element );"),
                                 fdecl->getParamDecl(0),fdecl->getParamDecl(1),std::string("out"),returntype,pstring);
            pstring += std::string("return(out);\n");
          }else{
            //Unsigned
            unsigned int i;
            pstring += rqtstring + std::string(" out;\n");
            createbinopvectormap(std::string("outelement = ((in0 >= in1) ? (in0 - in1) : 0);"),
                fdecl->getParamDecl(0),fdecl->getParamDecl(1),std::string("out"),returntype,pstring);
            pstring += std::string("return(out);\n");

          }
        }

        //MUL24
        if(fdeclname==std::string("mul24")){
          //arg0 width
          const clang::Type *arg0type = fdecl->getParamDecl(0)->getType().getTypePtr();
          const clang::BuiltinType *arg0basetype = get_builtintype_vectorbasetype(arg0type);
          builtinfile=integer;
          pstring += rqtstring + std::string(" out;\n");
          createbinopvectormap(std::string("outelement=((in0 << 8) >> 8) * ((in1 << 8) >> 8);"),
                fdecl->getParamDecl(0),fdecl->getParamDecl(1),std::string("out"),returntype,pstring);
          pstring += std::string("return(out);\n");
        }

        //MAD24
        if(fdeclname==std::string("mad24")){
          //arg0 width
          const clang::Type *arg0type = fdecl->getParamDecl(0)->getType().getTypePtr();
          const clang::BuiltinType *arg0basetype = get_builtintype_vectorbasetype(arg0type);
          builtinfile=integer;
          pstring += rqtstring + std::string(" out;\n");
          createtriopvectormap(std::string("outelement=(((in0 << 8) >> 8) * ((in1 << 8) >> 8) + in2);"),
                               fdecl->getParamDecl(0),fdecl->getParamDecl(1),fdecl->getParamDecl(2),std::string("out"),pstring);
          pstring += std::string("return(out);\n");
        }

        //MUL_HI
        if(fdeclname==std::string("mul_hi")){
          //arg0 width
          const clang::Type *arg0type = fdecl->getParamDecl(0)->getType().getTypePtr();
          const clang::BuiltinType *arg0basetype = get_builtintype_vectorbasetype(arg0type);
          builtinfile=integer;
          pstring += rqtstring + std::string(" out;\n");
            createbinopvectormap(std::string("outelement=mul_hi_impl(in0,in1);"),
                fdecl->getParamDecl(0),fdecl->getParamDecl(1),std::string("out"),returntype,pstring);
          pstring += std::string("return(out);\n");
        }

        //UPSAMPLE
        if(fdeclname==std::string("upsample")){
          //return type
          const clang::FunctionType *AFT= fdecl->getType()->getAs<clang::FunctionType>();
          clang::QualType rqt=AFT->getResultType();
          const clang::Type *rtp=rqt.getTypePtr();
          returntype=rtp;
          const clang::BuiltinType *returnbasetype = get_builtintype_vectorbasetype(returntype);
          std::string returnbasetypestr;
          get_implementation_vectorbasetype(returntype,returnbasetypestr);
          //arg0 type and size
          const clang::Type *arg0type = fdecl->getParamDecl(0)->getType().getTypePtr();
          const clang::BuiltinType *arg0basetype = get_builtintype_vectorbasetype(arg0type);
          std::string arg0basetypestr;
          get_implementation_vectorbasetype(arg0type,arg0basetypestr);
          int arg0size=get_builtintype_size(arg0basetype);
          std::stringstream arg0sizebits;
          arg0sizebits << (arg0size*8);
          //implementation 
          builtinfile=integer;
          pstring += rqtstring + std::string(" out;\n");
            createbinopvectormap(std::string("outelement=((")+returnbasetypestr+std::string(")in0 << ")+arg0sizebits.str()+std::string(") | in1;"),
                fdecl->getParamDecl(0),fdecl->getParamDecl(1),std::string("out"),returntype,pstring);
          pstring += std::string("return(out);\n");
        }

        //tri operatiorns
        //MADD_HI
        if(fdeclname==std::string("mad_hi")){
          //arg0 width
          const clang::Type *arg0type = fdecl->getParamDecl(0)->getType().getTypePtr();
          const clang::BuiltinType *arg0basetype = get_builtintype_vectorbasetype(arg0type);
          builtinfile=integer;
          pstring += rqtstring + std::string(" out;\n");
            createtriopvectormap(std::string("outelement=mul_hi_impl(in0,in1)+in2;"),
                fdecl->getParamDecl(0),fdecl->getParamDecl(1),fdecl->getParamDecl(2),std::string("out"),pstring);
          pstring += std::string("return(out);\n");
        }

        //MAD_SAT
        if(fdeclname==std::string("mad_sat")){
          //arg0 width
          builtinfile=integer;
          pstring += rqtstring + std::string(" out;\n");
            createtriopvectormap(std::string("outelement=mad_sat_impl(in0,in1,in2);"),
                fdecl->getParamDecl(0),fdecl->getParamDecl(1),fdecl->getParamDecl(2),std::string("out"),pstring);
          pstring += std::string("return(out);\n");
        }


        //CLAMP
        if(fdeclname==std::string("clamp")){
          //integer version
          const clang::Type *arg0type = fdecl->getParamDecl(0)->getType().getTypePtr();
          const clang::BuiltinType *arg0basetype = get_builtintype_vectorbasetype(arg0type);
          if(arg0basetype->isInteger()){
            builtinfile=integer;
            pstring += rqtstring + std::string(" out;\n");
            createtriopvectormap(std::string("temp0element=((in0>in1) ? in0 : in1); outelement = ((temp0element<in2) ? temp0element : in2);"),
                fdecl->getParamDecl(0),fdecl->getParamDecl(1),fdecl->getParamDecl(2),std::string("out"),pstring);
            pstring += std::string("return(out);\n");
          }
          // float version
          if(arg0basetype->isFloatingPoint()){
            builtinfile = commonfns;
            pstring += rqtstring + std::string(" out;\n");
            createtriopvectormap(std::string("temp0element=((in0>in1) ? in0 : in1); outelement = ((temp0element<in2) ? temp0element : in2);"),
                fdecl->getParamDecl(0),fdecl->getParamDecl(1),fdecl->getParamDecl(2),std::string("out"),pstring);
            pstring += std::string("return(out);\n");
					}
        }

        //
        // common functions builtins - step
        {
          const char *unopmathc[] = {"step"};
          const char *unopmathcimpl[] = {"step_impl"};
          std::vector<std::string> unopmath(unopmathc,unopmathc+sizeof(unopmathc)/sizeof(unopmathc[0]));
          std::vector<std::string> unopmathimpl(unopmathcimpl,unopmathcimpl+sizeof(unopmathcimpl)/sizeof(unopmathcimpl[0]));
          std::vector<std::string>::iterator itimpl=unopmathimpl.begin();
          for(std::vector<std::string>::iterator it=unopmath.begin();it!=unopmath.end();it++,itimpl++){
            if(fdeclname==(*it)){
              builtinfile=commonfns;
              pstring += rqtstring + std::string(" out;\n");
              createbinopvectormap2(std::string("outelement=")+(*itimpl)+std::string("(in0,in1);"),fdecl->getParamDecl(0),fdecl->getParamDecl(1),std::string("out"),pstring);
              pstring += std::string("return(out);\n");
            }
          }
        }
        //
        // common functions builtins - degrees,sign
        {
          const char *unopmathc[] = {"degrees","radians","sign"};
          const char *unopmathcimpl[] = {"degrees_impl","radians_impl","sign_impl"};
          std::vector<std::string> unopmath(unopmathc,unopmathc+sizeof(unopmathc)/sizeof(unopmathc[0]));
          std::vector<std::string> unopmathimpl(unopmathcimpl,unopmathcimpl+sizeof(unopmathcimpl)/sizeof(unopmathcimpl[0]));
          std::vector<std::string>::iterator itimpl=unopmathimpl.begin();
          for(std::vector<std::string>::iterator it=unopmath.begin();it!=unopmath.end();it++,itimpl++){
            if(fdeclname==(*it)){
              builtinfile=commonfns;
              pstring += rqtstring + std::string(" out;\n");
              createunopvectormap(std::string("outelement=")+(*itimpl)+std::string("(inelement);"),fdecl->getParamDecl(0),returntype,std::string("out"),pstring);
              pstring += std::string("return(out);\n");
            }
          }
        }

        //
        // common functions builtins - mix, smoothstep
        {
          const char *unopmathc[] = {"mix","smoothstep"};
          const char *unopmathcimpl[] = {"mix_impl","smoothstep_impl"};
          std::vector<std::string> unopmath(unopmathc,unopmathc+sizeof(unopmathc)/sizeof(unopmathc[0]));
          std::vector<std::string> unopmathimpl(unopmathcimpl,unopmathcimpl+sizeof(unopmathcimpl)/sizeof(unopmathcimpl[0]));
          std::vector<std::string>::iterator itimpl=unopmathimpl.begin();
          for(std::vector<std::string>::iterator it=unopmath.begin();it!=unopmath.end();it++,itimpl++){
            if(fdeclname==(*it)){
              builtinfile=commonfns;
              pstring += rqtstring + std::string(" out;\n");
              createtriopvectormap2(std::string("outelement=")+(*itimpl)+std::string("(in0,in1,in2);"),
                  fdecl->getParamDecl(0),fdecl->getParamDecl(1),fdecl->getParamDecl(2),std::string("out"),pstring);
              pstring += std::string("return(out);\n");
            }
          }
        }

				// geometric builtins - dot
        {
          const char *unopmathc[] = {"dot"};
          const char *unopmathcimpl[] = {"dot_impl"};
          std::vector<std::string> unopmath(unopmathc,unopmathc+sizeof(unopmathc)/sizeof(unopmathc[0]));
          std::vector<std::string> unopmathimpl(unopmathcimpl,unopmathcimpl+sizeof(unopmathcimpl)/sizeof(unopmathcimpl[0]));
          std::vector<std::string>::iterator itimpl=unopmathimpl.begin();
          for(std::vector<std::string>::iterator it=unopmath.begin();it!=unopmath.end();it++,itimpl++){
            if(fdeclname==(*it)){
							builtinfile=geometric;
							pstring += rqtstring + std::string(" out;\n");

							//Implementation of the dot function
							const clang::Type *from0type = fdecl->getParamDecl(0)->getType().getTypePtr();
							bool from0vectortype = false;
							if(const clang::VectorType *VT = dyn_cast<clang::VectorType>(from0type->getCanonicalTypeInternal())) from0vectortype=true;
							std::string from0name=fdecl->getParamDecl(0)->getName();
							std::string from1name=fdecl->getParamDecl(1)->getName();

							pstring += std::string("out = ");
							if(from0vectortype == false){
								pstring += from0name + std::string(" * ") + from1name + std::string(";\n");
							}
							else{
								if(const clang::VectorType *BT = dyn_cast<clang::VectorType>(from0type->getCanonicalTypeInternal())){
									for(unsigned int element=0;element<BT->getNumElements();element++){
										std::stringstream conv;
										conv << std::hex << element;
										pstring += std::string("(") + from0name + std::string(".s") + conv.str() + std::string(" * ") +from1name + std::string(".s") + conv.str() + std::string(")");
										if((element + 1) != BT->getNumElements())
											pstring += std::string(" + ");
										else
											pstring += std::string(";\n");
									}
								}
							}
							pstring += std::string("return(out);\n");
						}
					}
				}

				// geometric builtins - length
        {
          const char *unopmathc[] = {"length","fast_length"};
          std::vector<std::string> unopmath(unopmathc,unopmathc+sizeof(unopmathc)/sizeof(unopmathc[0]));
          std::vector<std::string>::iterator itimpl=unopmathimpl.begin();
          for(std::vector<std::string>::iterator it=unopmath.begin();it!=unopmath.end();it++,itimpl++){
            if(fdeclname==(*it)){
							builtinfile=geometric;
							pstring += rqtstring + std::string(" out;\n");
							pstring += std::string("__spir_double_t tempout;\n");

							//Implementation of the dot function
							const clang::Type *from0type = fdecl->getParamDecl(0)->getType().getTypePtr();
							bool from0vectortype = false;
							if(const clang::VectorType *VT = dyn_cast<clang::VectorType>(from0type->getCanonicalTypeInternal())) from0vectortype=true;
							std::string from0name=fdecl->getParamDecl(0)->getName();

							pstring += std::string("tempout = ");
							if(from0vectortype == false){
								pstring += std::string("(double)") + from0name + std::string(" * ") + std::string("(double)") + from0name + std::string(";\n");
							}
							else{
								if(const clang::VectorType *BT = dyn_cast<clang::VectorType>(from0type->getCanonicalTypeInternal())){
									for(unsigned int element=0;element<BT->getNumElements();element++){
										std::stringstream conv;
										conv << std::hex << element;
										pstring += std::string("((double)") + from0name + std::string(".s") + conv.str() + std::string(" * (double)") +from0name + std::string(".s") + conv.str() + std::string(")");
										if((element + 1) != BT->getNumElements())
											pstring += std::string(" + ");
										else
											pstring += std::string(";\n");
									}
								}
							}
							pstring += std::string("out=(float)sqrt(tempout);\n");
							pstring += std::string("return(out);\n");
						}
					}
				}
				// geometric builtins - distance
        {
          const char *unopmathc[] = {"distance","fast_distance"};
          std::vector<std::string> unopmath(unopmathc,unopmathc+sizeof(unopmathc)/sizeof(unopmathc[0]));
          for(std::vector<std::string>::iterator it=unopmath.begin();it!=unopmath.end();it++,itimpl++){
            if(fdeclname==(*it)){
							builtinfile=geometric;
							pstring += rqtstring + std::string(" out;\n");

							//Implementation of the dot function
							const clang::Type *from0type = fdecl->getParamDecl(0)->getType().getTypePtr();
							bool from0vectortype = false;
							if(const clang::VectorType *VT = dyn_cast<clang::VectorType>(from0type->getCanonicalTypeInternal())) from0vectortype=true;
							std::string from0name=fdecl->getParamDecl(0)->getName();
							std::string from1name=fdecl->getParamDecl(1)->getName();

							//param types
							std::string from0vectorbasetypestring;

							bool param0Vector = get_implementation_type(from0type,from0vectorbasetypestring);
							std::string tovectorbasetypestring=from0vectorbasetypestring;


							pstring += from0vectorbasetypestring + std::string(" temp_out;\n");
							pstring += std::string("__spir_double_t sum_result;\n");

							pstring += std::string("temp_out = ");
							pstring += from0name + std::string(" - ") + from1name + std::string(";\n");
							pstring += std::string("sum_result = ");
							if(from0vectortype == false){
								pstring += std::string("(double)temp_out") + std::string(" * ") + std::string("(double)temp_out") + std::string(";\n");
							}
							else{
								if(const clang::VectorType *BT = dyn_cast<clang::VectorType>(from0type->getCanonicalTypeInternal())){
									for(unsigned int element=0;element<BT->getNumElements();element++){
										std::stringstream conv;
										conv << std::hex << element;
										pstring += std::string("((double)") + std::string("temp_out.s") + conv.str() + std::string(" * (double)") + std::string("temp_out.s") + conv.str() + std::string(")");
										if((element + 1) != BT->getNumElements())
											pstring += std::string(" + ");
										else
											pstring += std::string(";\n");
									}
								}
							}
							pstring += std::string("out=(float)sqrt(sum_result);\n");
							pstring += std::string("return(out);\n");
						}
					}
				}
				// geometric builtins - normalize, fast_normalize
        {
          const char *unopmathc[] = {"normalize", "fast_normalize"};
          std::vector<std::string> unopmath(unopmathc,unopmathc+sizeof(unopmathc)/sizeof(unopmathc[0]));
          std::vector<std::string>::iterator itimpl=unopmathimpl.begin();
          for(std::vector<std::string>::iterator it=unopmath.begin();it!=unopmath.end();it++,itimpl++){
            if(fdeclname==(*it)){
							builtinfile=geometric;
							pstring += rqtstring + std::string(" out;\n");
							pstring += std::string("__spir_double_t temp_length;\n");
							pstring += std::string("__spir_double_t temp_square;\n");

							//Implementation of the dot function
							const clang::Type *from0type = fdecl->getParamDecl(0)->getType().getTypePtr();
							bool from0vectortype = false;
							if(const clang::VectorType *VT = dyn_cast<clang::VectorType>(from0type->getCanonicalTypeInternal())) from0vectortype=true;
							std::string from0name=fdecl->getParamDecl(0)->getName();

							pstring += std::string("temp_length = ");
							if(from0vectortype == false){
								pstring += std::string("(double)") + from0name + std::string(" * ") + std::string("(double)") + from0name + std::string(";\n");
							}
							else{
								if(const clang::VectorType *BT = dyn_cast<clang::VectorType>(from0type->getCanonicalTypeInternal())){
									for(unsigned int element=0;element<BT->getNumElements();element++){
										std::stringstream conv;
										conv << std::hex << element;
										pstring += std::string("((double)") + from0name + std::string(".s") + conv.str() + std::string(" * (double)") +from0name + std::string(".s") + conv.str() + std::string(")");
										if((element + 1) != BT->getNumElements())
											pstring += std::string(" + ");
										else
											pstring += std::string(";\n");
									}
								}
							}
							pstring += std::string("temp_square=sqrt(temp_length);\n");
							if(from0vectortype == false){
								pstring += std::string("out = ") + from0name + std::string("/temp_square;\n");
							}
							else{
								if(const clang::VectorType *BT = dyn_cast<clang::VectorType>(from0type->getCanonicalTypeInternal())){
									for(unsigned int element=0;element<BT->getNumElements();element++){
										std::stringstream conv;
										conv << std::hex << element;
										pstring += std::string("out.s") + conv.str() + std::string(" = ") + from0name + std::string(".s") + conv.str() + std::string("/temp_square;\n");
									}
								}
							}
							pstring += std::string("return(out);\n");
						}
					}
				}
				// geometric builtins - cross
        {
          const char *unopmathc[] = {"cross"};
          std::vector<std::string> unopmath(unopmathc,unopmathc+sizeof(unopmathc)/sizeof(unopmathc[0]));
          std::vector<std::string>::iterator itimpl=unopmathimpl.begin();
          for(std::vector<std::string>::iterator it=unopmath.begin();it!=unopmath.end();it++,itimpl++){
            if(fdeclname==(*it)){
							builtinfile=geometric;
							pstring += rqtstring + std::string(" out;\n");

							//Implementation of the cross function
							const clang::Type *from0type = fdecl->getParamDecl(0)->getType().getTypePtr();
							const clang::VectorType *BT = dyn_cast<clang::VectorType>(from0type->getCanonicalTypeInternal());
							unsigned int element_count = BT->getNumElements();
							std::string from0name=fdecl->getParamDecl(0)->getName();
							std::string from1name=fdecl->getParamDecl(1)->getName();
							if(element_count == 4)
								pstring += (" out.s3 = 0.0f;\n");
							pstring += std::string(" out.s0 = ") + from0name + std::string(".s1 * ") + from1name + std::string(".s2 - ") + from0name + std::string(".s2 * ") + from1name + std::string(".s1;\n");
							pstring += std::string(" out.s1 = ") + from0name + std::string(".s2 * ") + from1name + std::string(".s0 - ") + from0name + std::string(".s0 * ") + from1name + std::string(".s2;\n");
							pstring += std::string(" out.s2 = ") + from0name + std::string(".s0 * ") + from1name + std::string(".s1 - ") + from0name + std::string(".s1 * ") + from1name + std::string(".s0;\n");
							pstring += std::string("return(out);\n");
						}
					}
				}

				//
        //relational builtins
        //
        //unary operations
        //ISFINITE, ISINF, ISNAN, ISNORMAL, SIGNBIT
        //unary operations with existing math.h implementations
        //floating point types
        {
          //check for floating point version
          const char *unopmathc[] = {"isfinite","isinf","isnan","isnormal","signbit"};
          const char *unopmathcimpl[] = {"hls_isfinite","hls_isinf","hls_isnan","hls_isnormal","hls_signbit"};
          std::vector<std::string> unopmath(unopmathc,unopmathc+sizeof(unopmathc)/sizeof(unopmathc[0]));
          std::vector<std::string> unopmathimpl(unopmathcimpl,unopmathcimpl+sizeof(unopmathcimpl)/sizeof(unopmathcimpl[0]));
          std::vector<std::string>::iterator itimpl=unopmathimpl.begin();
          for(std::vector<std::string>::iterator it=unopmath.begin();it!=unopmath.end();it++,itimpl++){
            if(fdeclname==(*it)){
              bool floatversion=false;
              if(const clang::VectorType *VT=dyn_cast<clang::VectorType>(fdecl->getParamDecl(0)->getType().getTypePtr()->getCanonicalTypeInternal())){
                if(const clang::BuiltinType *BT=dyn_cast<clang::BuiltinType>(VT->getElementType().getTypePtr()->getCanonicalTypeInternal())){
                  if(BT->getKind()==BuiltinType::Float) floatversion=true;
                }
              }
              if(const clang::BuiltinType *BT=dyn_cast<clang::BuiltinType>(fdecl->getParamDecl(0)->getType().getTypePtr()->getCanonicalTypeInternal())){
                if(BT->getKind()==BuiltinType::Float) floatversion=true;
              }
              if(floatversion){
                builtinfile=relational;
                pstring += rqtstring + std::string(" out;\n");
                if(const clang::VectorType *VT=dyn_cast<clang::VectorType>(fdecl->getParamDecl(0)->getType().getTypePtr()->getCanonicalTypeInternal())){
                  createunopvectormap(std::string("outelement=(")+(*itimpl)+std::string("(inelement)) ? -1 : 0;"),fdecl->getParamDecl(0),returntype,std::string("out"),pstring);
                } else {
                  createunopvectormap(std::string("outelement=")+(*itimpl)+std::string("(inelement);"),fdecl->getParamDecl(0),returntype,std::string("out"),pstring);
                }
                pstring += std::string("return(out);\n");
              }
            }
          }
        }
        //other unary opeartions
        //ANY
        if(fdeclname==std::string("any")){
          builtinfile=relational;
          createunopvectormap_fold(std::string("carry |= msbit(inelement);"),std::string("0"),fdecl->getParamDecl(0),pstring);
          pstring += std::string("return(carry);\n");
        }
        //ALL
        if(fdeclname==std::string("all")){
          builtinfile=relational;
          createunopvectormap_fold(std::string("carry &= msbit(inelement);"),std::string("1"),fdecl->getParamDecl(0),pstring);
          pstring += std::string("return(carry);\n");
        }

        //binary operations
        //
        //simple INFIX operations
        //ISEQUAL, ISNOTEQUAL, ISGREATER, ISGREATEREQUAL, ISLESS, ISLESSEQUAL
        {
          const char *binopmathc[] = {"isequal","isnotequal","isgreater","isgreaterequal","isless","islessequal"};
          const char *binopmathcimpl[] = {"==","!=",">",">=","<","<="};
          std::vector<std::string> binopmath(binopmathc,binopmathc+sizeof(binopmathc)/sizeof(binopmathc[0]));
          std::vector<std::string> binopmathimpl(binopmathcimpl,binopmathcimpl+sizeof(binopmathcimpl)/sizeof(binopmathcimpl[0]));
          std::vector<std::string>::iterator itimpl=binopmathimpl.begin();
          for(std::vector<std::string>::iterator it=binopmath.begin();it!=binopmath.end();it++,itimpl++){
            if(fdeclname==(*it)){
              builtinfile=relational;
              pstring += rqtstring + std::string(" out;\n");
              if(const clang::VectorType *VT=dyn_cast<clang::VectorType>(fdecl->getParamDecl(0)->getType().getTypePtr()->getCanonicalTypeInternal())){
                //Vector version, return -1 on true
                createbinopvectormap(std::string("outelement=(in0")+(*itimpl)+std::string("in1) ? -1 : 0;"),fdecl->getParamDecl(0),fdecl->getParamDecl(1),std::string("out"),returntype,pstring);
              }else{
                //scalar version, return 1 on true
                createbinopvectormap(std::string("outelement=(in0")+(*itimpl)+std::string("in1);"),fdecl->getParamDecl(0),fdecl->getParamDecl(1),std::string("out"),returntype,pstring);
              }
              pstring += std::string("return(out);\n");
            }
          }
        }
        //ISLESSGREATER
        if(fdeclname==std::string("islessgreater")){
          builtinfile=relational;
          pstring += rqtstring + std::string(" out;\n");
          if(const clang::VectorType *VT=dyn_cast<clang::VectorType>(fdecl->getParamDecl(0)->getType().getTypePtr()->getCanonicalTypeInternal())){
            //Vector version, return -1 on true
            createbinopvectormap(std::string("outelement=((!hls_isnan(in0))&&(!hls_isnan(in1))&&((in0<in1)||(in0>in1))) ? -1 : 0;"),fdecl->getParamDecl(0),fdecl->getParamDecl(1),std::string("out"),returntype,pstring);
          }else{
            //scalar version, return 1 on true
            createbinopvectormap(std::string("outelement=(!hls_isnan(in0))&&(!hls_isnan(in1))&&((in0<in1)||(in0>in1));"),fdecl->getParamDecl(0),fdecl->getParamDecl(1),std::string("out"),returntype,pstring);
          }
          pstring += std::string("return(out);\n");
        }
        //ISORDERED
        if(fdeclname==std::string("isordered")){
          builtinfile=relational;
          pstring += rqtstring + std::string(" out;\n");
          if(const clang::VectorType *VT=dyn_cast<clang::VectorType>(fdecl->getParamDecl(0)->getType().getTypePtr()->getCanonicalTypeInternal())){
            //Vector version, return -1 on true
            createbinopvectormap(std::string("outelement=((!hls_isnan(in0))&&(!hls_isnan(in1))&&(in0==in0)&&(in1==in1)) ? -1 : 0;"),fdecl->getParamDecl(0),fdecl->getParamDecl(1),std::string("out"),returntype,pstring);
          }else{
            //scalar version, return 1 on true
            createbinopvectormap(std::string("outelement=((!hls_isnan(in0))&&(!hls_isnan(in1))&&(in0==in0)&&(in1==in1));"),fdecl->getParamDecl(0),fdecl->getParamDecl(1),std::string("out"),returntype,pstring);
          }
          pstring += std::string("return(out);\n");
        }
        //ISUNORDERED
        if(fdeclname==std::string("isunordered")){
          builtinfile=relational;
          pstring += rqtstring + std::string(" out;\n");
          if(const clang::VectorType *VT=dyn_cast<clang::VectorType>(fdecl->getParamDecl(0)->getType().getTypePtr()->getCanonicalTypeInternal())){
            //Vector version, return -1 on true
            createbinopvectormap(std::string("outelement=((hls_isnan(in0) || hls_isnan(in1)) ? -1 : 0);"),fdecl->getParamDecl(0),fdecl->getParamDecl(1),std::string("out"),returntype,pstring);
          }else{
            //scalar version, return 1 on true
            createbinopvectormap(std::string("outelement=(hls_isnan(in0) || hls_isnan(in1));"),fdecl->getParamDecl(0),fdecl->getParamDecl(1),std::string("out"),returntype,pstring);
          }
          pstring += std::string("return(out);\n");
        }







        //
        //tertiary operations
        //SELECT
        if(fdeclname==std::string("select")){
          builtinfile=relational;
          //Vectors
          if(isa<clang::VectorType>(fdecl->getParamDecl(0)->getType().getTypePtr()->getCanonicalTypeInternal())){
            pstring += rqtstring + std::string(" out;\n");
            createtriopvectormap(std::string("outelement=msbit(in2) ? in1 : in0;"),
                fdecl->getParamDecl(0),fdecl->getParamDecl(1),fdecl->getParamDecl(2),std::string("out"),pstring);
            pstring += std::string("return(out);\n");
          }
          //Scalars
          else{
            pstring += rqtstring + std::string(" out;\n");
            createtriopvectormap(std::string("outelement=in2  ? in1 : in0;"),
                fdecl->getParamDecl(0),fdecl->getParamDecl(1),fdecl->getParamDecl(2),std::string("out"),pstring);
            pstring += std::string("return(out);\n");
          }
        }
        //BITSELECT
        if(fdeclname==std::string("bitselect")){
          builtinfile=relational;
          const clang::Type *arg0type = fdecl->getParamDecl(0)->getType().getTypePtr();
          const clang::BuiltinType *arg0basetype = get_builtintype_vectorbasetype(arg0type);
          //float version
          //convert to integer because bitwise not cannot be used for floating point types
          if(arg0basetype->isFloatingPoint()) {
            pstring += rqtstring + std::string(" out;\n");
            createtriopvectormap(std::string("unsigned int in0uint=floatbitcasttouint(in0);  unsigned int in1uint=floatbitcasttouint(in1);  unsigned int in2uint=floatbitcasttouint(in2); outelement=uintbitcasttofloat((in2uint & in1uint) | (~in2uint & in0uint));"),
                fdecl->getParamDecl(0),fdecl->getParamDecl(1),fdecl->getParamDecl(2),std::string("out"),pstring);
            pstring += std::string("return(out);\n");
 
          }else{
            pstring += rqtstring + std::string(" out;\n");
            createtriopvectormap(std::string("outelement=(in2 & in1) | (~in2 & in0);"),
                fdecl->getParamDecl(0),fdecl->getParamDecl(1),fdecl->getParamDecl(2),std::string("out"),pstring);
            pstring += std::string("return(out);\n");
          }
        }





        //vector data load and store functions
        //
        //binary operations
        //
        //VLOAD
        if(fdeclname==std::string("vload2") || fdeclname==std::string("vload3") || fdeclname==std::string("vload4") ||
            fdeclname==std::string("vload8") || fdeclname==std::string("vload16")){
          pstring += "return {";
          if(const clang::VectorType *BT = dyn_cast<clang::VectorType>(fdecl->getResultType().getTypePtr()->getCanonicalTypeInternal())){
            std::stringstream convelements;
            convelements << BT->getNumElements();
            for(unsigned int element=0; element<BT->getNumElements(); element++){
              std::stringstream ss;
              if (element) ss << ", ";
              ss << "g[f*" << BT->getNumElements() << "+" << element <<  "]";
              pstring += ss.str();
            }
          }
          pstring += std::string("};\n");
        }
        //
        //VSTORE
        if(fdeclname==std::string("vstore2") || fdeclname==std::string("vstore3") || fdeclname==std::string("vstore4") ||
            fdeclname==std::string("vstore8") || fdeclname==std::string("vstore16")){
          if(const clang::VectorType *BT = dyn_cast<clang::VectorType>(fdecl->getParamDecl(0)->getType().getTypePtr()->getCanonicalTypeInternal())){
            std::stringstream convelements;
            convelements << BT->getNumElements();
            for(unsigned int element=0;element<BT->getNumElements();element++){
              std::stringstream convhex;
              convhex << std::hex << element;
              std::stringstream conv;
              conv << element;
              pstring += std::string("h[g*") + convelements.str() + std::string("+") + conv.str() + std::string("]=f.s") + convhex.str() + std::string(";\n");
            }
          }
          pstring += std::string("return ;\n");
        }
        //VLOAD_HALF
        if(fdeclname.compare(0, 5, "vload") == 0 &&
           fdeclname.find("_half") != std::string::npos) {
          if(const clang::VectorType *BT = dyn_cast<clang::VectorType>(fdecl->getResultType().getTypePtr()->getCanonicalTypeInternal())){
            std::stringstream convelements;
            convelements << BT->getNumElements();
            pstring += "  return {";
            for(unsigned int element=0; element<BT->getNumElements(); element++){
              std::stringstream ss;
              if (element) ss << ", ";
              ss << "hls_vload_half(p[offset*" << BT->getNumElements() << "+" << element <<  "])";
              pstring += ss.str();
            }
            pstring += std::string("};");
          }
          else {
            pstring += std::string("  return hls_vload_half(p[offset]);");
          }
        }
        //VSTORE_HALF
        if(fdeclname.compare(0, 6, "vstore") == 0 &&
           fdeclname.find("_half") != std::string::npos) {
          //rounding
          bool extraround=false;
          std::string fproundstring;
          if(fdeclname.find(std::string("_rte"))!=std::string::npos) extraround=true, fproundstring=std::string("FE_TONEAREST");
          if(fdeclname.find(std::string("_rtz"))!=std::string::npos) extraround=true, fproundstring=std::string("FE_TOWARDZERO");
          if(fdeclname.find(std::string("_rtp"))!=std::string::npos) extraround=true, fproundstring=std::string("FE_UPWARD");
          if(fdeclname.find(std::string("_rtn"))!=std::string::npos) extraround=true, fproundstring=std::string("FE_DOWNWARD");
          //vector 
          if(const clang::VectorType *BT = dyn_cast<clang::VectorType>(fdecl->getParamDecl(0)->getType().getTypePtr()->getCanonicalTypeInternal())){
            std::stringstream convelements;
            convelements << BT->getNumElements();
            for(unsigned int element=0;element<BT->getNumElements();element++){
              std::stringstream convhex;
              convhex << std::hex << element;
              std::stringstream conv;
              conv << element;
              if(extraround) pstring += std::string("  p[offset*") + convelements.str() + std::string("+") + conv.str() + std::string("]=hls_vstore_half_with_rounding(data.s") + convhex.str() + std::string(",")+fproundstring+std::string(");\n");
              else pstring += std::string("  p[offset*") + convelements.str() + std::string("+") + conv.str() + std::string("]=hls_vstore_half(data.s") + convhex.str() + std::string(");\n");
            }
          }
          //scalar
          else {
            if(extraround) pstring += std::string("  p[offset]=hls_vstore_half_with_rounding(data,")+fproundstring+std::string(");\n");
            else pstring += std::string("  p[offset]=hls_vstore_half(data);\n");

          }
        }
 




        //
        //async copy builtins
        //
        //ASYNC_WORK_GROUP_COPY
        if(fdeclname==std::string("async_work_group_copy")){
          //memcpy(f,g,8*sizeof(short)*h);
          builtinfile=async;
          pstring += std::string("#ifdef __SYNTHESIS__\n");
          pstring += std::string("  __attribute__((xcl_single_workitem))\n");
          pstring += std::string("#else\n");
          pstring += std::string("  if (__builtin_Z12get_local_idj(0)==0 && __builtin_Z12get_local_idj(1)==0 && __builtin_Z12get_local_idj(2)==0)\n");
          pstring += std::string("#endif\n");
          const clang::Type *ty = fdecl->getParamDecl(0)->getType().getTypePtr();
          const clang::PointerType *pty = dyn_cast<clang::PointerType>(ty);
          unsigned addr_space = pty->getPointeeType().getAddressSpace();

          pstring += "    {\n";
          //pstring += std::string("#ifdef __SYNTHESIS__\n");
          if (addr_space == clang::LangAS::opencl_local)
            pstring += "    _ssdm_xcl_PointerMap(g,0,0,";
          else
            pstring += "    _ssdm_xcl_PointerMap(f,0,0,";

          {
            std::string temp1,temp2;
            bool success=get_implementation_vectorbasetype(fdecl->getParamDecl(0)->getOriginalType().getTypePtr(),temp1);
            if(!success){
              std::cout << "error\n"; exit(1);
            }
            success=get_implementation_vectornumelements(fdecl->getParamDecl(0)->getOriginalType().getTypePtr(),temp2);
            if(!success){
              std::cout << "error\n"; exit(1);
            }
            pstring += temp2 + "*sizeof(" + temp1 + "));\n";
            //pstring += std::string("#endif\n");
            pstring += std::string("    __builtin_memcpy((void*)f,(void*)g,");
            if (atoi(temp2.c_str()) == 3) temp2 = "4";
            pstring += temp2 + std::string("*sizeof(") + temp1 + std::string(")*h);\n");

        }
        pstring += "    }\n";
          pstring += std::string("  return 0;\n");
        }
        //ASYNC_WORK_GROUP_STRIDED_COPY
        if(fdeclname==std::string("async_work_group_strided_copy")){
          //There are two variants of async_work_group_strided_copy
          //(1)copy from global to local with source stride
          //(2)copy from local to global with dest stride
          builtinfile=async;
          const clang::Type *param0type=fdecl->getParamDecl(0)->getType().getTypePtr();
          const clang::PointerType *param0pttype = dyn_cast<clang::PointerType>(param0type);
          unsigned param0addressspace=param0pttype->getPointeeType().getAddressSpace();
          //std::cout << "async_work_group_strided_copy address space " << param0addressspace << "\n";

          if(param0addressspace==clang::LangAS::opencl_local){
            //async_work_group_strided_copy(_local gentype *dst,const __global gentype *src, size_t num_gentypes, size_t src_stride, event_t event)
            //(1)copy from global to local with source stride
            //std::cout << "async_work_group_strided_copy address space local\n";
            std::string param0typestring;
            bool success=get_implementation_type(fdecl->getParamDecl(0)->getType().getTypePtr(),param0typestring);
            if(!success){
              std::cout << "error\n"; exit(1);
            }
            std::string param1typestring;
            success=get_implementation_type(fdecl->getParamDecl(1)->getType().getTypePtr(),param1typestring);
            if(!success){
              std::cout << "error\n"; exit(1);
            }
            pstring += std::string("#ifdef __SYNTHESIS__\n");
            pstring += std::string("  __attribute__((xcl_single_workitem)) {\n");
            pstring += std::string("#else\n");
            pstring += std::string("  if (__builtin_Z12get_local_idj(0)==0 && __builtin_Z12get_local_idj(1)==0 && __builtin_Z12get_local_idj(2)==0) {\n");
            pstring += std::string("#endif\n");
            pstring += std::string("    ")+param0typestring+std::string(" it0 = f;\n");
            pstring += std::string("    ")+param1typestring+std::string(" it1 = g;\n");
            pstring += std::string("    __spir_size_t loop;\n");
            pstring += std::string("    for(loop=0; loop<h; loop++){\n");
            pstring += std::string("      (*it0) = (*it1);\n");
            pstring += std::string("      it0++;\n");
            pstring += std::string("      it1 += i;\n");
            pstring += std::string("    }\n");
            pstring += std::string("  }\n");
            pstring += std::string("  return 0;\n");

          }
          else{
            //async_work_group_strided_copy(_global gentype *dst,const __local gentype *src, size_t num_gentypes, size_t dst_stride, event_t event)
            //(2)copy from local to global with dest stride
            //std::cout << "async_work_group_strided_copy address space global\n";
            std::string param0typestring;
            bool success=get_implementation_type(fdecl->getParamDecl(0)->getType().getTypePtr(),param0typestring);
            if(!success){
              std::cout << "error\n"; exit(1);
            }
            std::string param1typestring;
            success=get_implementation_type(fdecl->getParamDecl(1)->getType().getTypePtr(),param1typestring);
            if(!success){
              std::cout << "error\n"; exit(1);
            }
            pstring += std::string("#ifdef __SYNTHESIS__\n");
            pstring += std::string("  __attribute__((xcl_single_workitem)) {\n");
            pstring += std::string("#else\n");
            pstring += std::string("  if (__builtin_Z12get_local_idj(0)==0 && __builtin_Z12get_local_idj(1)==0 && __builtin_Z12get_local_idj(2)==0) {\n");
            pstring += std::string("#endif\n");
            pstring += std::string("    ")+param0typestring+std::string(" it0 = f;\n");
            pstring += std::string("    ")+param1typestring+std::string(" it1 = g;\n");
            pstring += std::string("    __spir_size_t loop;\n");
            pstring += std::string("    for (loop=0; loop<h; loop++){\n");
            pstring += std::string("      (*it0) = (*it1);\n");
            pstring += std::string("      it0 += i;\n");
            pstring += std::string("      it1++;\n");
            pstring += std::string("    }\n");
            pstring += std::string("  }\n");
            pstring += std::string("  return 0;\n");

          }
        }
        //PREFETCH
        if(fdeclname==std::string("prefetch")){
          builtinfile=async;
        }
        // wait_group_events
        if (fdeclname == std::string("wait_group_events")) {
          builtinfile=async;
          pstring += std::string("#ifdef __SYNTHESIS__\n");
          pstring += std::string("#else\n");
          pstring += std::string("  __builtin_Z7barrierj(0);\n");
          pstring += std::string("#endif\n");
        }

        if (fdeclname == std::string("reserve_read_pipe")) {
          pstring += "  return cpu_reserve_read_pipe((void*)p, num_packets);";
        }
        else if (fdeclname == std::string("reserve_write_pipe")) {
          pstring += "  return cpu_reserve_write_pipe((void*)p, num_packets);";
        }
        else if (fdeclname == std::string("commit_read_pipe")) {
          pstring += "  cpu_commit_read_pipe((void*)p, (void*)id);";
        }
        else if (fdeclname == std::string("commit_write_pipe")) {
          pstring += "  cpu_commit_write_pipe((void*)p, (void*)id);";
        }
        else if (fdeclname == std::string("read_pipe")) {
          if (fdecl->getNumParams() == 2) {
            // non-reserve based
            std::string elemtype;
            const clang::Type *Ty = fdecl->getParamDecl(0)->getType().getTypePtr();
            const clang::OpenCLPipeType *PT = cast<clang::OpenCLPipeType>(Ty);
            get_implementation_type(PT->getElementType().getTypePtr(), elemtype);
            pstring += "  "+elemtype+" tmp;\n";
            pstring += "  bool empty_n = _ssdm_StreamNbRead((void*)p, &tmp);\n";
            pstring += "  *e = tmp;\n";
            pstring += "  return empty_n ? 0 : -1;\n";
          }
          else {
            pstring += "  return cpu_read_pipe_reserve((void*)p, id, index, (void*)e);";
          }
        }
        else if (fdeclname == std::string("write_pipe")) {
          if (fdecl->getNumParams() == 2) {
            // non-reserve based
            std::string elemtype;
            const clang::Type *Ty = fdecl->getParamDecl(0)->getType().getTypePtr();
            const clang::OpenCLPipeType *PT = cast<clang::OpenCLPipeType>(Ty);
            get_implementation_type(PT->getElementType().getTypePtr(), elemtype);
            pstring += "  "+elemtype+" tmp = *e;\n";
            pstring += "  bool full_n = _ssdm_StreamNbWrite((void*)p, &tmp);\n";
            pstring += "  return full_n ? 0 : -1;\n";
          }
          else {
            // reserve based
            pstring += "  return cpu_write_pipe_reserve((void*)p, id, index, (void*)e);";
          }
        }
        else if (fdeclname == std::string("work_group_read_pipe")) {
          pstring += "  return cpu_work_group_read_pipe_reserve((void*)p,id,num_packets,(void*)e);\n";
        }
        else if (fdeclname == std::string("work_group_write_pipe")) {
          pstring += "  return cpu_work_group_write_pipe_reserve((void*)p,id,num_packets,(void*)e);\n";
        }
        else if (fdeclname == std::string("work_group_reserve_read_pipe")) {
          pstring += "  __attribute__((address_space(16776961))) __spir_size_t r;\n";
          pstring += "  if (__builtin_Z12get_local_idj(0)==0 && __builtin_Z12get_local_idj(1)==0 && __builtin_Z12get_local_idj(2)==0) {\n";
          pstring += "    r = (__spir_size_t)cpu_work_group_reserve_read_pipe((void*)p,num_packets);\n";
          pstring += "  }\n";
          pstring += "  __builtin_Z7barrierj(0);\n";
          pstring += "  return (void*)r;\n";
        }
        else if (fdeclname == std::string("work_group_reserve_write_pipe")) {
          pstring += "  __attribute__((address_space(16776961))) __spir_size_t r;\n";
          pstring += "  if (__builtin_Z12get_local_idj(0)==0 && __builtin_Z12get_local_idj(1)==0 && __builtin_Z12get_local_idj(2)==0) {\n";
          pstring += "    r = (__spir_size_t)cpu_work_group_reserve_write_pipe((void*)p,num_packets);\n";
          pstring += "  }\n";
          pstring += "  __builtin_Z7barrierj(0);\n";
          pstring += "  return (void*)r;\n";
        }
        else if (fdeclname == std::string("work_group_commit_read_pipe")) {
          pstring += "  if (__builtin_Z12get_local_idj(0)==0 && __builtin_Z12get_local_idj(1)==0 && __builtin_Z12get_local_idj(2)==0)\n";
          pstring += "    cpu_work_group_commit_read_pipe((void*)p,(void*)id);\n";
          pstring += "  __builtin_Z7barrierj(0);\n";
        }
        else if (fdeclname == std::string("work_group_commit_write_pipe")) {
          pstring += "  if (__builtin_Z12get_local_idj(0)==0 && __builtin_Z12get_local_idj(1)==0 && __builtin_Z12get_local_idj(2)==0)\n";
          pstring += "    cpu_work_group_commit_write_pipe((void*)p,(void*)id);\n";
          pstring += "  __builtin_Z7barrierj(0);\n";
        }
        else if (fdeclname == std::string("get_pipe_num_packets")) {
          pstring += "  return cpu_get_pipe_num_packets((void*)p);\n";
        }
        else if (fdeclname == std::string("get_pipe_max_packets")) {
          pstring += "  return cpu_get_pipe_max_packets((void*)p);\n";
        }
      }


      pstring+=std::string("\n");
      pstring+=std::string("}\n");

      switch(builtinfile){
        default :
          assert(0 && "invalid builtinfile enum");
          break;
        case conversions :
          outstring_conversions[builtinfilesplit] += "__attribute__((always_inline))\n" + rqtstring + " " + nstring + pstring + "\n";
          outstring_conversions[builtinfilesplit] += "\n";
          break;
        case native :
          outstring_native += "__attribute__((always_inline))\n" + rqtstring + " " + nstring + pstring + "\n";
          outstring_native += "\n";
          break;
        case math :
          outstring_math += "__attribute__((always_inline))\n" + rqtstring + " " + nstring + pstring + "\n";
          outstring_math += "\n";
          break;
        case commonfns :
          outstring_commonfns += "__attribute__((always_inline))\n" + rqtstring + " " + nstring + pstring + "\n";
          outstring_commonfns += "\n";
          break;
        case geometric :
          outstring_geometric += "__attribute__((always_inline))\n" + rqtstring + " " + nstring + pstring + "\n";
          outstring_geometric += "\n";
          break;
        case relational :
          outstring_relational += "__attribute__((always_inline))\n" + rqtstring + " " + nstring + pstring + "\n";
          outstring_relational += "\n";
          break;
        case integer :
          outstring_integer += "__attribute__((always_inline))\n" + rqtstring + " " + nstring + pstring + "\n";
          outstring_integer += "\n";
          break;
      case async:
          outstring_async += "__attribute__((always_inline))\n" + rqtstring + " " + nstring + pstring + "\n";
          outstring_async += "\n";
        case other :
          outstring += "__attribute__((always_inline))\n" + rqtstring + " " + nstring + pstring + "\n";
          outstring += "\n";
          break;
      }

    }
    it++;
  }
  return true;
}


std::string &ClcImplementationASTConsumer::getoutstring(){
  return outstring;
}

std::string &ClcImplementationASTConsumer::getoutstring_math(){
  return outstring_math;
}

std::string &ClcImplementationASTConsumer::getoutstring_native(){
  return outstring_native;
}

std::string &ClcImplementationASTConsumer::getoutstring_relational(){
  return outstring_relational;
}

std::vector<std::string> &ClcImplementationASTConsumer::getoutstring_conversions(){
  return outstring_conversions;
}

std::string &ClcImplementationASTConsumer::getoutstring_integer(){
  return outstring_integer;
}

std::string &ClcImplementationASTConsumer::getoutstring_commonfns(){
  return outstring_commonfns;
}

std::string &ClcImplementationASTConsumer::getoutstring_geometric(){
  return outstring_geometric;
}

std::string &ClcImplementationASTConsumer::getoutstring_async(){
  return outstring_async;
}

