#include "fhe_operations.hpp"
#include "fhe_types.hpp"
#include <openfhe.h>
#include <iostream>

using namespace lbcrypto;

namespace CKKS {

CKKS_scheme::CKKS_scheme(int multDepth, int scaleModSize, int batchSize)
    : multDepth(multDepth), scaleModSize(scaleModSize), batchSize(batchSize) {

    

    // uint32_t scaleModSize = 50;
    uint32_t firstModSize = 60;
    SecurityLevel sl      = lbcrypto::HEStd_128_classic;
    // BINFHE_PARAMSET slBin = TOY;
    // uint32_t logQ_ccLWE   = 25;
    // uint32_t slots        = 1;  
    // uint32_t batchSize    = slots;

    ScalingTechnique scTech = lbcrypto::FIXEDAUTO;
    // uint32_t multDepth      = 17;

    CCParams<CryptoContextCKKSRNS> parameters;
    parameters.SetMultiplicativeDepth(multDepth);
    parameters.SetScalingModSize(scaleModSize);
    parameters.SetFirstModSize(firstModSize);
    parameters.SetScalingTechnique(scTech);
    parameters.SetSecurityLevel(sl);
    parameters.SetBatchSize(batchSize);
    // parameters.SetSecretKeyDist(UNIFORM_TERNARY);
    // parameters.SetKeySwitchTechnique(HYBRID);
    // parameters.SetNumLargeDigits(3);

    context.cc = GenCryptoContext(parameters);
    context.setBatchSize(batchSize);

    context.getCryptoContext()->Enable(PKE);
    context.getCryptoContext()->Enable(KEYSWITCH);
    context.getCryptoContext()->Enable(LEVELEDSHE);
    context.getCryptoContext()->Enable(ADVANCEDSHE);
    context.getCryptoContext()->Enable(SCHEMESWITCH);

    std::cout << "CKKS scheme is using ring dimension " << context.getCryptoContext()->GetRingDimension() << "\n";


    context.setKeys(context.getCryptoContext()->KeyGen());
    // context->cc->EvalMultKeyGen(keys->keyPair.secretKey);
    // context->cc->EvalRotateKeyGen(keys->keyPair.secretKey, {1, -2});


}

Context_CKKS CKKS_scheme::getContext(){
    return context;
}


FHEplainf FHEencode(FHEcontext* ctx, const std::vector<double>& a){
    FHEplainf ptxt = ctx->getCKKS().getCryptoContext()->MakeCKKSPackedPlaintext(a);
    return FHEplainf(ptxt);
}

FHEdouble FHEencrypt(FHEcontext* ctx, const FHEplainf a){
    auto result = ctx->getCKKS().getCryptoContext()->Encrypt(ctx->getCKKS().getKeys().publicKey, a.getPlaintext());
    return FHEdouble(result);
}

FHEdouble FHEencrypt(FHEcontext* ctx, const std::vector<double>& a){
    return FHEencrypt(ctx, FHEencode(ctx, a));
}

CKKS::FHEdouble FHEencrypt(FHEcontext* ctx, const double a){
    return FHEencrypt(ctx, FHEencode(ctx, {a}));
}

FHEplainf FHEdecrypt(FHEcontext* ctx, const FHEdouble a){
    Plaintext result;
    ctx->getCKKS().getCryptoContext()->Decrypt(ctx->getCKKS().getKeys().secretKey, a.getCiphertext(), &result);
    return FHEplainf(result);
}

// Arithmetic Operations for FHEdouble
FHEdouble FHEaddf(FHEcontext* ctx, const FHEdouble a, const FHEdouble b) {
    auto result = ctx->getCKKS().getCryptoContext()->EvalAdd(a.getCiphertext(), b.getCiphertext());
    return FHEdouble(result);
}

FHEdouble FHEsubf(FHEcontext* ctx, const FHEdouble a, const FHEdouble b) {
    auto result = ctx->getCKKS().getCryptoContext()->EvalSub(a.getCiphertext(), b.getCiphertext());
    return FHEdouble(result);
}

FHEdouble FHEmulf(FHEcontext* ctx, const FHEdouble a, const FHEdouble b) {
    auto result = ctx->getCKKS().getCryptoContext()->EvalMult(a.getCiphertext(), b.getCiphertext());
    return FHEdouble(result);
}

FHEdouble FHEdivf(FHEcontext* ctx, const FHEdouble a, const FHEdouble b) {
    auto temp = ctx->getCKKS().getCryptoContext()->EvalDivide(b.getCiphertext(), 1.0, 4294967295.0, 10);
    auto result = ctx->getCKKS().getCryptoContext()->EvalMult(temp,a.getCiphertext());
    return FHEdouble(result);
}

// Arithmetic Operations with Plaintext
FHEdouble FHEaddfP(FHEcontext* ctx, const FHEdouble a, double b) {
    auto result = ctx->getCKKS().getCryptoContext()->EvalAdd(a.getCiphertext(), b);
    return FHEdouble(result);
}

FHEdouble FHEsubfP(FHEcontext* ctx, const FHEdouble a, double b) {
    auto result = ctx->getCKKS().getCryptoContext()->EvalSub(a.getCiphertext(), b);
    return FHEdouble(result);
}

FHEdouble FHEsubfP(FHEcontext* ctx, double b, const FHEdouble a) {
    auto result = ctx->getCKKS().getCryptoContext()->EvalSub(b, a.getCiphertext());
    return FHEdouble(result);
}

FHEdouble FHEmulfP(FHEcontext* ctx, const FHEdouble a, double b) {
    auto result = ctx->getCKKS().getCryptoContext()->EvalMult(a.getCiphertext(), b);
    return FHEdouble(result);
}

FHEdouble FHEdivfP(FHEcontext* ctx, const FHEdouble a, double b) {
    auto result = ctx->getCKKS().getCryptoContext()->EvalDivide(a.getCiphertext(), 1.0, 4294967295.0, 10);
    return FHEdouble(result);
}

FHEdouble FHEdivfP(FHEcontext* ctx, double b, const FHEdouble a) {
    auto temp = ctx->getCKKS().getCryptoContext()->EvalDivide(a.getCiphertext(), 1.0, 4294967295.0, 10);
    auto result = ctx->getCKKS().getCryptoContext()->EvalMult(temp, b);
    return FHEdouble(result);
}

FHEdouble FHErotate(FHEcontext* ctx, CKKS::FHEdouble a, int n){
    auto result = ctx->getCKKS().getCryptoContext()->EvalRotate(a.getCiphertext(), n);
    return CKKS::FHEdouble(result);
}

FHEdouble FHEbroadcast(FHEcontext* ctx, CKKS::FHEdouble a){
    int slots = ctx->getCKKS().getBatchSize();
    std::vector<double> mask(slots, 0);
    mask[0] = 1;
    auto maskctxt = CKKS::FHEencrypt(ctx,mask);
    auto valuectxt = CKKS::FHEmulf(ctx, a, maskctxt);
    int shift = 1;
    while (shift < slots) {
        auto rotatedctxt = ctx->getCKKS().getCryptoContext()->EvalRotate(valuectxt.getCiphertext(), shift);
        valuectxt = CKKS::FHEdouble(ctx->getCKKS().getCryptoContext()->EvalAdd(valuectxt.getCiphertext(), rotatedctxt));
        shift *= 2;
    }
    return valuectxt;
}

FHEdouble FHEvectorSum(FHEcontext* ctx, CKKS::FHEdouble a){
    int shift = ctx->getCKKS().getBatchSize()/2;
    while (shift > 0) {
        auto rotatedctxt = ctx->getCKKS().getCryptoContext()->EvalRotate(a.getCiphertext(), shift);
        a = CKKS::FHEdouble(ctx->getCKKS().getCryptoContext()->EvalAdd(a.getCiphertext(), rotatedctxt));
        shift /= 2;
    }
    return a;
}

}

