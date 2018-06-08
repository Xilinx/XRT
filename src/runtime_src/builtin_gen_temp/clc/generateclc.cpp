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

/*
   Generate IntrinsicsSPIR.td fragment
   Arguement 3 of Intrinsic not supported
    Will generate IntrNoMem for everthing where eg. get_work_dim-> IntrReadArgMem is more appropriate
*/


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


#ifndef DEBUG_NOLLVM
using namespace clang;
using namespace clang::driver;
#endif

#include "generateclc.h"

std::string stringpreamble(){
  std::string outstring;
  outstring = "//WARNING : Entire file generated by Xilinx (generateclc). Do not hand edit\n";
  outstring += "\n";
  outstring += "// Copyright 2011 – 2011 Xilinx, Inc. All rights reserved.\n";
  outstring += "//\n";
  outstring += "// This file contains confidential and proprietary information\n";
  outstring += "// of Xilinx, Inc. and is protected under U.S. and\n";
  outstring += "// international copyright and other intellectual property\n";
  outstring += "// laws.\n";
  outstring += "//\n";
  outstring += "// DISCLAIMER\n";
  outstring += "// This disclaimer is not a license and does not grant any\n";
  outstring += "// rights to the materials distributed herewith. Except as\n";
  outstring += "// otherwise provided in a valid license issued to you by\n";
  outstring += "// Xilinx, and to the maximum extent permitted by applicable\n";
  outstring += "// law: (1) THESE MATERIALS ARE MADE AVAILABLE \"AS IS\" AND\n";
  outstring += "// WITH ALL FAULTS, AND XILINX HEREBY DISCLAIMS ALL WARRANTIES\n";
  outstring += "// AND CONDITIONS, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING\n";
  outstring += "// BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, NON-\n";
  outstring += "// INFRINGEMENT, OR FITNESS FOR ANY PARTICULAR PURPOSE; and\n";
  outstring += "// (2) Xilinx shall not be liable (whether in contract or tort,\n";
  outstring += "// including negligence, or under any other theory of\n";
  outstring += "// liability) for any loss or damage of any kind or nature\n";
  outstring += "// related to, arising under or in connection with these\n";
  outstring += "// materials, including for any direct, or any indirect,\n";
  outstring += "// special, incidental, or consequential loss or damage\n";
  outstring += "// (including loss of data, profits, goodwill, or any type of\n";
  outstring += "// loss or damage suffered as a result of any action brought\n";
  outstring += "// by a third party) even if such damage or loss was\n";
  outstring += "// reasonably foreseeable or Xilinx had been advised of the\n";
  outstring += "// possibility of the same.\n";
  outstring += "//\n";
  outstring += "// CRITICAL APPLICATIONS\n";
  outstring += "// Xilinx products are not designed or intended to be fail-\n";
  outstring += "// safe, or for use in any application requiring fail-safe\n";
  outstring += "// performance, such as life-support or safety devices or\n";
  outstring += "// systems, Class III medical devices, nuclear facilities,\n";
  outstring += "// applications related to the deployment of airbags, or any\n";
  outstring += "// other applications that could lead to death, personal\n";
  outstring += "// injury, or severe property or environmental damage\n";
  outstring += "// (individually and collectively, \"Critical\n";
  outstring += "// Applications\"). Customer assumes the sole risk and\n";
  outstring += "// liability of any use of Xilinx products in Critical\n";
  outstring += "// Applications, subject only to applicable laws and\n";
  outstring += "// regulations governing limitations on product liability.\n";
  outstring += "//\n";
  outstring += "// THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS\n";
  outstring += "// PART OF THIS FILE AT ALL TIMES.\n";
  return(outstring);
}


//print identifiertable
void printIdentifierTable(IdentifierTable &i){
  IdentifierTable::iterator it=i.begin();
  while(it!=i.end()){
    std::cout << (*it).second->getName().str() << " " << (*it).second->getBuiltinID() << "\n";
    it++;
  }
}


//-----------------------------------------------------------------------------------------------------------------
//Generate clang BuiltinsSPIR.def
//create parameter type name mangling as defined in Builtins.def

#if 0
std::string deftype(const clang::Type *t){
  std::stringstream conv;
  const clang::BuiltinType *BT = dyn_cast<clang::BuiltinType>(t->getCanonicalTypeInternal());
  if(isa<BuiltinType>(t->getCanonicalTypeInternal())){
    switch(BT->getKind()){
      case BuiltinType::Void:                   return(std::string("v")); break;
      case BuiltinType::Bool:                   return(std::string("b")); break;
      case BuiltinType::Char_S:                 return(std::string("c")); break;
      case BuiltinType::Char_U:                 return(std::string("c")); break;
      case BuiltinType::SChar:                  return(std::string("c")); break;
      case BuiltinType::Short:                  return(std::string("s")); break;
      case BuiltinType::Int:                    return(std::string("i")); break;
      case BuiltinType::Long:                   return(std::string("Li")); break;
      case BuiltinType::LongLong:               return(std::string("LLi")); break;
      case BuiltinType::Int128:                 return(std::string("LLLi")); break;
      case BuiltinType::UChar:                  return(std::string("Uc")); break;
      case BuiltinType::UShort:                 return(std::string("Us")); break;
      case BuiltinType::UInt:                   return(std::string("Ui")); break;
      case BuiltinType::ULong:                  return(std::string("ULi")); break;
      case BuiltinType::ULongLong:              return(std::string("ULLi")); break;
      case BuiltinType::UInt128:                return(std::string("ULLLi")); break;
      case BuiltinType::Half:                   return(std::string("f")); break; //builtin halfs?
      case BuiltinType::Float:                  return(std::string("f")); break;
      case BuiltinType::Double:                 return(std::string("d")); break;
      case BuiltinType::LongDouble:             return(std::string("Ld")); break;
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
      case BuiltinType::ObjCSel:                std::cout << "error\n"; exit(1); break;

    }
  }
  return(std::string(""));
}

