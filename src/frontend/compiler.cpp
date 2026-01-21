#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/ToolOutputFile.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/EmitC/IR/EmitC.h"
#include "mlir/IR/Dialect.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/Support/TypeID.h"
#include "mlir/InitAllDialects.h"
#include "mlir/InitAllPasses.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Pass/PassManager.h"
#include "mlir/Support/FileUtilities.h"
#include "mlir/Tools/mlir-opt/MlirOptMain.h"
#include "mlir/Parser/Parser.h"
#include "mlir/Dialect/DLTI/DLTI.h"
#include "mlir/Transforms/DialectConversion.h"


#include "mlir/IR/OwningOpRef.h"
#include "mlir/Target/Cpp/CppEmitter.h"
#include "llvm/Support/raw_ostream.h"

#include "mlir/Transforms/GreedyPatternRewriteDriver.h"
#include "mlir/Tools/mlir-translate/MlirTranslateMain.h"



#include <llvm/ADT/APFloat.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/raw_ostream.h>
#include <mlir/Dialect/Affine/IR/AffineOps.h>
#include <mlir/Dialect/Func/IR/FuncOps.h>
#include <mlir/Dialect/MemRef/IR/MemRef.h>
#include <mlir/Dialect/SCF/IR/SCF.h>
#include <mlir/Dialect/Vector/IR/VectorOps.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/IR/Operation.h>
#include <mlir/IR/Value.h>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <cstdlib>

#include <llvm/Demangle/Demangle.h>

#include "main_emitter.hpp"

using namespace mlir;
using namespace llvm;




std::string demangle(const std::string &mangledName) {
    char *demangledName = llvm::itaniumDemangle(mangledName.c_str());
    std::string result(demangledName);
    free(demangledName);
    size_t pos = result.find('(');

    if (pos != std::string::npos) {
        result = result.substr(0, pos);
    }

    std::cout << result << std::endl;
    return result;
}