std::vector<CGGI::FHEi32> CKKStoCGGI(FHEcontext* ctx, CKKS::FHEdouble a) {
    auto LWECiphertexts = ctx->getCKKS().getCryptoContext()->EvalCKKStoFHEW(a.getCiphertext(), ctx->getCKKS().getBatchSize());
    std::vector<CGGI::FHEi32> result;

    // Assuming LWECiphertexts is a vector of LWECiphertext
    for (auto& lweCipher : LWECiphertexts) {
        // Create a FHEi32 object for each LWECiphertext and store in the result vector
        result.push_back(CGGI::FHEi32(lweCipher));  
    }

    // Return the vector of FHEi32 pointers
    return result;

}

CKKS::FHEdouble CGGItoCKKS(FHEcontext* ctx, std::vector<CGGI::FHEi32> a) {
    std::vector<lbcrypto::LWECiphertext> lweCiphertexts;
    // Unpack the vector of CGGI::FHEi32 to LWECiphertext
    for (auto& fhei32 : a) {
        lweCiphertexts.push_back(fhei32.getCiphertext());
    }
    int batchSize = ctx->getCKKS().getBatchSize();
    auto ctxt = ctx->getCKKS().getCryptoContext()->EvalFHEWtoCKKS(lweCiphertexts,batchSize,batchSize);

    return CKKS::FHEdouble(ctxt);

}

std::vector<CGGI::FHEi32> FHEsign(FHEcontext* ctx, std::vector<CGGI::FHEi32> lwes){
    std::vector<CGGI::FHEi32> result;
    for (auto& lwe : lwes) {
        auto temp = ctx->getCGGI().getCryptoContext()->EvalSign(lwe.getCiphertext());
        result.push_back(CGGI::FHEi32(temp));
    }
    return result;
}


CKKS::FHEdouble FHEeq(FHEcontext* ctx, CKKS::FHEdouble a, CKKS::FHEdouble b){
    auto dif = FHEsubf(ctx, a,b);
    auto lwes = CKKStoCGGI(ctx, dif);
    auto sign = FHEsign(ctx, lwes);

    auto dif_prime = FHEsubf(ctx, b,a);
    auto lwes_prime = CKKStoCGGI(ctx, dif_prime);
    auto sign_prime = FHEsign(ctx, lwes_prime);

    std::vector<CGGI::FHEi32> ors;
    for (size_t i = 0; i < sign.size(); i++){
        ors.push_back(CGGI::FHEor(ctx, sign[i],sign_prime[i]));
        ors[i] = CGGI::FHEnot(ctx, ors[i]);
    }
    return CGGItoCKKS(ctx, ors);
    

}

CKKS::FHEdouble FHEselect(FHEcontext* ctx, CKKS::FHEdouble sign, CKKS::FHEdouble value1, CKKS::FHEdouble value2){
    // auto sign_prime = FHEsubfP(ctx, 1.0,sign);
    // return FHEaddf(ctx, FHEmulf(ctx, sign,value1),FHEmulf(ctx, sign_prime,value2));
    return FHEaddf(ctx, FHEmulf(ctx, sign, CKKS::FHEsubf(ctx,value1,value2)),value2);
}
