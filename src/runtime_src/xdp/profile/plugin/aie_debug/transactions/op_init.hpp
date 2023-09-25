// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. - All rights reserved
#ifndef __OPINIT_HPP__
#define __OPINIT_HPP__

#include <xaiengine.h>

#include "op_types.h"

using namespace std;

class instr_base{
public:
    instr_base() : op_ptr_(nullptr) {
    }
    virtual ~instr_base() {
    }
    virtual void serialize( void * ptr) const
    {
        //cout << "SERIALIZE numbytes: " << op_ptr_->size_in_bytes << std::endl;
        memcpy ( ptr, op_ptr_, op_ptr_->size_in_bytes);
    }

    virtual unsigned size() const
    {
        return op_ptr_->size_in_bytes;
    }

    virtual string type() const = 0;
protected:
    op_base * op_ptr_;
};

class transaction_op: public instr_base{
public:
    transaction_op() = delete;

    transaction_op( void * txn )
    {
        XAie_TxnHeader *Hdr = (XAie_TxnHeader *)txn;
        printf("Header version %d.%d\n", Hdr->Major, Hdr->Minor);
        printf("Device Generation: %d\n", Hdr->DevGen);
        printf("Cols, Rows, NumMemRows : (%d, %d, %d)\n", Hdr->NumCols, Hdr->NumRows, Hdr->NumMemTileRows);
        printf("TransactionSize: %u\n", Hdr->TxnSize);
        printf("NumOps: %u\n", Hdr->NumOps);

        transaction_op_t * tptr = new transaction_op_t();
        tptr->b.type = e_TRANSACTION_OP;
        tptr->b.size_in_bytes = sizeof(transaction_op_t) + Hdr->TxnSize;

        cmdBuf_ = new uint8_t[Hdr->TxnSize];
        memcpy(cmdBuf_, txn, Hdr->TxnSize);
        op_ptr_ = (op_base*)tptr;
        TxnSize = Hdr->TxnSize;
    }

    virtual void serialize( void * ptr) const override
    {
        memcpy ( ptr, op_ptr_, sizeof(transaction_op_t) );
        ptr = (char*) ptr + sizeof(transaction_op_t);
        memcpy ( ptr, cmdBuf_, TxnSize);
    }

    virtual ~transaction_op()
    {
        transaction_op_t * tptr = reinterpret_cast<transaction_op_t*>(op_ptr_);
        delete tptr;
        if (cmdBuf_) delete[] cmdBuf_;
    }
    virtual string type() const override { return "transaction_op";}
private:
    uint8_t* cmdBuf_;
    uint32_t TxnSize;
};

#endif