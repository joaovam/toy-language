#ifndef AFFINE_FULL_UNROLL_H
#define AFFINE_FULL_UNROLL_H

#include "mlir/Pass/Pass.h"
#include "llvm/ADT/StringRef.h"

class AffineFullUnrollPass : public PassWrapper<AffineFullUnrollPass, OperationPass<mlir::func::FuncOp>>{
private:
    void runOnOperation() override;

    StringRef getArgument
};

#endif