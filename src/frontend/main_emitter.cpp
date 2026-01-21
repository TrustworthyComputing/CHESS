#include "main_emitter.hpp"

#include "mlir/Dialect/EmitC/IR/EmitC.h"
#include "llvm/ADT/StringRef.h"

#include <vector>

namespace {

enum class ArgKind {
    kScalar,
    kArray,
};

struct ArgInfo {
    ArgKind kind;
    unsigned index;
};

bool isOpaqueType(mlir::Type type, llvm::StringRef name) {
    if (auto opaque = type.dyn_cast<mlir::emitc::OpaqueType>()) {
        return opaque.getValue() == name;
    }
    return false;
}

bool isPointerToOpaque(mlir::Type type, llvm::StringRef name) {
    if (auto pointer = type.dyn_cast<mlir::emitc::PointerType>()) {
        return isOpaqueType(pointer.getPointee(), name);
    }
    return false;
}

}  // namespace

namespace frontend {

bool emitFheMain(llvm::raw_ostream &os, mlir::ModuleOp module) {
    mlir::func::FuncOp targetFunc;
    for (auto func : module.getOps<mlir::func::FuncOp>()) {
        if (func.isExternal()) {
            continue;
        }
        if (func.getName() == "main") {
            continue;
        }
        targetFunc = func;
        break;
    }

    if (!targetFunc) {
        return false;
    }

    auto funcType = targetFunc.getFunctionType();
    auto inputs = funcType.getInputs();
    if (inputs.empty() || !isPointerToOpaque(inputs.front(), "FHEcontext")) {
        return false;
    }

    std::vector<ArgInfo> args;
    for (unsigned i = 1; i < inputs.size(); ++i) {
        if (isOpaqueType(inputs[i], "FHEdouble")) {
            args.push_back({ArgKind::kScalar, i});
        } else if (isPointerToOpaque(inputs[i], "FHEdouble")) {
            args.push_back({ArgKind::kArray, i});
        } else {
            return false;
        }
    }

    bool returnsFheDouble = false;
    auto results = funcType.getResults();
    if (results.size() == 1 && isOpaqueType(results.front(), "FHEdouble")) {
        returnsFheDouble = true;
    }

    bool hasArrayArgs = false;
    for (const auto &arg : args) {
        if (arg.kind == ArgKind::kArray) {
            hasArrayArgs = true;
            break;
        }
    }

    os << "\nint main() {\n";
    os << "    CKKS_scheme ck(15, 50, 16);\n";
    os << "    CGGI_scheme cg(ck.getContext());\n";
    os << "    FHEcontext* ctx = new FHEcontext(ck.getContext(), cg.getContext());\n\n";

    if (hasArrayArgs) {
        os << "    constexpr size_t kDefaultArraySize = 10;\n\n";
    }

    for (const auto &arg : args) {
        if (arg.kind == ArgKind::kScalar) {
            os << "    double arg" << arg.index << "_plain = 1.0;\n";
            os << "    FHEdouble arg" << arg.index << " = FHEencrypt(ctx, arg" << arg.index << "_plain);\n\n";
        } else {
            os << "    const size_t arg" << arg.index << "_size = kDefaultArraySize;\n";
            os << "    std::vector<double> arg" << arg.index << "_plain(arg" << arg.index << "_size);\n";
            os << "    for (size_t i = 0; i < arg" << arg.index << "_plain.size(); ++i) {\n";
            os << "        arg" << arg.index << "_plain[i] = static_cast<double>(i);\n";
            os << "    }\n";
            os << "    std::vector<FHEdouble> arg" << arg.index << "(arg" << arg.index << "_size);\n";
            os << "    for (size_t i = 0; i < arg" << arg.index << ".size(); ++i) {\n";
            os << "        arg" << arg.index << "[i] = FHEencrypt(ctx, arg" << arg.index << "_plain[i]);\n";
            os << "    }\n\n";
        }
    }

    os << "    ";
    if (returnsFheDouble) {
        os << "FHEdouble result = ";
    }
    os << targetFunc.getName() << "(ctx";
    for (const auto &arg : args) {
        if (arg.kind == ArgKind::kArray) {
            os << ", arg" << arg.index << ".data()";
        } else {
            os << ", arg" << arg.index;
        }
    }
    os << ");\n\n";

    if (returnsFheDouble) {
        os << "    FHEplainf result_plain = FHEdecrypt(ctx, result);\n";
        os << "    std::cout << \"Result: \" << result_plain.getPlaintext() << std::endl;\n\n";
    }

    for (const auto &arg : args) {
        if (arg.kind != ArgKind::kArray) {
            continue;
        }
        os << "    std::cout << \"arg" << arg.index << ": \";\n";
        os << "    for (size_t i = 0; i < arg" << arg.index << ".size(); ++i) {\n";
        os << "        FHEplainf out_plain = FHEdecrypt(ctx, arg" << arg.index << "[i]);\n";
        os << "        std::cout << out_plain.getPlaintext();\n";
        os << "        if (i + 1 < arg" << arg.index << ".size()) {\n";
        os << "            std::cout << \" \";\n";
        os << "        }\n";
        os << "    }\n";
        os << "    std::cout << std::endl;\n\n";
    }

    os << "    return 0;\n";
    os << "}\n";

    return true;
}

}  // namespace frontend
