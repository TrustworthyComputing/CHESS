#include "fhe_operations.hpp"
#include "fhe_types.hpp"
#include <openfhe.h>

using namespace lbcrypto;

namespace CGGI { 

CGGI_scheme::CGGI_scheme(CKKS::Context_CKKS ckcontext) {
    SecurityLevel sl      = lbcrypto::HEStd_128_classic;
    BINFHE_PARAMSET slBin = lbcrypto::STD128;
    uint32_t logQ_ccLWE   = 25;
    int batchSize = ckcontext.getBatchSize();

    SchSwchParams params;
    params.SetSecurityLevelCKKS(sl);
    params.SetSecurityLevelFHEW(slBin);
    // params.SetArbitraryFunctionEvaluation(true);
    params.SetCtxtModSizeFHEWLargePrec(logQ_ccLWE);
    params.SetNumSlotsCKKS(batchSize);
    params.SetNumValues(batchSize);
    auto privateKeyFHEW = ckcontext.getCryptoContext()->EvalSchemeSwitchingSetup(params);

    context.setKeys(privateKeyFHEW);

    context.setCryptoContext(ckcontext.getCryptoContext()->GetBinCCForSchemeSwitch());
    context.getCryptoContext()->BTKeyGen(privateKeyFHEW);
    ckcontext.getCryptoContext()->EvalSchemeSwitchingKeyGen(ckcontext.getKeys(), privateKeyFHEW);

    auto modulus_LWE     = 1 << logQ_ccLWE;
    auto beta            = context.getCryptoContext()->GetBeta().ConvertToInt();
    auto pLWE2           = modulus_LWE / (2 * beta);  // Large precision
    double scaleSignFHEW = 1.0;
    ckcontext.getCryptoContext()->EvalCompareSwitchPrecompute(pLWE2, scaleSignFHEW);
}

Context_CGGI CGGI_scheme::getContext(){
    return context;
}

FHEplaini FHEdecrypt(FHEcontext* ctx, FHEi32 a, int p){
    LWEPlaintext result;
    std::cout << "sag" << std::endl;
    ctx->getCGGI().getCryptoContext()->Decrypt(ctx->getCGGI().getKeys(), a.getCiphertext(),&result, p);
    std::cout << "lol" << std::endl;
    return FHEplaini(result);
}

FHEi32 FHEnot(FHEcontext* ctx, FHEi32 a){
    auto res = ctx->getCGGI().getCryptoContext()->EvalNOT(a.getCiphertext());
    return FHEi32(res);
}

FHEi32 FHEor(FHEcontext* ctx, FHEi32 a, FHEi32 b){
    auto res = ctx->getCGGI().getCryptoContext()->EvalBinGate(OR,a.getCiphertext(),b.getCiphertext());
    return FHEi32(res);

}

FHEi32 FHExor(FHEcontext* ctx, FHEi32 a, FHEi32 b){
    auto res = ctx->getCGGI().getCryptoContext()->EvalBinGate(XOR,a.getCiphertext(),b.getCiphertext());
    return FHEi32(res);
}

FHEi32 FHEand(FHEcontext* ctx, FHEi32 a, FHEi32 b){
    auto res = ctx->getCGGI().getCryptoContext()->EvalBinGate(AND,a.getCiphertext(),b.getCiphertext());
    return FHEi32(res);
}

FHEi32 FHExnor(FHEcontext* ctx, FHEi32 a, FHEi32 b){
    auto res = ctx->getCGGI().getCryptoContext()->EvalBinGate(XNOR,a.getCiphertext(),b.getCiphertext());
    return FHEi32(res);
}

    
    // Arithmetic Operations for FHEdouble
FHEi32 FHEaddi(FHEcontext* ctx, FHEi32 a, FHEi32 b) {
    ctx->getCGGI().getCryptoContext()->GetLWEScheme()->EvalAddEq(a.getCiphertext(), b.getCiphertext());
    return a;
}

FHEi32 FHEsubi(FHEcontext* ctx, FHEi32 a, FHEi32 b) {
    ctx->getCGGI().getCryptoContext()->GetLWEScheme()->EvalSubEq(a.getCiphertext(), b.getCiphertext());
    return a;
}

// Arithmetic Operations with Plaintext
FHEi32 FHEaddiP(FHEcontext* ctx, FHEi32 a, int b) {
    ctx->getCGGI().getCryptoContext()->GetLWEScheme()->EvalAddConstEq(a.getCiphertext(), b);
    return a;
}

FHEi32 FHEsubiP(FHEcontext* ctx, FHEi32 a, int b) {
    ctx->getCGGI().getCryptoContext()->GetLWEScheme()->EvalSubConstEq(a.getCiphertext(), b);
    return a;
}

FHEi32 FHEsubiP(FHEcontext* ctx, int b, FHEi32 a) {
    auto temp = ctx->getCGGI().getCryptoContext()->EvalNOT(a.getCiphertext());
    ctx->getCGGI().getCryptoContext()->GetLWEScheme()->EvalAddConstEq(temp, 1);
    ctx->getCGGI().getCryptoContext()->GetLWEScheme()->EvalAddConstEq(temp,b);
    return FHEi32(temp);
}

FHEi32 FHEmuliP(FHEcontext* ctx, FHEi32 a, int b) {
    ctx->getCGGI().getCryptoContext()->GetLWEScheme()->EvalMultConstEq(a.getCiphertext(), b);
    return a;
}

// FHEi32 FHEmuli(FHEi32 a, FHEi32 b) {
//     auto result = context->cc->EvalMult(a->getCiphertext(), b->getCiphertext());
//     return FHEi32(result);
// }

// FHEi32 FHEdivi(FHEi32 a, FHEi32 b) {
//     auto temp = context->cc->EvalDivide(b->getCiphertext(), 1.0, 4294967295.0, 10);
//     auto result = context->cc->EvalMult( FHEi32(temp))->getCiphertext(),a->getCiphertext());
//     return FHEi32(result);
// }

// FHEi32 FHEdiviP(FHEi32 a, int b) {
//     auto result = context->cc->EvalDivide(a->getCiphertext(), 1.0, 4294967295.0, 10);
//     return FHEi32(result);
// }

// FHEi32 FHEdiviP(int b, FHEi32 a) {
//     auto temp = context->cc->EvalDivide(a->getCiphertext(), 1.0, 4294967295.0, 10);
//     auto result = context->cc->EvalMult( FHEi32(temp))->getCiphertext(),b);
//     return FHEi32(result);
// }

}

// CGGI::FHEi32 FHEor(CGGI::FHEi32 a, CGGI::FHEi32 b){
//     return conFHEor(a,b);
// }

