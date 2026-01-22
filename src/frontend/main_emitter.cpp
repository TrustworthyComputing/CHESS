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

    bool wantsPlainCheck = (targetFunc.getName() == "min_index");
    bool hasArrayArgs = false;
    std::vector<unsigned> arrayArgIndices;
    for (const auto &arg : args) {
        if (arg.kind == ArgKind::kArray) {
            hasArrayArgs = true;
            arrayArgIndices.push_back(arg.index);
        }
    }

    if (wantsPlainCheck) {
        os << "\n#include \"../input/min_index.cpp\"\n";
    }

    os << "\nint main(int argc, char **argv) {\n";
    os << "    std::string security = \"secure\";\n";
    os << "    if (argc > 1) {\n";
    os << "        security = argv[1];\n";
    os << "    }\n";
    os << "    CKKS_scheme ck(15, 50, 16, security);\n";
    os << "    CGGI_scheme cg(ck.getContext(), security);\n";
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

    if (wantsPlainCheck && arrayArgIndices.size() >= 2) {
        unsigned inputIndex = arrayArgIndices[0];
        unsigned outputIndex = arrayArgIndices[1];
        os << "    int plain_input[kDefaultArraySize] = {};\n";
        os << "    int plain_output[kDefaultArraySize] = {};\n";
        os << "    for (size_t i = 0; i < kDefaultArraySize; ++i) {\n";
        os << "        plain_input[i] = static_cast<int>(arg" << inputIndex << "_plain[i]);\n";
        os << "    }\n";
        os << "    min_index(plain_input, plain_output);\n";
        os << "    bool ok = true;\n";
        os << "    for (size_t i = 0; i < kDefaultArraySize; ++i) {\n";
        os << "        FHEplainf out_plain = FHEdecrypt(ctx, arg" << outputIndex << "[i]);\n";
        os << "        auto packed = out_plain.getPlaintext()->GetRealPackedValue();\n";
        os << "        double value = packed.empty() ? 0.0 : packed[0];\n";
        os << "        int rounded = value >= 0.5 ? 1 : 0;\n";
        os << "        if (rounded != plain_output[i]) {\n";
        os << "            ok = false;\n";
        os << "            break;\n";
        os << "        }\n";
        os << "    }\n";
        os << "    std::cout << \"Plain min_index: \";\n";
        os << "    for (size_t i = 0; i < kDefaultArraySize; ++i) {\n";
        os << "        std::cout << plain_output[i];\n";
        os << "        if (i + 1 < kDefaultArraySize) {\n";
        os << "            std::cout << \" \";\n";
        os << "        }\n";
        os << "    }\n";
        os << "    std::cout << std::endl;\n";
        os << "    std::cout << \"FHE min_index ok: \" << (ok ? \"yes\" : \"no\") << std::endl;\n\n";
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