std::string defqualtype(const clang::QualType &q){
  std::stringstream conv;
  const clang::Type* t=q.getTypePtr();
  std::string deftypet=deftype(t);
  if(deftypet!=std::string("")){
    return deftypet;
  }
  std::string str=q.getAsString();
  //vector
  if(const clang::VectorType *VT = dyn_cast<clang::VectorType>(t->getCanonicalTypeInternal())){
    conv << VT->getNumElements();
    return(std::string("V")+conv.str()+deftype(VT->getElementType().getTypePtr()));
  }
  //pointer
  if(const clang::PointerType *PT = dyn_cast<clang::PointerType>(t->getCanonicalTypeInternal())){
    std::string addressspacestring("");
    QualType QT=PT->getPointeeType();
    if(QT.getAddressSpace()!=0) conv << QT.getAddressSpace();
    return(deftype(PT->getPointeeType().getTypePtr())+std::string("*")+conv.str());
  }
  return(std::string("error"));
}

#endif

bool deftype(const clang::Type *t,std::string &converted){
  std::stringstream conv;
  //builtin type (non vector or pointer)
  const clang::BuiltinType *BT = dyn_cast<clang::BuiltinType>(t->getCanonicalTypeInternal());
  if(isa<BuiltinType>(t->getCanonicalTypeInternal())){
    switch(BT->getKind()){
      case BuiltinType::Void:                   converted=(std::string("v")); return true;break;
      case BuiltinType::Bool:                   converted=(std::string("b")); return true;break;
      case BuiltinType::Char_S:                 converted=(std::string("c")); return true;break;
      case BuiltinType::Char_U:                 converted=(std::string("c")); return true;break;
      case BuiltinType::SChar:                  converted=(std::string("c")); return true;break;
      case BuiltinType::Short:                  converted=(std::string("s")); return true;break;
      case BuiltinType::Int:                    converted=(std::string("i")); return true;break;
      case BuiltinType::Long:                   converted=(std::string("Li")); return true;break;
      case BuiltinType::LongLong:               converted=(std::string("LLi")); return true;break;
      case BuiltinType::Int128:                 converted=(std::string("LLLi")); return true;break;
      case BuiltinType::UChar:                  converted=(std::string("Uc")); return true;break;
      case BuiltinType::UShort:                 converted=(std::string("Us")); return true;break;
      case BuiltinType::UInt:                   converted=(std::string("Ui")); return true;break;
      case BuiltinType::ULong:                  converted=(std::string("ULi")); return true;break;
      case BuiltinType::ULongLong:              converted=(std::string("ULLi")); return true;break;
      case BuiltinType::UInt128:                converted=(std::string("ULLLi")); return true;break;
      case BuiltinType::Half:                   converted=(std::string("f")); return true;break; //builtin halfs?
      case BuiltinType::Float:                  converted=(std::string("f")); return true;break;
      case BuiltinType::Double:                 converted=(std::string("d")); return true;break;
      case BuiltinType::LongDouble:             converted=(std::string("Ld")); return true;break;

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
    std::string deftypet;
    conv << VT->getNumElements();
    typesuccess=deftype(VT->getElementType().getTypePtr(),deftypet);
    converted=(std::string("V")+conv.str()+deftypet);
    return true;
  }
  //pointer
  if(const clang::PointerType *PT = dyn_cast<clang::PointerType>(t->getCanonicalTypeInternal())){
    bool typesuccess;
    std::string deftypet;
    std::string postfixq;
    std::string addressspacestring("");
    QualType QT=PT->getPointeeType();
    //first postfix with const.. turns out order matters to TableGen here..
    if(QT.isConstQualified()) postfixq+=std::string("C");
    if(QT.isVolatileQualified()) postfixq+=std::string("V");
    if(QT.getAddressSpace()!=0) conv << QT.getAddressSpace();
    typesuccess=deftype(PT->getPointeeType().getTypePtr(),deftypet);
    converted=(deftypet+postfixq+std::string("*")+conv.str());
    //!in TypePrinter note : recursive call in printPointer!
   return true;
  }

  return(false);
}

std::string defqualtype(const clang::QualType &q){
  std::stringstream conv;
  const clang::Type* t=q.getTypePtr();
  std::string deftypet;
  bool typesuccess=deftype(t,deftypet);
  if(typesuccess){
    Qualifiers quals=q.split().Quals;
    //std::cout << "begin\n";
    t->dump();

    //std::cout << "quals.getAsString()=" << quals.getAsString() << "\n";
    if(!quals.empty()){
      //std::cout << "quals nonempty";
    }
    unsigned cvr=quals.getCVRQualifiers();
    //std::cout << "quals.getCVRQualifiered()=" << cvr << "\n";


    Qualifiers quals2=t->getCanonicalTypeInternal().split().Quals;
    //std::cout << quals2.getAsString() << "\n";


    if(deftypet=="") return ("");
    if(q.isConstQualified()) deftypet+=std::string("C");
    if(q.isVolatileQualified()) deftypet+=std::string("V");
    else return(deftypet);
  }
  std::string str=q.getAsString();
 return(std::string("error"));
}



//Generate SPIRBuiltins.def

class ClcDefASTConsumer : public clang::ASTConsumer{
  private:
    MangleContext *manglecontext;
    std::string outstring;
  public:
    ClcDefASTConsumer(MangleContext *m){
      manglecontext=m;
      outstring = stringpreamble();
    }

    /*
        virtual void HandleTranslationUnit(clang::ASTContext &Ctx){
        printf("ClcASTConsumer\n");
        }
     */

    virtual bool HandleTopLevelDecl(DeclGroupRef D){
      //printf("declgroupref\n");
      DeclGroupRef::iterator it=D.begin();
      while(it!=D.end()){
        if(clang::FunctionDecl *fdecl=dyn_cast<clang::FunctionDecl>(*it)){

          std::string fdeclname =fdecl->getNameInfo().getName().getAsString();

          std::string mangle_raw_string;
          llvm::raw_string_ostream mangle_raw_ostream(mangle_raw_string);
          manglecontext->mangleName(fdecl,mangle_raw_ostream);
          std::string defline;
          //Field 1 function name
          defline +=std::string("BUILTIN(__builtin");
          //defline +=fdecl->getNameInfo().getName().getAsString();
          defline += mangle_raw_ostream.str();
          defline += std::string(",\"");
          //Field 2 return type
          {
            const clang::QualType &q=fdecl->getResultType();
            const clang::Type* t=q.getTypePtr();
            defline += defqualtype(q);
          }
          //Field 3 parameters
          clang::FunctionDecl::param_iterator paramit=fdecl->param_begin();
          while(paramit!=fdecl->param_end()){
            const clang::QualType &q=(*paramit)->getOriginalType();
            const clang::Type* t=q.getTypePtr();
            defline += defqualtype(q);
            paramit++;
          }

          defline += std::string("\",\"nc\")");
          outstring += defline + std::string("\n");
          //std::cout << defline << "\n";
        }

        it++;
      }
      return true;
    }

    std::string &getoutstring(){
      return outstring;
    }
};

//-----------------------------------------------------------------------------------------------------------------
//generate clc.h header
//call the itanium mangled clang builtin from builtin header stub

class ClcHeaderASTConsumer : public clang::ASTConsumer{
  private:
    //rewriter *rewriter;
    MangleContext *manglecontext;
    Preprocessor *preprocessor;
    Sema *sema;
    std::string outstring;
  public:

    ClcHeaderASTConsumer(/*(rewriter *r,*/MangleContext *m,clang::Preprocessor *pre){
        //rewriter=r;
        manglecontext=m;
        preprocessor=pre;
        outstring = stringpreamble();
    }

    ~ClcHeaderASTConsumer(){
    }

    void setSema(Sema *sem){
      sema=sem;
    }

    virtual bool HandleTopLevelDecl(DeclGroupRef d){
      //printf("declgroupref\n");
      DeclGroupRef::iterator it=d.begin();
      while(it!=d.end()){
        if(clang::FunctionDecl *fdecl=dyn_cast<clang::FunctionDecl>(*it)){

          //In header, access to builtins takes two forms
          //(a) For non overloaded functions generate a preprocessor define to the mangled builtin
          //(b) For overloaded functions generate a stub for each overload which calls the mangled builtling

          if(!fdecl->hasAttr<OverloadableAttr>()){
            //non overloaded
            outstring += "#define ";
            outstring += fdecl->getNameInfo().getAsString();
            outstring += " ";
            std::string mangle_raw_string;
            llvm::raw_string_ostream mangle_raw_ostream(mangle_raw_string);
            manglecontext->mangleName(fdecl,mangle_raw_ostream);
            outstring += std::string("__builtin")+mangle_raw_ostream.str();
            outstring += "\n";
          }
          else{
            //overloaded

            //fdecl->print(llvm::outs());
            Scope *functionscope=sema->getScopeForContext(fdecl);

            //form of input stub should be
            //uchar8 __attribute__ ((always_inline)) __attribute__((overloadable)) abs (char8 x){return 0;}
            //map to call to builtin
            ASTContext *context=&(fdecl->getASTContext());

            clang::Stmt *body=fdecl->getBody();

            if (fdecl->getNameInfo().getAsString() == "async_work_group_copy") {
              std::string temp;
              llvm::raw_string_ostream outstring_ostream(temp);
              fdecl->print(outstring_ostream);
              outstring += outstring_ostream.str();
              outstring += "{\n";
              outstring += "  if (get_local_id(0)==0 && get_local_id(1)==0 && get_local_id(2)==0)\n";
              outstring += "    memcpy((void *)f,(void *)g,1*sizeof(*f)*h);\n";
              outstring += "  return i;\n";
              outstring += "}\n";
              it++;
              continue;
            }

            //lazily create builtin functiondecl
            std::string mangle_raw_string;
            llvm::raw_string_ostream mangle_raw_ostream(mangle_raw_string);
            manglecontext->mangleName(fdecl,mangle_raw_ostream);

            //std::cout << "requesting buitlin name " << std::string("__builtin")+mangle_raw_ostream.str() << "\n";

            IdentifierInfo *newbuiltinii = preprocessor->getIdentifierInfo(std::string("__builtin")+mangle_raw_ostream.str());
            unsigned int newbuiltinid=newbuiltinii->getBuiltinID();
            if(newbuiltinid==0){
              std::cout << "cannot find builtin of name " << newbuiltinii->getName().str() << "\n";
              exit(1);
            }

            //std::cout << "builtinid " << newbuiltinid << "\n";

            //printidentifiertable(context->idents);

            //get functiondecl for builtin
            FunctionDecl *newbuiltindecl =cast<FunctionDecl>(sema->LazilyCreateBuiltin(newbuiltinii,newbuiltinid,
                  functionscope, false, fdecl->getLocStart()));

            if(newbuiltindecl==NULL){
              std::cout << "missing builtin\n";
              exit(0);
            }

            //create declrefexpr to builtin
            //sourcelocation bodyloc=body->getlocstart();
            DeclRefExpr *declrefexpr=new (context) DeclRefExpr(newbuiltindecl,false,newbuiltindecl->getType(),VK_RValue,SourceLocation());

            //std::vector<declrefexpr *> paramexpr;
            Expr *paramexpr[fdecl->getNumParams()];
            //arguement expressions
            {
              clang::FunctionDecl::param_iterator paramit=fdecl->param_begin();
              unsigned int i=0;
              while(paramit!=fdecl->param_end()){
                DeclRefExpr *declreftemp=new (context) DeclRefExpr(*paramit,false,(*paramit)->getType(),VK_RValue,SourceLocation());
                paramexpr[i]=declreftemp;
                i++;
                paramit++;
              }

            }

            clang::CallExpr *callexpr=new (context) CallExpr(fdecl->getASTContext(),
                declrefexpr,
                paramexpr,
                fdecl->getNumParams(),
                fdecl->getResultType(),
                VK_RValue,
                SourceLocation());

            clang::Stmt *bodystmt;
            //if fdecl returns void then create a CallStmt
            if(fdecl->getResultType().getTypePtr()->isVoidType()){
              bodystmt=callexpr;
            }else{
              //if fdecl returns a value then create ReturnStmt
              clang::ReturnStmt *returnstmt = new (context) ReturnStmt(SourceLocation());
              returnstmt->setRetValue(callexpr);
              bodystmt=returnstmt;
            }

            //callexpr->dumppretty(fdecl->getastcontext());
            //std::cout << "\n";

            clang::CompoundStmt *bodycompound=new (context) CompoundStmt(*context,(Stmt**) &bodystmt,1,SourceLocation(),SourceLocation());
            fdecl->setBody(bodycompound);

            {
              std::string temp;
              llvm::raw_string_ostream outstring_ostream(temp);
              fdecl->print(outstring_ostream);
              outstring += outstring_ostream.str();
            }

            //rewriter->replacestmt(body,callexpr);
          }
        }
        it++;
      }
      return true;
    }

    std::string &getoutstring(){
      return outstring;
    }
};

//-----------------------------------------------------------------------------------------------------------------
//Clang code generation from builtins to intrinsics
//generate CGBuiltins.cpp fragment
class ClcCGBuiltinASTConsumer : public clang::ASTConsumer{
  private:
    MangleContext *manglecontext;
    std::string outstring;
  public:
    ClcCGBuiltinASTConsumer(MangleContext *m){
      manglecontext=m;
      outstring = stringpreamble();
    }

    std::string CGqualtype(const clang::QualType &q,unsigned int argno){
      std::stringstream conv;
      const clang::Type* t=q.getTypePtr();
      std::string cgtypet;
      if(const clang::PointerType *PT = dyn_cast<clang::PointerType>(t->getCanonicalTypeInternal())){
        conv << argno;
        cgtypet=cgtypet+std::string("ArgTypes.push_back(Ops[") + conv.str() + std::string("]->getType());\n");
      }
      return cgtypet;
    }

    virtual bool HandleTopLevelDecl(DeclGroupRef D){
      //printf("declgroupref\n");
      DeclGroupRef::iterator it=D.begin();
      while(it!=D.end()){
        if(clang::FunctionDecl *fdecl=dyn_cast<clang::FunctionDecl>(*it)){
          std::string mangle_raw_string;
          llvm::raw_string_ostream mangle_raw_ostream(mangle_raw_string);
          manglecontext->mangleName(fdecl,mangle_raw_ostream);
          std::string defline;
          //function name
          defline +=std::string("case SPIR::BI__builtin");
          defline += mangle_raw_ostream.str();
          defline += std::string(":\n");
          defline += std::string("ID = Intrinsic::spir_builtin");
          defline += mangle_raw_ostream.str();
          defline += std::string(";\n");
          //There is no method to express pointers to explict address spaces as arg types in Intrinsics
          //because arg types are specifed as llvm::MVT in ValueTypes.h
          //The closest is MVT::iPTRAny which is a tablegen only overloaded type, specified in the
          //intrinsics with LLVMAnyPointerType<...
          //For all such declarations, add the following code pattern
          //"ArgTypes.push_back(Ops[Argno]->getType());
          clang::FunctionDecl::param_iterator paramit=fdecl->param_begin();
          unsigned int paramno=0;
          while(paramit!=fdecl->param_end()){
            const clang::QualType &q=(*paramit)->getOriginalType();
            const clang::Type* t=q.getTypePtr();
            defline += CGqualtype(q,paramno);
            paramno++;
            paramit++;
          }
          defline += std::string("break;\n");
          outstring += defline;
        }
        it++;
      }
      return true;
    }

    std::string &getoutstring(){
      return outstring;
    }
};

//-----------------------------------------------------------------------------------------------------------------
//Generate IntrinsicsSPIR

bool intrinsicstype(const clang::Type *t,std::string &converted){
  std::stringstream conv;
  //builtin type (non vector or pointer)
  const clang::BuiltinType *BT = dyn_cast<clang::BuiltinType>(t->getCanonicalTypeInternal());
  if(isa<BuiltinType>(t->getCanonicalTypeInternal())){
    switch(BT->getKind()){
      case BuiltinType::Void:                   converted=std::string(""); return true;break;
      case BuiltinType::Bool:                   converted=std::string("llvm_i1_ty"); return true;break;
      case BuiltinType::Char_S:                 converted=std::string("llvm_i8_ty"); return true;break;
      case BuiltinType::Char_U:                 converted=std::string("llvm_i8_ty"); return true;break;
      case BuiltinType::SChar:                  converted=std::string("llvm_i8_ty"); return true;break;
      case BuiltinType::Short:                  converted=std::string("llvm_i16_ty"); return true;break;
      case BuiltinType::Int:                    converted=std::string("llvm_i32_ty"); return true;break;
      case BuiltinType::Long:                   converted=std::string("llvm_i64_ty"); return true;break;
      case BuiltinType::LongLong:               converted=std::string("llvm_i128_ty"); return true;break;
      case BuiltinType::Int128:                 converted=std::string("llvm_i128_ty"); return true;break;
      case BuiltinType::UChar:                  converted=std::string("llvm_i8_ty"); return true;break;
      case BuiltinType::UShort:                 converted=std::string("llvm_i16_ty"); return true;break;
      case BuiltinType::UInt:                   converted=std::string("llvm_i32_ty"); return true;break;
      case BuiltinType::ULong:                  converted=std::string("llvm_i64_ty"); return true;break;
      case BuiltinType::ULongLong:              converted=std::string("llvm_i128_ty"); return true;break;
      case BuiltinType::UInt128:                converted=std::string("llvm_i128_ty"); return true;break;
                                                //half unsupported
      case BuiltinType::Half:                   std::cout << "error half unsupported\n"; exit(1);return true;break;
      case BuiltinType::Float:                  converted=std::string("llvm_f32_ty"); return true;break;
      case BuiltinType::Double:                 converted=std::string("llvm_f64_ty"); return true;break;
      case BuiltinType::LongDouble:             converted=std::string("llvm_f128_ty"); return true;break;
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
    std::string intrinsicstypet;
    conv << VT->getNumElements();
    typesuccess=intrinsicstype(VT->getElementType().getTypePtr(),intrinsicstypet);
    //strip leading "llvm_
    intrinsicstypet=intrinsicstypet.substr(5,intrinsicstypet.length()-5);
    //strip trailing "_ty"
    intrinsicstypet=intrinsicstypet.substr(0,intrinsicstypet.length()-3);
    converted=(std::string("llvm_v")+conv.str()+
        intrinsicstypet+
        std::string("_ty"));
    return true;
  }
  //pointer
  if(const clang::PointerType *PT = dyn_cast<clang::PointerType>(t->getCanonicalTypeInternal())){
    bool typesuccess;
    std::string intrinsicstypet;
    std::string addressspacestring("");
    QualType QT=PT->getPointeeType();
    if(QT.getAddressSpace()!=0) conv << QT.getAddressSpace();
    typesuccess=intrinsicstype(PT->getPointeeType().getTypePtr(),intrinsicstypet);
    //strip leading "llvm_
    intrinsicstypet=intrinsicstypet.substr(5,intrinsicstypet.length()-5);
    //strip trailing "_ty"
    intrinsicstypet=intrinsicstypet.substr(0,intrinsicstypet.length()-3);
    converted=(std::string("LLVMAnyPointerType<llvm_")+
        intrinsicstypet+
        std::string("_ty>"));
    //std::string("*")+conv.str());
    return true;
  }

  return(false);
}

std::string intrinsicsqualtype(const clang::QualType &q){
  std::stringstream conv;
  const clang::Type* t=q.getTypePtr();
  std::string intrinsicstypet;
  bool typesuccess=intrinsicstype(t,intrinsicstypet);
  if(typesuccess){
    if(intrinsicstypet=="") return ("");
    else return(intrinsicstypet);
  }
  std::string str=q.getAsString();
  return(std::string("error"));
}



class ClcIntrinsicsASTConsumer : public clang::ASTConsumer{
  private:
    MangleContext *manglecontext;
    std::string outstring;
  public:
    ClcIntrinsicsASTConsumer(MangleContext *m){
      outstring=stringpreamble();
      manglecontext=m;
    }

    /*
        virtual void HandleTranslationUnit(clang::ASTContext &Ctx){
        printf("ClcASTConsumer\n");
        }
     */

    virtual bool HandleTopLevelDecl(DeclGroupRef D){
      //printf("declgroupref\n");
      DeclGroupRef::iterator it=D.begin();
      while(it!=D.end()){
        if(clang::FunctionDecl *fdecl=dyn_cast<clang::FunctionDecl>(*it)){

          std::string fdeclname =fdecl->getNameInfo().getName().getAsString();

          std::string mangle_raw_string;
          llvm::raw_string_ostream mangle_raw_ostream(mangle_raw_string);
          manglecontext->mangleName(fdecl,mangle_raw_ostream);
          std::string line;
          //function name
          line +=std::string("def int_spir_builtin");
          //line +=fdecl->getNameInfo().getName().getAsString();
          line += mangle_raw_ostream.str();
          line += std::string(" : Intrinsic<[");
          //Field 1 return type
          {
            const clang::QualType &q=fdecl->getResultType();
            const clang::Type* t=q.getTypePtr();
            line += intrinsicsqualtype(q);
          }
          line += std::string("], [");
          //Field 2 parameters
          clang::FunctionDecl::param_iterator paramit=fdecl->param_begin();
          while(paramit!=fdecl->param_end()){
            const clang::QualType &q=(*paramit)->getOriginalType();
            const clang::Type* t=q.getTypePtr();
            line += intrinsicsqualtype(q);
            paramit++;
            if(paramit!=fdecl->param_end()) line += std::string(",");
          }

          line += std::string("], [IntrReadWriteArgMem], \"llvm.spir.builtin.");
          line += mangle_raw_ostream.str();
          line += std::string("\">;\n");
          outstring += line;
          //std::cout << defline << "\n";
        }

        it++;
      }
      return true;
    }

    std::string &getoutstring(){
      return outstring;
    }
};

//-----------------------------------------------------------------------------------------------------------------


int main(int argc,char **argv){

  enum runmodes{
    generateclangbuiltins_def,
    generateheader,
    generatecgbuiltin,
    generateintrinsics,
    generateimplementation
  } runmode;

  if(argc!=2){
    std::cout << "generateclc def | header | cgbuiltin | intrinsics | cpp\n";
    exit(1);
  }
  std::string argv1(argv[1]);
  if(argv1==std::string("def")){
    runmode=generateclangbuiltins_def;
  }
  else if(argv1==std::string("header")){
    runmode=generateheader;
  }
  else if(argv1==std::string("cgbuiltin")){
    runmode=generatecgbuiltin;
  }
  else if(argv1==std::string("intrinsics")){
    runmode=generateintrinsics;
  }
  else if(argv1==std::string("cpp")){
    runmode=generateimplementation;
  }
  else{
    std::cout << "generateclc def | header | cgbuiltin | intrinsics | cpp\n";
    exit(1);
  }


  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();

  llvm::OwningPtr<clang::CompilerInstance> llvm_clang;
  llvm::OwningPtr<llvm::ExecutionEngine> llvm_executionengine;
  llvm::OwningPtr<clang::CodeGenAction> llvm_emitllvmaction;
  llvm::OwningPtr<llvm::Module> llvm_module;

  llvm_clang.reset(new CompilerInstance);
  bool multithreadenable=llvm::llvm_start_multithreaded();
  if(!multithreadenable){
    printf("llvm::llvm_start_multithreaded failed\n");
    return 1;
  }

  clang::CompilerInstance *clang = llvm_clang.get();

  //Compiler Instance Setup
  //clang::Diagnostics
  if(!clang->hasDiagnostics()){
    clang->createDiagnostics(1,NULL);
  }
  clang::DiagnosticsEngine &diagnostic=clang->getDiagnostics();

  //clang::FileManager
  if(!clang->hasFileManager()){
    clang->createFileManager();
  }
  clang::FileManager &filemanager=clang->getFileManager();
  //clang::SourceManager
  if(!clang->hasSourceManager()){
    clang->createSourceManager(filemanager);
  }
  clang::SourceManager &sourcemanager=clang->getSourceManager();
  /*
  //clang::Sema (semantic analysis)
  clang::Sema &sema=clang->getSema();
  //clang::Preprocessor (preprocessor)
  clang::Preprocessor &pre=clang->getPreprocessor();
   */

#if 0
  CompilerInvocation &clang_invocation=clang->getInvocation();

  //TargetOptions
  // see tools/clang/lib/Frontend/CompilerInvocation.cpp static void ParseTargetArgs
  clang::TargetOptions &targetoptions=clang_invocation.getTargetOpts();
  //targetoptions.Triple = "x86_64-unknown-linux-gnu";
  targetoptions.Triple = "spir";
  // targetoptions.ABI =
  // targetoptions.CXXABI =
  //targetoptions.CPU = std::string("arm");
  // targetoptions.ABI =
  // targetoptions.AXXABI =
  // targetopLinkerLinkerVersion=
  // targetoptions.Featuers=
  clang::LangOptions &langoptions=clang_invocation.getLangOpts();
  langoptions.OpenCL=1;
  //langoptions.FakeAddressSpaceMap=1;

  //SourceManager &clang_sourcemanager=clang->getSourceManager();
  //Set CompilerInvocation options
  //see CompilerInvocation::CreateFromArgs
  //tools/clang/Frontend/CompilerInvocation.h
  //AnalyzerOptions
  //see include/clang/Frontend/AnalyzerOptions.h
  //clang::AnalyzerOptions &analyzeroptions=clang_invocation.getAnalyzerOpts();
  //CodeGenerator Options
  //see include/cl/Frontend/CodeGenOptions.h
  clang::CodeGenOptions &codegenopts=clang_invocation.getCodeGenOpts();
  //codegenopts.DebugInfo = 1;
  //Dependency Output Options
  //see include/cl/Frontend/DependencyOutputOptions.h
  //eg. OutputFile, HeaderIncludeOutputFile, Targets
  //clang::DependencyOutputOptions &dependencyoutputoptions=clang_invocation.getDependencyOutputOpts();
  //Diagnostic Options
  //see include/cl/Frontend/DiagnosticOptions.h
  //eg. Pedantic
  //clang::DiagnosticOptions &diagnosticoptions=clang_invocation.getDiagnosticOpts();
  //File System Options
  //see include/cl/Frontend/FileSystemOptions.s
  //source search directory
  //clang::FileSystemOptions &filesystemoptions=clang_invocation.getFileSystemOpts();
  //Header Search Options
  //see include/cl/Frontend/HeaderSearchOptions
  //clang::HeaderSearchOptions &headersearchoptions=clang_invocation.getHeaderSearchOpts();
  //Frontendoptions
  //see include/cl/Frontend/FrontendOptions.h
  //eg input/output files
  clang::FrontendOptions &frontendoptions=clang_invocation.getFrontendOpts();
  //frontend action kind
  //clang::frontend::ActionKind frontendoptions_actionkind=clang::frontend::EmitLLVMOnly;
  clang::frontend::ActionKind frontendoptions_actionkind=clang::frontend::EmitLLVM;
  //clang::frontend::ActionKind frontendoptions_actionkind=clang::frontend::EmitAssembly;
  frontendoptions.ProgramAction = frontendoptions_actionkind;
  //frontendoptions.DisableFree = 1;
  //Fronendoptions.Inputs

  //preprocessoroptions.clearRemappedFiles();

  //input files from disk
  std::vector<std::pair<clang::InputKind, std::string> > frontendoptions_inputs;
  frontendoptions_inputs.push_back(std::pair<clang::InputKind,std::string>(IK_C,"clc.cl"));

  frontendoptions.Inputs = frontendoptions_inputs;
  //output file
  std::string frontendoptions_outputfile("clc.ll");
  frontendoptions.OutputFile = frontendoptions_outputfile;

  //clang::Diagnostic diag(&diagnostic);
  llvm::IntrusiveRefCntPtr<clang::DiagnosticsEngine> diagnosticptr(&diagnostic);
  ASTUnit *astunit=clang::ASTUnit::LoadFromCompilerInvocationAction(&clang_invocation,diagnosticptr);

  /*
  //emit .ll
  //Execute front end action
  llvm_emitllvmaction.reset(new EmitLLVMAction());

  //get AST
  ASTUnit* astunit=llvm_emitllvmaction->takeCurrentASTUnit();

  if(!clang->ExecuteAction(*llvm_emitllvmaction)){
  llvm::errs() << "execute action failed\n";
  return CL_BUILD_PROGRAM_FAILURE;
  }
   */

  //Itanium name mangler
  ASTContext &astcontext=astunit->getASTContext();
  MangleContext *manglecontext=clang::createItaniumMangleContext(astcontext,diagnostic);

  //Pass 1 create the Builtins.def file
  clang::ASTUnit::top_level_iterator declit=astunit->top_level_begin();
  while(declit!=astunit->top_level_end()){
    if(clang::FunctionDecl *fdecl=dyn_cast<clang::FunctionDecl>(*declit)){
      std::string mangle_raw_string;
      llvm::raw_string_ostream mangle_raw_ostream(mangle_raw_string);
      manglecontext->mangleName(fdecl,mangle_raw_ostream);
      std::string defline;
      //Field 1 function name
      defline +=std::string("BUILTIN(");
      //defline +=fdecl->getNameInfo().getName().getAsString();
      defline += mangle_raw_ostream.str();
      defline += std::string(",\"");
      //Field 2 return type
      {
        const clang::QualType &q=fdecl->getResultType();
        const clang::Type* t=q.getTypePtr();
        defline += defqualtype(q);
      }
      //Field 3 parameters
      clang::FunctionDecl::param_iterator paramit=fdecl->param_begin();
      while(paramit!=fdecl->param_end()){
        const clang::QualType &q=(*paramit)->getOriginalType();
        const clang::Type* t=q.getTypePtr();
        defline += defqualtype(q);
        paramit++;
      }

      defline += std::string(",\",\"nc\")");
      std::cout << defline << "\n";
    }
    declit++;
  }

#endif

  clang->createDiagnostics(0,0);

  TargetOptions targetoptions;
  targetoptions.Triple="spir64";
  clang->setTarget(TargetInfo::CreateTargetInfo(clang->getDiagnostics(),targetoptions));

  clang::LangOptions &langoptions=clang->getLangOpts();
  langoptions.OpenCL=1;
  clang->createFileManager();


  clang->createSourceManager(clang->getFileManager());

  const FileEntry *filein=clang->getFileManager().getFile("clc.cl");
  if (!filein) {
      std::cout << "Cannot find clc.cl\n";
      exit(1);
  }
  clang->getSourceManager().createMainFileID(filein);

  clang->createPreprocessor();

  clang::Preprocessor &preprocessor=clang->getPreprocessor();
  preprocessor.getBuiltinInfo().InitializeBuiltins(preprocessor.getIdentifierTable(),preprocessor.getLangOpts());

  clang->createASTContext();

  //AST consumer for Builtins.def plus required mangler
  ASTContext &astcontext=clang->getASTContext();
  MangleContext *manglecontext=clang::createItaniumMangleContext(astcontext,diagnostic);
  /*
     ParseAST(clang->getPreprocessor(),
     &clcastconsumer,
     clang->getASTContext());
   */

  //AST consumer for clc.h using rewriter
  //clang->resetAndLeakPreprocessor();
  //clang->createPreprocessor();
  //clang->resetAndLeakASTContext();
  //clang->createASTContext();

  //Rewriter rewriter(sourcemanager,clang->getLangOpts());


  /*
     CodeCompleteConsumer *codecompleteconsumer=0;
     if(clang->hasCodeCompletionConsumer()) codecompleteconsumer=&clang->getCodeCompletionConsumer();
     if(!clang->hasSema()){
    clang->createSema(TU_Complete,codecompleteconsumer);
  }
  */
  //clcheaderastconsumer.setSema(&(clang->getSema()));

  if(runmode==generateclangbuiltins_def){
    ClcDefASTConsumer clcdefastconsumer(manglecontext);
    llvm::OwningPtr<Sema> sema(new Sema(clang->getPreprocessor(),
                                        clang->getASTContext(),
                                        clcdefastconsumer,
                                        TU_Complete,
                                        NULL) );

    clang->getDiagnosticClient().BeginSourceFile(clang->getLangOpts(),&clang->getPreprocessor());

    ParseAST(*sema.get(),
           false);

    std::cout << "generating BuiltinsSPIR.def\n";
    std::ofstream clcofstream("BuiltinsSPIR.def");
    clcofstream << clcdefastconsumer.getoutstring();
    clcofstream.close();
  }
  if(runmode==generateheader){
    ClcHeaderASTConsumer clcheaderastconsumer(/*&rewriter,*/manglecontext,&clang->getPreprocessor());
    llvm::OwningPtr<Sema> sema(new Sema(clang->getPreprocessor(),
                                        clang->getASTContext(),
                                        clcheaderastconsumer,
                                       TU_Complete,
                                       NULL) );
    clcheaderastconsumer.setSema(sema.get());

    clang->getDiagnosticClient().BeginSourceFile(clang->getLangOpts(),&clang->getPreprocessor());

    ParseAST(*sema.get(),
           false);

    std::cout << "generating clcbuiltins.h\n";
    std::ofstream clcheaderofstream("clcbuiltins.h");
    clcheaderofstream << clcheaderastconsumer.getoutstring();
    clcheaderofstream.close();
  }
  if(runmode==generatecgbuiltin){
    ClcCGBuiltinASTConsumer clccgbuiltinastconsumer(/*&rewriter,*/manglecontext);
    llvm::OwningPtr<Sema> sema(new Sema(clang->getPreprocessor(),
                                        clang->getASTContext(),
                                        clccgbuiltinastconsumer,
                                       TU_Complete,
                                       NULL) );
    clang->getDiagnosticClient().BeginSourceFile(clang->getLangOpts(),&clang->getPreprocessor());

    ParseAST(*sema.get(),
           false);

    std::cout << "generating CGBuiltinsSPIR.inc fragment\n";
    std::ofstream clccgbuiltinofstream("CGBuiltinSPIR.inc");
    clccgbuiltinofstream << clccgbuiltinastconsumer.getoutstring();
    clccgbuiltinofstream.close();
  }

  if(runmode==generateintrinsics){
    ClcIntrinsicsASTConsumer clcintrinsicsastconsumer(/*&rewriter,*/manglecontext);
    llvm::OwningPtr<Sema> sema(new Sema(clang->getPreprocessor(),
                                        clang->getASTContext(),
                                        clcintrinsicsastconsumer,
                                       TU_Complete,
                                       NULL) );
    clang->getDiagnosticClient().BeginSourceFile(clang->getLangOpts(),&clang->getPreprocessor());

    ParseAST(*sema.get(),
           false);

    std::cout << "generating IntrinsicsSPIRgen.td fragment\n";
    std::ofstream clccgbuiltinofstream("IntrinsicsSPIRgen.td");
    clccgbuiltinofstream << clcintrinsicsastconsumer.getoutstring();
    clccgbuiltinofstream.close();
  }
  if(runmode==generateimplementation){
    ClcImplementationASTConsumer clcimplementationastconsumer(/*&rewriter,*/manglecontext,&clang->getPreprocessor());
    llvm::OwningPtr<Sema> sema(new Sema(clang->getPreprocessor(),
                                        clang->getASTContext(),
                                        clcimplementationastconsumer,
                                       TU_Complete,
                                       NULL) );
    clcimplementationastconsumer.setSema(sema.get());

    clang->getDiagnosticClient().BeginSourceFile(clang->getLangOpts(),&clang->getPreprocessor());

    ParseAST(*sema.get(),
           false);

    std::cout << "generating clc.cpp fragment\n";
    std::ofstream clcimplementationofstream("clc.cpp");
    clcimplementationofstream << std::string("#include \"math.h\"\n");
    clcimplementationofstream << std::string("#include \"fenv.h\"\n");
    clcimplementationofstream << std::string("#include \"string.h\"\n");
    clcimplementationofstream << std::string("#include <libspir_types.h>\n");
    clcimplementationofstream << std::string("\n");
    clcimplementationofstream << std::string("extern \"C\" \n{\n");
    clcimplementationofstream << std::string("\n");
    clcimplementationofstream << clcimplementationastconsumer.getoutstring();
    clcimplementationofstream << std::string("}\n");
    clcimplementationofstream.close();

    //math
    {
      std::ofstream cm("math_builtins.cpp");
      cm << std::string("#include \"math.h\"\n");
      cm << std::string("#include <libspir_types.h>\n");
      cm << std::string("#include \"math_impl.c\"\n");
      cm << std::string("#define MAKE_HEX_FLOAT(x, y, z) x\n");
      cm << std::string("#include \"hlsmath/hlsmath_base.cpp\"\n");
      cm << std::string("#include \"hlsmath/hlsmath_trig.cpp\"\n");
      cm << std::string("#include \"hlsmath/hlsmath_exp.cpp\"\n");
      cm << std::string("#include \"hlsmath/hlsmath_func.cpp\"\n");
      cm << std::string("\n");
      cm << std::string("extern \"C\" {\n");
      cm << std::string("\n");
      cm << clcimplementationastconsumer.getoutstring_math();
      cm << std::string("} //extern C\n");
      cm.close();
    }

    //native math
    {
      std::ofstream cm("native_builtins.c");
      cm << std::string("#include \"math.h\"\n");
      cm << std::string("#include <libspir_types.h>\n");
      cm << std::string("\n");
//      cm << std::string("extern \"C\" \n{\n");
      cm << std::string("\n");
      cm << clcimplementationastconsumer.getoutstring_native();
//      cm << std::string("}\n");
      cm.close();
    }

    //common functions
    {
      std::ofstream cc("commonfns_builtins.c");
      cc << std::string("#include \"math.h\"\n");
      cc << std::string("#include \"fenv.h\"\n");
      cc << std::string("#include \"string.h\"\n");
      cc << std::string("#include <libspir_types.h>\n");
      cc << std::string("#include \"commonfns_impl.c\"\n");
      cc << std::string("\n");
      //cm << std::string("extern \"C\" \n{\n");
      cc << std::string("\n");
      cc << clcimplementationastconsumer.getoutstring_commonfns();
      //cm << std::string("}\n");
      cc.close();
    }

    //geometric functions
    {
      std::ofstream geo("geometric_builtins.c");
      geo << std::string("#include <math.h>\n");
      geo << std::string("#include \"fenv.h\"\n");
      geo << std::string("#include \"string.h\"\n");
      geo << std::string("#include <libspir_types.h>\n");
      geo << std::string("\n");
      //cm << std::string("extern \"C\" \n{\n");
      geo << std::string("\n");
      geo << clcimplementationastconsumer.getoutstring_geometric();
      //cm << std::string("}\n");
      geo.close();
    }

    //relational
    {
      std::ofstream cr("relational_builtins.cpp");
      cr << std::string("#include \"math.h\"\n");
      cr << std::string("#include \"relational_impl.c\"\n");
      cr << std::string("#include <libspir_types.h>\n");
      cr << std::string("#include \"hlsmath/hlsmath_base.cpp\"\n");
      cr << std::string("#include \"hlsmath/hlsmath_trig.cpp\"\n");
      cr << std::string("#include \"hlsmath/hlsmath_exp.cpp\"\n");
      cr << std::string("#include \"hlsmath/hlsmath_func.cpp\"\n");
      cr << std::string("extern \"C\" {\n");
      cr << std::string("\n");
      cr << clcimplementationastconsumer.getoutstring_relational();
      cr << std::string("} //extern C\n");
      cr.close();
    }

    //conversions
    {
      std::vector<std::string> &conversions= clcimplementationastconsumer.getoutstring_conversions();
      std::vector<std::string>::iterator it=conversions.begin();
      unsigned int itint=0;
      while(it!=conversions.end()){
        char splitc[10];
        sprintf(splitc,"%i",itint);
        std::string splitstring(splitc);
        std::string filename=std::string("conversions_builtins")+splitstring+std::string(".cpp");
        std::ofstream cr(filename.c_str());
        cr << std::string("#include \"math.h\"\n");
        cr << std::string("#include \"fenv.h\"\n");
        cr << std::string("#include \"string.h\"\n");
        cr << std::string("#include \"hlsmath/hlsmath_conv.cpp\"\n");
        cr << std::string("#include \"conversions_impl.cpp\"\n");
        cr << std::string("#include <libspir_types.h>\n");
        cr << std::string("extern \"C\" {\n");
        cr << std::string("\n");
        cr << (*it);
        cr << std::string("} //extern C\n");
        cr.close();
        it++;
        itint++;
      }
    }

    //integer
    {
      std::ofstream cr("integer_builtins.c");
      cr << std::string("#include \"math.h\"\n");
      cr << std::string("#include \"fenv.h\"\n");
      cr << std::string("#include \"string.h\"\n");
      cr << std::string("#include \"integer_impl.c\"\n");
      cr << std::string("#include <libspir_types.h>\n");
      cr << std::string("\n");
      cr << clcimplementationastconsumer.getoutstring_integer();
      cr.close();
    }

    //async_copies_builtins
    {
      std::ofstream os("async_copies_builtins.cpp");
      os << std::string("#include \"libspir_types.h\"\n");
      os << std::string("#include <string.h>\n");
      os << std::string("extern \"C\" {\n");
      os << std::string("\n");
      os << std::string("void _ssdm_xcl_PointerMap(...);\n");
      os << std::string("\n");
      os << clcimplementationastconsumer.getoutstring_async();
      os << std::string("} //extern C\n");
      os.close();
    }



  }
  return 0;
}


