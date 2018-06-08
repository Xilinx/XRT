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

#ifndef __XILINX_GENERATECLC_H
#define __XILINX_GENERATECLC_H

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

//JIT
#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/JIT.h"
#include "llvm/ExecutionEngine/GenericValue.h"

//Xilinx OpenCL
#include <CL/opencl.h>
#include <stdlib.h>
#include <stdio.h>

#include <time.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>


class ClcImplementationASTConsumer : public clang::ASTConsumer{
  private:
    //rewriter *rewriter;
    MangleContext *manglecontext;
    Preprocessor *preprocessor;
    Sema *sema;
    std::string outstring;
    std::string outstring_math;
    std::string outstring_native;
    std::string outstring_relational;
    std::vector<std::string> outstring_conversions;
    //std::string outstring_item;
    std::string outstring_integer;
    std::string outstring_commonfns;
    //std::string outstring_common;
    std::string outstring_geometric;
    //std::string outstring_vectordata;
    //std::string outstring_sync;
    std::string outstring_async;
    int outstring_split;
    
  public:
    ClcImplementationASTConsumer(/*(rewriter *r,*/MangleContext *m,clang::Preprocessor *pre);
    ~ClcImplementationASTConsumer();
    void setSema(Sema *sem);
    virtual bool HandleTopLevelDecl(DeclGroupRef d);
    std::string &getoutstring();
    std::string &getoutstring_math();
    std::string &getoutstring_native();
    std::string &getoutstring_relational();
    std::vector<std::string> &getoutstring_conversions();
    std::string &getoutstring_integer();
    std::string &getoutstring_commonfns();
    std::string &getoutstring_geometric();
    std::string &getoutstring_async();

};

#endif