namespace {

struct ArithToEmitc : public PassWrapper<ArithToEmitc, OperationPass<ModuleOp>> {
    void runOnOperation() override {
        ModuleOp module = getOperation();

        auto attrDict = module->getAttrDictionary();

        // Iterate over all attributes and remove them
        for (auto attr : attrDict) {
            module->removeAttr(attr.getName());
        }


        OpBuilder builder(module.getContext());

        auto context = emitc::OpaqueType::get(builder.getContext(), "FHEcontext");
        auto fhecontext = emitc::PointerType::get(context);
        auto fhedouble = emitc::OpaqueType::get(builder.getContext(), "FHEdouble");
        auto fhei32 = emitc::OpaqueType::get(builder.getContext(), "FHEi32");

        // Iterate over functions in the module.
        module.walk([&](func::FuncOp func) {

        builder.setInsertionPoint(func);

        std::string originalName = func.getName().str();
        std::string demangledName = demangle(originalName);
        std::string newName = demangledName;
        func.setName(newName);

        // Update function argument types and return type.
        auto funcType = func.getFunctionType();
        SmallVector<mlir::Type, 4> newInputTypes;

        newInputTypes.push_back(fhecontext);
        for (auto inputType : funcType.getInputs()) {
            // Convert types like `f64` to `emitc.opaque<"FHEdouble">`.
            if (inputType.isF64()) {
            newInputTypes.push_back(fhedouble);
            } 
            else if(inputType.isInteger(32)) {
                newInputTypes.push_back(fhedouble);
            }
            else if(auto vecType = inputType.dyn_cast<mlir::VectorType>()) {
                auto elemType = vecType.getElementType();
                if (elemType.isF64()) {
                    newInputTypes.push_back(fhedouble);
                }
                else if(elemType.isInteger(32)) {
                    newInputTypes.push_back(fhei32);
                }
            }
            else if(auto memRefType = inputType.dyn_cast<mlir::MemRefType>()) {
                auto elemType = memRefType.getElementType();
                if (elemType.isInteger(32) || elemType.isF64()) {
                    newInputTypes.push_back(emitc::PointerType::get(fhedouble));
                } else {
                    newInputTypes.push_back(inputType);
                }
            }
            else {
            newInputTypes.push_back(inputType);
            }
        }

        SmallVector<mlir::Type, 4> newResultTypes;
        for (auto returnType : funcType.getResults()) {
            if (returnType.isF64() || returnType.isInteger(32)) {
                newResultTypes.push_back(fhedouble);
            } else {
                newResultTypes.push_back(returnType);
            }
        }

        func.setType(mlir::FunctionType::get(builder.getContext(), newInputTypes, newResultTypes));

        Block &entryBlock = func.getBody().front();

        SmallVector<BlockArgument, 4> newArgs;
        newArgs.push_back(entryBlock.insertArgument(
            entryBlock.args_begin(), 
            newInputTypes[0], 
            builder.getUnknownLoc()
        ));
        outs() << "Adding argument 0 of type " << newInputTypes[0] << "\n";

        for (size_t i = 1; i < newInputTypes.size(); ++i) {
            auto oldArg = entryBlock.getArgument(i);
            if (newInputTypes[i] != oldArg.getType()) {
                outs() << "Changing type of argument " << i << " from " << oldArg.getType() << " to " << newInputTypes[i] << "\n";
                oldArg.setType(newInputTypes[i]);
            }
            outs() << "Adding argument " << i << " of type " << newInputTypes[i] << "\n";
        }


        auto ckarg = func.getArgument(0);

        func.walk([&](Operation *op) {      
            builder.setInsertionPoint(op);

            if (op->getDialect()->getNamespace() == "vector") { 
                if (auto vectorOp = dyn_cast<vector::BroadcastOp>(op)){
                    auto value = vectorOp->getOperand(0);
                    if (value.dyn_cast<BlockArgument>()){
                        vectorOp.replaceAllUsesWith(value);
                    }
                    else{
                        auto newOp = builder.create<emitc::CallOp>(
                        vectorOp.getLoc(),
                        TypeRange(fhedouble),
                        llvm::StringRef("FHEbroadcast"),
                        ArrayAttr(),
                        ArrayAttr(),
                        mlir::ArrayRef<mlir::Value>{ckarg, value});

                    vectorOp.replaceAllUsesWith(newOp.getResult(0));
                    }
                    
                    vectorOp.erase();
                }
                else if (auto vectorOp = dyn_cast<vector::TransferReadOp>(op)){
                    builder.setInsertionPoint(vectorOp);
                    SmallVector<mlir::Value, 4> operands;
                    operands.push_back(ckarg);
                    operands.push_back(vectorOp.getOperand(0));
                    for (auto index : vectorOp.getIndices()) {
                        operands.push_back(index);
                    }
                    auto newOp = builder.create<emitc::CallOp>(
                        vectorOp.getLoc(),
                        TypeRange(fhedouble),
                        llvm::StringRef("FHEloadf"),
                        ArrayAttr(),
                        ArrayAttr(),
                        operands);
                    vectorOp.replaceAllUsesWith(newOp.getResult(0));
                    vectorOp.erase();
                }
                else if (auto vectorOp = dyn_cast<vector::TransferWriteOp>(op)){
                    builder.setInsertionPoint(vectorOp);
                    SmallVector<mlir::Value, 4> operands;
                    operands.push_back(ckarg);
                    operands.push_back(vectorOp.getOperand(0));
                    operands.push_back(vectorOp.getOperand(1));
                    for (auto index : vectorOp.getIndices()) {
                        operands.push_back(index);
                    }
                    builder.create<emitc::CallOp>(
                        vectorOp.getLoc(),
                        TypeRange(),
                        llvm::StringRef("FHEstoref"),
                        ArrayAttr(),
                        ArrayAttr(),
                        operands);
                    vectorOp.erase();
                }
                else if (auto vectorOp = dyn_cast<vector::ReductionOp>(op)){
                    auto red = vectorOp.getKind();                    auto value = vectorOp.getOperand(0);

                    llvm::StringRef funcName;
                    switch (red) {
                        case vector::CombiningKind::ADD:
                            funcName = "FHEvectorSum";
                            break;
                        case vector::CombiningKind::MUL:
                            funcName = "FHEvectorMul";
                            break;
                        case vector::CombiningKind::MINF:
                            funcName = "FHEvectorMin";
                            break;
                        case vector::CombiningKind::MAXF:
                            funcName = "FHEvectorMax";
                            break;
                        default:
                            llvm::errs() << "Unsupported reduction operation\n";
                            return;
                    }
                    auto newOp = builder.create<emitc::CallOp>(
                        vectorOp.getLoc(),
                        TypeRange(fhedouble),
                        funcName,
                        ArrayAttr(),
                        ArrayAttr(),
                        mlir::ArrayRef<mlir::Value>{ckarg, value});
                
                    vectorOp.replaceAllUsesWith(newOp->getResult(0));
                    vectorOp.erase();
                }
            
            }
            
            else if (auto cmpOp = dyn_cast<arith::CmpFOp>(op)){
                builder.setInsertionPoint(cmpOp);
                auto arg0 = cmpOp.getOperand(0);
                auto arg1 = cmpOp.getOperand(1);
                auto predicate = cmpOp.getPredicate();
                auto s = stringifyCmpFPredicate(predicate);
                auto newOp = builder.create<emitc::CallOp>(
                    cmpOp.getLoc(),
                    TypeRange(fhedouble),
                    llvm::StringRef("FHE" + s.str().substr(1,2)),
                    ArrayAttr(),
                    ArrayAttr(),
                    mlir::ArrayRef<mlir::Value>{ckarg, arg0, arg1});

                cmpOp.replaceAllUsesWith(newOp.getResult(0));
                cmpOp.erase();
            }
            else if (auto cmpOp = dyn_cast<arith::CmpIOp>(op)){
                builder.setInsertionPoint(cmpOp);
                auto arg0 = cmpOp.getOperand(0);
                auto arg1 = cmpOp.getOperand(1);
                auto predicate = cmpOp.getPredicate();
                std::string funcName;
                switch (predicate) {
                    case arith::CmpIPredicate::eq:
                        funcName = "FHEeq";
                        break;
                    case arith::CmpIPredicate::ne:
                        funcName = "FHEne";
                        break;
                    case arith::CmpIPredicate::slt:
                    case arith::CmpIPredicate::ult:
                        funcName = "FHElt";
                        break;
                    case arith::CmpIPredicate::sgt:
                    case arith::CmpIPredicate::ugt:
                        funcName = "FHEgt";
                        break;
                    case arith::CmpIPredicate::sle:
                    case arith::CmpIPredicate::ule:
                        funcName = "FHEle";
                        break;
                    case arith::CmpIPredicate::sge:
                    case arith::CmpIPredicate::uge:
                        funcName = "FHEge";
                        break;
                }
                auto newOp = builder.create<emitc::CallOp>(
                    cmpOp.getLoc(),
                    TypeRange(fhedouble),
                    llvm::StringRef(funcName),
                    ArrayAttr(),
                    ArrayAttr(),
                    mlir::ArrayRef<mlir::Value>{ckarg, arg0, arg1});

                cmpOp.replaceAllUsesWith(newOp.getResult(0));
                cmpOp.erase();
            }
            
            else if (auto selectOp = dyn_cast<arith::SelectOp>(op)){
                builder.setInsertionPoint(selectOp);
                auto arg0 = selectOp.getOperand(0);
                auto arg1 = selectOp.getOperand(1);
                auto arg2 = selectOp.getOperand(2);

                auto newOp = builder.create<emitc::CallOp>(
                    selectOp.getLoc(),
                    TypeRange(fhedouble),
                    llvm::StringRef("FHEselect"),
                    ArrayAttr(),
                    ArrayAttr(),
                    mlir::ArrayRef<mlir::Value>{ckarg, arg0, arg1, arg2});
                selectOp.replaceAllUsesWith(newOp.getResult(0));
                selectOp.erase();
            }

            else if (auto forOp = dyn_cast<scf::ForOp>(op)){
                builder.setInsertionPoint(forOp);
                auto *newblock = forOp.getBody();
                auto res = forOp.getResults();
                for (auto it : llvm::enumerate(res)) {                    
                    if (it.value().getType().isF64() || it.value().getType().isInteger(32)) {
                        it.value().setType(fhedouble);
                    }
                    else if(auto vecType = it.value().getType().dyn_cast<mlir::VectorType>()) {                        auto elemType = vecType.getElementType();
                        if (elemType.isF64()) {                            
                            it.value().setType(fhedouble);
                        }
                    }
                }
                for (auto it : llvm::enumerate(newblock->getArguments())) {                    
                    if (it.value().getType().isF64() || it.value().getType().isInteger(32)) {
                        it.value().setType(fhedouble);
                    }
                    else if(auto vecType = it.value().getType().dyn_cast<mlir::VectorType>()) {
                        auto elemType = vecType.getElementType();
                        if (elemType.isF64()) {
                            it.value().setType(fhedouble);
                        }
                    }
                }
            }

            else if (auto arithOp = dyn_cast<arith::ConstantOp>(op)){                
                if (auto vectorType = arithOp.getType().dyn_cast<mlir::VectorType>()) {
                    // Handle vector constant
                    auto denseAttr = arithOp.getValue().cast<DenseElementsAttr>();
                    std::vector<double> values;
                    for (auto value : denseAttr.getValues<APFloat>()) {
                        values.push_back(value.convertToDouble());
                    }
                    
                    std::stringstream ss;
                    auto dim = vectorType.getDimSize(0);
                    auto size = values.size();
                    if (size == 1){
                        ss << "std::vector<double>(";
                        ss << dim << ", " << values[0] << ")";
                    }
                    else{
                        ss << "std::vector<double>{";
                        for (size_t i = 0; i < values.size(); ++i) {
                            if (i > 0) ss << ", ";
                            ss << values[i];
                        }
                        ss << "}";
                    }

                    auto opaqueAttr = emitc::OpaqueAttr::get(builder.getContext(), ss.str());
                    auto vecDoubleType = emitc::OpaqueType::get(builder.getContext(), "std::vector<double>");
        
                    auto newConstantOp = builder.create<emitc::ConstantOp>(
                        arithOp.getLoc(),
                        vecDoubleType, // Keep the original type
                        opaqueAttr
                    );
                    
                    auto newCallOp = builder.create<emitc::CallOp>(
                        arithOp.getLoc(),
                        TypeRange(fhedouble),
                        llvm::StringRef("FHEencrypt"),
                        ArrayAttr(),
                        ArrayAttr(),
                        mlir::ArrayRef<mlir::Value>{ckarg, newConstantOp.getResult()}
                    );
                    
                    arithOp.replaceAllUsesWith(newCallOp->getResult(0));
                    arithOp.erase();
                
                } 
                else {
                    // Handle scalar constant - fixed the FloatAttr casting
                    if (auto floatAttr = arithOp.getValue().dyn_cast<FloatAttr>()) {
                        double value2 = floatAttr.getValueAsDouble();
                        
                        auto newConstantOp = builder.create<emitc::ConstantOp>(
                            arithOp.getLoc(),
                            builder.getF64Type(),
                            builder.getF64FloatAttr(value2)
                        );
                        
                        auto newCallOp = builder.create<emitc::CallOp>(
                            arithOp.getLoc(),
                            TypeRange(fhedouble),
                            llvm::StringRef("FHEencrypt"),
                            ArrayAttr(),
                            ArrayAttr(),
                            mlir::ArrayRef<mlir::Value>{ckarg, newConstantOp.getResult()}
                        );
                        
                        arithOp.replaceAllUsesWith(newCallOp->getResult(0));
                        arithOp.erase();
                    } else if (auto intAttr = arithOp.getValue().dyn_cast<IntegerAttr>()) {
                        if (!arithOp.getType().isInteger(32)) {
                            return;
                        }
                        double value2 = static_cast<double>(intAttr.getInt());
                        auto newConstantOp = builder.create<emitc::ConstantOp>(
                            arithOp.getLoc(),
                            builder.getF64Type(),
                            builder.getF64FloatAttr(value2)
                        );
                        auto newCallOp = builder.create<emitc::CallOp>(
                            arithOp.getLoc(),
                            TypeRange(fhedouble),
                            llvm::StringRef("FHEencrypt"),
                            ArrayAttr(),
                            ArrayAttr(),
                            mlir::ArrayRef<mlir::Value>{ckarg, newConstantOp.getResult()}
                        );
                        arithOp.replaceAllUsesWith(newCallOp->getResult(0));
                        arithOp.erase();
                    }
                }
                         
            }

            else if (auto arithOp = dyn_cast<arith::AddFOp>(op)) {
                builder.setInsertionPoint(arithOp);

                // Create a new `emitc.call` operation.
                auto arg0 = arithOp.getOperand(0);
                auto arg1 = arithOp.getOperand(1);                

                auto newOp = builder.create<emitc::CallOp>(
                    arithOp.getLoc(),
                    TypeRange(fhedouble),
                    llvm::StringRef("FHEaddf"),
                    ArrayAttr(),
                    ArrayAttr(),
                    mlir::ArrayRef<mlir::Value>{ckarg, arg0, arg1});

                // Replace the original addf operation with the new one.
                arithOp.replaceAllUsesWith(newOp.getResult(0));
                arithOp.erase();
            }
            else if (auto arithOp = dyn_cast<arith::SubFOp>(op)) {
                builder.setInsertionPoint(arithOp);

                // Create a new `emitc.call` operation.
                auto arg0 = arithOp.getOperand(0);
                auto arg1 = arithOp.getOperand(1);
                

                auto newOp = builder.create<emitc::CallOp>(
                    arithOp.getLoc(),
                    TypeRange(fhedouble),
                    llvm::StringRef("FHEsubf"),
                    ArrayAttr(),
                    ArrayAttr(),
                    mlir::ArrayRef<mlir::Value>{ckarg, arg0, arg1});

                // Replace the original addf operation with the new one.
                arithOp.replaceAllUsesWith(newOp.getResult(0));
                arithOp.erase();
            }

            else if (auto arithOp = dyn_cast<arith::MulFOp>(op)) {
                builder.setInsertionPoint(arithOp);

                // Create a new `emitc.call` operation.
                auto arg0 = arithOp.getOperand(0);
                auto arg1 = arithOp.getOperand(1);
                

                auto newOp = builder.create<emitc::CallOp>(
                    arithOp.getLoc(),
                    TypeRange(fhedouble),
                    llvm::StringRef("FHEmulf"),
                    ArrayAttr(),
                    ArrayAttr(),
                    mlir::ArrayRef<mlir::Value>{ckarg, arg0, arg1});

                // Replace the original addf operation with the new one.
                arithOp.replaceAllUsesWith(newOp.getResult(0));
                arithOp.erase();
            }
            else if (auto arithOp = dyn_cast<arith::DivFOp>(op)) {
                builder.setInsertionPoint(arithOp);

                // Create a new `emitc.call` operation.
                auto arg0 = arithOp.getOperand(0);
                auto arg1 = arithOp.getOperand(1);
                

                auto newOp = builder.create<emitc::CallOp>(
                    arithOp.getLoc(),
                    TypeRange(fhedouble),
                    llvm::StringRef("FHEdivf"),
                    ArrayAttr(),
                    ArrayAttr(),
                    mlir::ArrayRef<mlir::Value>{ckarg, arg0, arg1});

                // Replace the original addf operation with the new one.
                arithOp.replaceAllUsesWith(newOp.getResult(0));
                arithOp.erase();
            }
            else if (auto extOp = dyn_cast<arith::ExtUIOp>(op)) {
                if (extOp.getIn().getType() == fhedouble) {
                    extOp.replaceAllUsesWith(extOp.getIn());
                    extOp.erase();
                } else {
                    builder.setInsertionPoint(extOp);
                    auto newOp = builder.create<emitc::CastOp>(
                        extOp.getLoc(),
                        extOp.getType(),
                        extOp.getIn());
                    extOp.replaceAllUsesWith(newOp.getResult());
                    extOp.erase();
                }
            }
            else if (auto loadOp = dyn_cast<memref::LoadOp>(op)) {
                builder.setInsertionPoint(loadOp);
                SmallVector<mlir::Value, 4> operands;
                operands.push_back(ckarg);
                operands.push_back(loadOp.getMemRef());
                for (auto index : loadOp.getIndices()) {
                    operands.push_back(index);
                }
                auto newOp = builder.create<emitc::CallOp>(
                    loadOp.getLoc(),
                    TypeRange(fhedouble),
                    llvm::StringRef("FHEloadf"),
                    ArrayAttr(),
                    ArrayAttr(),
                    operands);
                loadOp.replaceAllUsesWith(newOp.getResult(0));
                loadOp.erase();
            }
            else if (auto storeOp = dyn_cast<memref::StoreOp>(op)) {
                builder.setInsertionPoint(storeOp);
                SmallVector<mlir::Value, 4> operands;
                operands.push_back(ckarg);
                operands.push_back(storeOp.getValueToStore());
                operands.push_back(storeOp.getMemRef());
                for (auto index : storeOp.getIndices()) {
                    operands.push_back(index);
                }
                builder.create<emitc::CallOp>(
                    storeOp.getLoc(),
                    TypeRange(),
                    llvm::StringRef("FHEstoref"),
                    ArrayAttr(),
                    ArrayAttr(),
                    operands);
                storeOp.erase();
            }
            
        });
        });
    }

    MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(ArithToEmitc)
};

} // end anonymous namespace
int main(int argc, char **argv) {
    // Initialize MLIR context with all dialects.
    mlir::MLIRContext context;
    context.getOrLoadDialect<mlir::emitc::EmitCDialect>();
    context.getOrLoadDialect<mlir::arith::ArithDialect>();
    context.getOrLoadDialect<mlir::func::FuncDialect>();
    context.getOrLoadDialect<mlir::affine::AffineDialect>();
    context.getOrLoadDialect<mlir::scf::SCFDialect>();
    context.getOrLoadDialect<mlir::LLVM::LLVMDialect>();
    context.getLoadedDialect<mlir::vector::VectorDialect>();
    context.getLoadedDialect<mlir::memref::MemRefDialect>();

    mlir::DialectRegistry registry;
    // registry.insert<mlir::DLTIDialect>();  // Assuming DLTI dialect is available
    registry.insert<mlir::arith::ArithDialect>();
    registry.insert<mlir::LLVM::LLVMDialect>();
    registry.insert<mlir::func::FuncDialect>();
    registry.insert<emitc::EmitCDialect>();
    registry.insert<mlir::affine::AffineDialect>();
    registry.insert<mlir::scf::SCFDialect>();
    registry.insert<mlir::vector::VectorDialect>();
    registry.insert<mlir::memref::MemRefDialect>();

    // Attach the registry to the context
    context.appendDialectRegistry(registry);
    context.allowUnregisteredDialects();

    std::string inputFilename = "examples/mlir/output1.mlir";
    std::string outputFilename = "examples/mlir/ir.mlir";
    if (argc >= 2) {
        inputFilename = argv[1];
    }
    if (argc >= 3) {
        outputFilename = argv[2];
    }
    if (argc > 3) {
        llvm::errs() << "Usage: chess [input.mlir] [output.mlir]\n";
        return 1;
    }

    // Open and parse the MLIR file
    std::string filename = inputFilename;
    auto module = mlir::parseSourceFile<mlir::ModuleOp>(filename, &context);
    if (!module) {
        llvm::errs() << "Error parsing MLIR file\n";
        return 1;
    }

    // Apply the pass to replace operations with EmitC function calls.

    outs() << "starting pass\n";
    PassManager pm(&context);
    pm.addPass(std::make_unique<ArithToEmitc>());
    // pm.addPass(std::make_unique<AffineOps>());
    if (failed(pm.run(*module))) {
        llvm::errs() << "Pass failed\n";
        return 1;
    }

    outs() << "Pass succeeded\n";

    // Output the transformed module
    auto outputFile = openOutputFile(outputFilename);
    if (!outputFile) {
        llvm::errs() << "Error opening output file\n";
        return 1;
    }
    bool emitCpp = false;
    if (outputFilename.size() >= 4 &&
        outputFilename.compare(outputFilename.size() - 4, 4, ".cpp") == 0) {
        emitCpp = true;
    }

    if (emitCpp) {
        outputFile->os()
            << "#include \"../../src/backend/fhe_operations.hpp\"\n"
               "#include \"../../src/backend/fhe_types.hpp\"\n"
               "#include <cstddef>\n"
               "#include <cstdint>\n"
               "#include <iostream>\n"
               "#include <vector>\n\n"
               "using namespace CKKS;\n"
               "using namespace CGGI;\n\n";
        if (failed(mlir::emitc::translateToCpp(module.get(), outputFile->os()))) {
            llvm::errs() << "Error translating MLIR to C++\n";
            return 1;
        }
        if (!frontend::emitFheMain(outputFile->os(), *module)) {
            llvm::errs() << "Warning: unable to emit FHE main for output\n";
        }
    } else {
        module->print(outputFile->os());
    }
    outputFile->keep();

    return 0;
}
