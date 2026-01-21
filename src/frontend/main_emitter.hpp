#pragma once

#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/BuiltinOps.h"
#include "llvm/Support/raw_ostream.h"

namespace frontend {

bool emitFheMain(llvm::raw_ostream &os, mlir::ModuleOp module);

}  // namespace frontend
