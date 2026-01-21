#include "binfhe-constants.h"
#include "lattice/stdlatticeparms.h"
#include "lwe-ciphertext-fwd.h"
#include "math/hal/nativeintbackend.h"
#include "openfhe.h"
#include "binfhecontext.h"
#include <chrono>
#include <iostream>
#include <random>
// #include <omp.h>

using namespace lbcrypto;
using namespace std;

void min_index(){
    
}



void SwitchFHEWtoCKKS() {
    std::cout << "\n-----SwitchFHEWtoCKKS-----\n" << std::endl;
    std::cout << "Output precision is only wrt the operations in CKKS after switching back.\n" << std::endl;

    // Step 1: Setup CryptoContext for CKKS to be switched into

    // A. Specify main parameters
    ScalingTechnique scTech = FIXEDAUTO;
    // for r = 3 in FHEWtoCKKS, Chebyshev max depth allowed is 9, 1 more level for postscaling
    uint32_t multDepth = 3 + 9 + 1;
    if (scTech == FLEXIBLEAUTOEXT)
        multDepth += 1;
    uint32_t scaleModSize = 50;
    uint32_t ringDim      = 8192;
    SecurityLevel sl      = HEStd_NotSet;  // If this is not HEStd_NotSet, ensure ringDim is compatible
    uint32_t logQ_ccLWE   = 28;

    // uint32_t slots = ringDim/2; // Uncomment for fully-packed
    uint32_t slots     = 16;  // sparsely-packed
    uint32_t batchSize = slots;

    CCParams<CryptoContextCKKSRNS> parameters;
    parameters.SetMultiplicativeDepth(multDepth);
    parameters.SetScalingModSize(scaleModSize);
    parameters.SetScalingTechnique(scTech);
    parameters.SetSecurityLevel(sl);
    parameters.SetRingDim(ringDim);
    parameters.SetBatchSize(batchSize);

    CryptoContext<DCRTPoly> cc = GenCryptoContext(parameters);

    // Enable the features that you wish to use
    cc->Enable(PKE);
    cc->Enable(KEYSWITCH);
    cc->Enable(LEVELEDSHE);
    cc->Enable(ADVANCEDSHE);
    cc->Enable(SCHEMESWITCH);

    std::cout << "CKKS scheme is using ring dimension " << cc->GetRingDimension();
    std::cout << ", number of slots " << slots << ", and supports a multiplicative depth of " << multDepth << std::endl
              << std::endl;

    // Generate encryption keys.
    auto keys = cc->KeyGen();

    // Step 2: Prepare the FHEW cryptocontext and keys for FHEW and scheme switching
    auto ccLWE = std::make_shared<BinFHEContext>();
    ccLWE->BinFHEContext::GenerateBinFHEContext(TOY, false, logQ_ccLWE, 0, GINX, false);

    // LWE private key
    LWEPrivateKey lwesk;
    lwesk = ccLWE->KeyGen();

    std::cout << "FHEW scheme is using lattice parameter " << ccLWE->GetParams()->GetLWEParams()->Getn();
    std::cout << ", logQ " << logQ_ccLWE;
    std::cout << ", and modulus q " << ccLWE->GetParams()->GetLWEParams()->Getq() << std::endl << std::endl;

    // Step 3. Precompute the necessary keys and information for switching from FHEW to CKKS
    cc->EvalFHEWtoCKKSSetup(ccLWE, slots, logQ_ccLWE);
    cc->SetBinCCForSchemeSwitch(ccLWE);

    cc->EvalFHEWtoCKKSKeyGen(keys, lwesk);

    // Step 4: Encoding and encryption of inputs
    // For correct CKKS decryption, the messages have to be much smaller than the FHEW plaintext modulus!

    auto pLWE1       = ccLWE->GetMaxPlaintextSpace().ConvertToInt();  // Small precision
    uint32_t pLWE2   = 256;                                           // Medium precision
    auto modulus_LWE = 1 << logQ_ccLWE;
    auto beta        = ccLWE->GetBeta().ConvertToInt();
    auto pLWE3       = modulus_LWE / (2 * beta);  // Large precision
    // Inputs
    std::vector<int> x1 = {1, 2, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0};
    std::vector<int> x2 = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 64};
    if (x1.size() < slots) {
        std::vector<int> zeros(slots - x1.size(), 0);
        x1.insert(x1.end(), zeros.begin(), zeros.end());
        x2.insert(x2.end(), zeros.begin(), zeros.end());
    }

    // Encrypt
    std::vector<LWECiphertext> ctxtsLWE1(slots);
    for (uint32_t i = 0; i < slots; i++) {
        // encrypted under small plantext modulus p = 4 and ciphertext modulus
        ctxtsLWE1[i] = ccLWE->Encrypt(lwesk, x1[i]);
    }

    std::vector<LWECiphertext> ctxtsLWE2(slots);
    for (uint32_t i = 0; i < slots; i++) {
        // encrypted under larger plaintext modulus p = 16 but small ciphertext modulus
        ctxtsLWE2[i] = ccLWE->Encrypt(lwesk, x1[i], LARGE_DIM, pLWE1);
    }

    std::vector<LWECiphertext> ctxtsLWE3(slots);
    for (uint32_t i = 0; i < slots; i++) {
        // encrypted under larger plaintext modulus and large ciphertext modulus
        ctxtsLWE3[i] = ccLWE->Encrypt(lwesk, x2[i], LARGE_DIM, pLWE2, modulus_LWE);
    }

    std::vector<LWECiphertext> ctxtsLWE4(slots);
    for (uint32_t i = 0; i < slots; i++) {
        // encrypted under large plaintext modulus and large ciphertext modulus
        ctxtsLWE4[i] = ccLWE->Encrypt(lwesk, x2[i], LARGE_DIM, pLWE3, modulus_LWE);
    }

    // Step 5. Perform the scheme switching
    auto cTemp = cc->EvalFHEWtoCKKS(ctxtsLWE1, slots, slots);

    std::cout << "\n---Input x1: " << x1 << " encrypted under p = " << 4 << " and Q = " << ctxtsLWE1[0]->GetModulus()
              << "---" << std::endl;

    // Step 6. Decrypt
    Plaintext plaintextDec;
    cc->Decrypt(keys.secretKey, cTemp, &plaintextDec);
    plaintextDec->SetLength(slots);
    std::cout << "Switched CKKS decryption 1: " << plaintextDec << std::endl;

    // Step 5'. Perform the scheme switching
    cTemp = cc->EvalFHEWtoCKKS(ctxtsLWE2, slots, slots, pLWE1, 0, pLWE1);

    std::cout << "\n---Input x1: " << x1 << " encrypted under p = " << NativeInteger(pLWE1)
              << " and Q = " << ctxtsLWE2[0]->GetModulus() << "---" << std::endl;

    // Step 6'. Decrypt
    cc->Decrypt(keys.secretKey, cTemp, &plaintextDec);
    plaintextDec->SetLength(slots);
    std::cout << "Switched CKKS decryption 2: " << plaintextDec << std::endl;

    // Step 5''. Perform the scheme switching
    cTemp = cc->EvalFHEWtoCKKS(ctxtsLWE3, slots, slots, pLWE2, 0, pLWE2);

    std::cout << "\n---Input x2: " << x2 << " encrypted under p = " << pLWE2
              << " and Q = " << ctxtsLWE3[0]->GetModulus() << "---" << std::endl;

    // Step 6''. Decrypt
    cc->Decrypt(keys.secretKey, cTemp, &plaintextDec);
    plaintextDec->SetLength(slots);
    std::cout << "Switched CKKS decryption 3: " << plaintextDec << std::endl;

    // Step 5'''. Perform the scheme switching
    std::setprecision(logQ_ccLWE + 10);
    auto cTemp2 = cc->EvalFHEWtoCKKS(ctxtsLWE4, slots, slots, pLWE3, 0, pLWE3);

    std::cout << "\n---Input x2: " << x2 << " encrypted under p = " << NativeInteger(pLWE3)
              << " and Q = " << ctxtsLWE4[0]->GetModulus() << "---" << std::endl;

    // Step 6'''. Decrypt
    Plaintext plaintextDec2;
    cc->Decrypt(keys.secretKey, cTemp2, &plaintextDec2);
    plaintextDec2->SetLength(slots);
    std::cout << "Switched CKKS decryption 4: " << plaintextDec2 << std::endl;
}

void sag(){
    // Sample Program: Step 1: Set CryptoContext

    auto cc = BinFHEContext();

    // Set the ciphertext modulus to be 1 << 17
    // Note that normally we do not use this way to obtain the input ciphertext.
    // Instead, we assume that an LWE ciphertext with large ciphertext
    // modulus is already provided (e.g., by extracting from a CKKS ciphertext).
    // However, we do not provide such a step in this example.
    // Therefore, we use a brute force way to create a large LWE ciphertext.

    cc.GenerateBinFHEContext(TOY, GINX);

    auto sk = cc.KeyGen();

    cout << "Generating the bootstrapping keys..." << endl;

    // Generate the bootstrapping keys (refresh and switching keys)
    cc.BTKeyGen(sk);

    cout << "Completed the key generation." << endl;

    // Sample Program: Step 3: Extract the MSB and decrypt to check the result
    // Note that we check for 8 different numbers


        // Get the MSB

    // auto p          = 6;
    auto a = cc.Encrypt(sk, 3,lbcrypto::FRESH, 1 << 4);
    auto b = cc.Encrypt(sk, 2,lbcrypto::FRESH, 1 << 4);
    auto c = cc.Encrypt(sk, 0,lbcrypto::FRESH, 1 << 4);
    // cc.GetLWEScheme()->EvalSubEq(a,b);
    // auto s = cc.EvalSign(a);
    cc.GetLWEScheme()->EvalAddEq(a,b);
    // vector<LWECiphertext> ctxts;
    // ctxts.push_back(a);
    // ctxts.push_back(b);
    // ctxts.push_back(c);
    // auto ctres = cc.EvalBinGate(CMUX, ctxts);
    LWEPlaintext result;
    cc.Decrypt(sk, a, &result, 1 << 8);
    cout << "Result: " << result << endl;

}

void cmux() {
    // Sample Program: Step 1: Set CryptoContext

    auto cc = BinFHEContext();

    cc.GenerateBinFHEContext(STD128_4, GINX);

    // Sample Program: Step 2: Key Generation

    // Generate the secret key
    auto sk = cc.KeyGen();

    cout << "Generating the bootstrapping keys..." << endl;

    // Generate the bootstrapping keys (refresh and switching keys)
    cc.BTKeyGen(sk);

    cout << "Completed the key generation." << endl;

    // Sample Program: Step 3: Encryption

    // Encrypt several ciphertexts representing Boolean True (1) or False (0).
    // plaintext modulus is set higher than 4 to 2 * num_of_inputs
    auto p          = 6;
    auto ct1_3input = cc.Encrypt(sk, 1, SMALL_DIM, p);
    auto ct2_3input = cc.Encrypt(sk, 1, SMALL_DIM, p);
    auto ct3_3input = cc.Encrypt(sk, 0, SMALL_DIM, p);

    // 1, 1, 0
    vector<LWECiphertext> ct123;
    ct123.push_back(ct1_3input);
    ct123.push_back(ct2_3input);
    ct123.push_back(ct3_3input);

    // 1, 1, 0
    auto ctAND3 = cc.EvalBinGate(AND3, ct123);

    // 1, 1, 0
    auto ctOR3 = cc.EvalBinGate(OR3, ct123);
    // Sample Program: Step 5: Decryption

    LWEPlaintext result;
    cc.Decrypt(sk, ctAND3, &result, p);
    if (result != 0)
        OPENFHE_THROW("Decryption failure");

    cout << "Result of encrypted computation of AND(1, 1, 0) = " << result << endl;

    cc.Decrypt(sk, ctOR3, &result, p);
    if (result != 1)
        OPENFHE_THROW("Decryption failure");

    cout << "Result of encrypted computation of OR(1, 1, 0) = " << result << endl;

    // majority gate and cmux for 3 input does not need higher plaintext modulus
    p                  = 4;
    auto ct1_3input_p4 = cc.Encrypt(sk, 1, lbcrypto::FRESH, p);
    auto ct2_3input_p4 = cc.Encrypt(sk, 1, lbcrypto::FRESH, p);
    auto ct3_3input_p4 = cc.Encrypt(sk, 0, lbcrypto::FRESH, p);
    auto ct4_3input_p4 = cc.Encrypt(sk, 0, lbcrypto::FRESH, p);

    // 1, 1, 0
    vector<LWECiphertext> ct123_p4;
    ct123_p4.push_back(ct1_3input_p4);
    ct123_p4.push_back(ct2_3input_p4);
    ct123_p4.push_back(ct3_3input_p4);

    // 1, 0, 0
    vector<LWECiphertext> ct134_p4;
    ct134_p4.push_back(ct1_3input_p4);
    ct134_p4.push_back(ct3_3input_p4);
    ct134_p4.push_back(ct4_3input_p4);

    // 1, 0, 1
    vector<LWECiphertext> ct132_p4;
    ct132_p4.push_back(ct1_3input_p4);
    ct132_p4.push_back(ct3_3input_p4);
    ct132_p4.push_back(ct2_3input_p4);

    // 1, 1, 0
    auto ctMajority = cc.EvalBinGate(MAJORITY, ct123_p4);

    // 1, 0, 1
    auto ctCMUX0 = cc.EvalBinGate(CMUX, ct132_p4);

    // 1, 0, 0
    auto ctCMUX1 = cc.EvalBinGate(CMUX, ct134_p4);

    cc.Decrypt(sk, ctMajority, &result);
    if (result != 1)
        OPENFHE_THROW("Decryption failure");

    cout << "Result of encrypted computation of Majority(1, 1, 0) = " << result << endl;

    cc.Decrypt(sk, ctCMUX1, &result);
    if (result != 1)
        OPENFHE_THROW("Decryption failure");
    cout << "Result of encrypted computation of CMUX(1, 0, 0) = " << result << endl;

    cc.Decrypt(sk, ctCMUX0, &result);
    if (result != 0)
        OPENFHE_THROW("Decryption failure");

    cout << "Result of encrypted computation of CMUX(1, 0, 1) = " << result << endl;

    // for 4 input gates
    p               = 8;
    auto ct1_4input = cc.Encrypt(sk, 1, SMALL_DIM, p);
    auto ct2_4input = cc.Encrypt(sk, 0, SMALL_DIM, p);
    auto ct3_4input = cc.Encrypt(sk, 0, SMALL_DIM, p);
    auto ct4_4input = cc.Encrypt(sk, 0, SMALL_DIM, p);

    // 1, 0, 0, 0
    vector<LWECiphertext> ct1234;
    ct1234.push_back(ct1_4input);
    ct1234.push_back(ct2_4input);
    ct1234.push_back(ct3_4input);
    ct1234.push_back(ct4_4input);

    // Sample Program: Step 4: Evaluation

    // 1, 0, 0, 0
    auto ctAND4 = cc.EvalBinGate(AND4, ct1234);

    // 1, 0, 0, 0
    auto ctOR4 = cc.EvalBinGate(OR4, ct1234);

    // Sample Program: Step 5: Decryption
    cc.Decrypt(sk, ctAND4, &result, p);
    if (result != 0)
        OPENFHE_THROW("Decryption failure");

    cout << "Result of encrypted computation of AND(1, 0, 0, 0) = " << result << endl;

    cc.Decrypt(sk, ctOR4, &result, p);
    if (result != 1)
        OPENFHE_THROW("Decryption failure");

    cout << "Result of encrypted computation of OR(1, 0, 0, 0) = " << result << endl;

}

void SwitchCKKSToFHEW() {

    std::cout << "\n-----SwitchCKKSToFHEW-----\n" << std::endl;

    // Step 1: Setup CryptoContext for CKKS

    // Specify main parameters
    uint32_t multDepth    = 3;
    uint32_t firstModSize = 60;
    uint32_t scaleModSize = 50;
    uint32_t ringDim      = 4096;
    SecurityLevel sl      = HEStd_NotSet;
    BINFHE_PARAMSET slBin = TOY;
    uint32_t logQ_ccLWE   = 26;
    // uint32_t slots        = ringDim / 2;  // Uncomment for fully-packed
    uint32_t slots     = 16;  // sparsely-packed
    uint32_t batchSize = slots;

    CCParams<CryptoContextCKKSRNS> parameters;

    parameters.SetMultiplicativeDepth(multDepth);
    parameters.SetFirstModSize(firstModSize);
    parameters.SetScalingModSize(scaleModSize);
    parameters.SetScalingTechnique(FLEXIBLEAUTOEXT);
    parameters.SetSecurityLevel(sl);
    parameters.SetRingDim(ringDim);
    parameters.SetBatchSize(batchSize);

    CryptoContext<DCRTPoly> cc = GenCryptoContext(parameters);

    // Enable the features that you wish to use
    cc->Enable(PKE);
    cc->Enable(KEYSWITCH);
    cc->Enable(LEVELEDSHE);
    cc->Enable(SCHEMESWITCH);

    std::cout << "CKKS scheme is using ring dimension " << cc->GetRingDimension();
    std::cout << ", number of slots " << slots << ", and supports a multiplicative depth of " << multDepth << std::endl
              << std::endl;

    // Generate encryption keys
    auto keys = cc->KeyGen();

    // Step 2: Prepare the FHEW cryptocontext and keys for FHEW and scheme switching
    SchSwchParams params;
    params.SetSecurityLevelCKKS(sl);
    params.SetSecurityLevelFHEW(slBin);
    params.SetCtxtModSizeFHEWLargePrec(logQ_ccLWE);
    params.SetNumSlotsCKKS(slots);
    // params.SetArbitraryFunctionEvaluation(true);
    // params.GetArbitraryFunctionEvaluation();
    auto privateKeyFHEW = cc->EvalCKKStoFHEWSetup(params);


    auto ccLWE = std::make_shared<lbcrypto::BinFHEContext>();
    auto logQ = 27;
    ccLWE->GenerateBinFHEContext(TOY, true, logQ,GINX);
    cc->SetBinCCForSchemeSwitch(ccLWE);
    // auto ccLWE          = cc->GetBinCCForSchemeSwitch();
    cc->EvalCKKStoFHEWKeyGen(keys, privateKeyFHEW);
    

    std::cout << "FHEW scheme is using lattice parameter " << ccLWE->GetParams()->GetLWEParams()->Getn();
    std::cout << ", logQ " << logQ_ccLWE;
    std::cout << ", and modulus q " << ccLWE->GetParams()->GetLWEParams()->Getq() << std::endl << std::endl;

    // Compute the scaling factor to decrypt correctly in FHEW; under the hood, the LWE mod switch will performed on the ciphertext at the last level
    auto pLWE1       = ccLWE->GetMaxPlaintextSpace().ConvertToInt();  // Small precision
    std::cout << "pLWE1: " << pLWE1 << std::endl;
    auto modulus_LWE = 1 << logQ_ccLWE;
    auto beta        = ccLWE->GetBeta().ConvertToInt();
    auto pLWE2       = modulus_LWE / (2 * beta);  // Large precision
    std::cout << "pLWE2: " << pLWE2 << std::endl;
    double scale1 = 1.0 / pLWE1;
    double scale2 = 1.0 / pLWE2;

    // Perform the precomputation for switching
    cc->EvalCKKStoFHEWPrecompute(scale1);
    

    // Step 3: Encoding and encryption of inputs

    // Inputs
    std::vector<double> x2  = {5,6};

    uint32_t encodedLength2 = x2.size();

    Plaintext ptxt2 = cc->MakeCKKSPackedPlaintext(x2, 1, 0, nullptr);

    auto c2 = cc->Encrypt(keys.publicKey, ptxt2);

    cc->EvalCKKStoFHEWPrecompute(scale2);

    // Transform the ciphertext from CKKS to FHEW (only for the number of inputs given)
    auto cTemp2 = cc->EvalCKKStoFHEW(c2, encodedLength2);

    std::cout << "\n---Decrypting switched ciphertext with large precision (plaintext modulus " << NativeInteger(pLWE2)
              << ")---\n"
              << std::endl;

    ptxt2->SetLength(encodedLength2);
    LWEPlaintext result;
    std::cout << "Input x2: " << ptxt2->GetRealPackedValue() << std::endl;
    std::cout << "FHEW decryption: ";
    for (uint32_t i = 0; i < cTemp2.size(); ++i) {
        ccLWE->Decrypt(privateKeyFHEW, cTemp2[i], &result, pLWE2);
        std::cout << result << " ";
    }
    std::cout << "\n" << std::endl;


    // Generate the bootstrapping keys (refresh and switching keys)
    ccLWE->BTKeyGen(privateKeyFHEW);


    auto modtemp = cTemp2[0]->GetModulus();
    std::cout << "modtemp: " << modtemp << std::endl;
    auto qtemp = ccLWE->GetParams()->GetLWEParams()->Getq();
    std::cout << "qtemp: " << qtemp << std::endl;

    auto dec1 = ccLWE->EvalDecomp(cTemp2[0]);
    auto dec2 = ccLWE->EvalDecomp(cTemp2[1]);
    // cc->EvalFHEWtoCKKS(std::vector<std::shared_ptr<LWECiphertextImpl>> &LWECiphertexts)




    auto saglength = dec1[0]->GetLength();
    auto qmod = dec1[0]->GetModulus();
    auto pmod = dec1[0]->GetptModulus();
    auto N = ccLWE->GetParams()->GetLWEParams()->GetN();
    std::cout << "N: " << N << std::endl;   
    std::cout << "pmod: " << pmod << std::endl;
    std::cout << "qmod: " << qmod << std::endl;
    std::cout << "saglengths: " << saglength << std::endl;


    int p = ccLWE->GetMaxPlaintextSpace().ConvertToInt();  // Obtain the maximum plaintext space
    std::cout << "p: " << p << std::endl;


    



    // Initialize Function f(x) = x^3 % p
    auto fp = [](NativeInteger m, NativeInteger p1) -> NativeInteger {
        auto l = (m>>1)%2;
        auto r = (m)%2;
        return NativeInteger(l.ConvertToInt() & r.ConvertToInt());
    };

    auto d1 = dec1[0];
    auto d2 = dec2[0];
    ccLWE->GetLWEScheme()->EvalMultConstEq(d1,2);
    ccLWE->GetLWEScheme()->EvalAddEq(d1,d2);

    


    auto lut = ccLWE->GenerateLUTviaFunction(fp, p);
    auto ctfunc1 = ccLWE->EvalFunc(d1, lut);

    LWEPlaintext resultDecomp;
    std::cout << "decryption eval func: ";
    ccLWE->Decrypt(privateKeyFHEW, ctfunc1, &resultDecomp, p);
    std::cout << resultDecomp << std::endl;

    auto d3 = ccLWE->EvalBinGate(XOR,d1,d2);
    LWEPlaintext resultand;
    std::cout << "decryption and: ";
    ccLWE->Decrypt(privateKeyFHEW, d3, &resultand, p);
    std::cout << resultand << std::endl;


    

    for (uint32_t j = 0; j < cTemp2.size(); j++) {
        // Decompose the large ciphertext into small ciphertexts that fit in q
        auto decomp = ccLWE->EvalDecomp(cTemp2[j]);

        // Decryption
        auto p = ccLWE->GetMaxPlaintextSpace().ConvertToInt();
        std::cout << "p: " << p << std::endl;
        LWECiphertext ct;
        for (size_t i = 0; i < decomp.size(); i++) {
            ct = decomp[i];
            LWEPlaintext resultDecomp;
            // The last digit should be up to P / p^floor(log_p(P))
            if (i == decomp.size() - 1) {
                p = pLWE2 / std::pow(static_cast<double>(pLWE1), std::floor(std::log(pLWE2) / std::log(pLWE1)));
                std::cout << "\np last digit: " << p << std::endl;
            }
            ccLWE->Decrypt(privateKeyFHEW, ct, &resultDecomp, p);
            std::cout << "(" << resultDecomp << " * " << NativeInteger(pLWE1) << "^" << i << ")";
            if (i != decomp.size() - 1) {
                std::cout << " + ";
            }
        }
        std::cout << std::endl;
    }
}


void test(){
    auto cc = BinFHEContext();

    // STD128 is the security level of 128 bits of security based on LWE Estimator
    // and HE standard. Other common options are TOY, MEDIUM, STD192, and STD256.
    // MEDIUM corresponds to the level of more than 100 bits for both quantum and
    // classical computer attacks.
    cc.GenerateBinFHEContext(STD128);

    // Sample Program: Step 2: Key Generation

    // Generate the secret key
    auto sk = cc.KeyGen();

    cout << "Generating the bootstrapping keys..." << endl;

    // Generate the bootstrapping keys (refresh and switching keys)
    cc.BTKeyGen(sk);

    cout << "Completed the key generation." << endl;

    // Sample Program: Step 3: Encryption

    // Encrypt two ciphertexts representing Boolean True (1).
    // By default, freshly encrypted ciphertexts are bootstrapped.
    // If you wish to get a fresh encryption without bootstrapping, write
    // auto   ct1 = cc.Encrypt(sk, 1, FRESH);
    int p = 1 << 27;
    int q = 1 << 30;
    auto ct1 = cc.Encrypt(sk, 1000000,FRESH,p,q);
    auto ct2 = cc.Encrypt(sk, 58623,FRESH,p,q);
    

    // cc.GetLWEScheme()->EvalMultConstEq(ct1,23);

    LWEPlaintext result;
    // cc.GetLWEScheme()->EvalAddEq(ct1,ct2);
    cc.GetLWEScheme()->EvalMultConstEq(ct1,16);
    cc.Decrypt(sk,ct1,&result,p);
    cout<<"result: "<<result<<"\n";

}

NativeInteger RoundqQAlter(const NativeInteger& v, const NativeInteger& q, const NativeInteger& Q) {
    return NativeInteger(
               (BasicInteger)std::floor(0.5 + v.ConvertToDouble() * q.ConvertToDouble() / Q.ConvertToDouble()))
        .Mod(q);
}



void ArgminViaSchemeSwitching() {
    std::cout << "\n-----ArgminViaSchemeSwitching-----\n" << std::endl;
    std::cout << "Output precision is only wrt the operations in CKKS after switching back\n" << std::endl;

    // Step 1: Setup CryptoContext for CKKS
    uint32_t scaleModSize = 50;
    uint32_t firstModSize = 60;
    // uint32_t ringDim      = 8192;
    SecurityLevel sl      = lbcrypto::HEStd_128_classic;
    BINFHE_PARAMSET slBin = lbcrypto::STD128;
    uint32_t logQ_ccLWE   = 24;
    bool oneHot           = true;  // Change to false if the output should not be one-hot encoded

    uint32_t slots          = 256;  // sparsely-packed
    uint32_t batchSize      = slots;
    uint32_t numValues      = 256;
    ScalingTechnique scTech = FLEXIBLEAUTOEXT;
    // 13 for FHEW to CKKS, log2(numValues) for argmin
    uint32_t multDepth = 9 + 3 + 1 + static_cast<int>(std::log2(numValues));
    if (scTech == FLEXIBLEAUTOEXT)
        multDepth += 1;

    CCParams<CryptoContextCKKSRNS> parameters;
    parameters.SetMultiplicativeDepth(multDepth);
    parameters.SetScalingModSize(scaleModSize);
    parameters.SetFirstModSize(firstModSize);
    parameters.SetScalingTechnique(scTech);
    parameters.SetSecurityLevel(sl);
    // parameters.SetRingDim(ringDim);
    parameters.SetBatchSize(batchSize);

    CryptoContext<DCRTPoly> cc = GenCryptoContext(parameters);

    // Enable the features that you wish to use
    cc->Enable(PKE);
    cc->Enable(KEYSWITCH);
    cc->Enable(LEVELEDSHE);
    cc->Enable(ADVANCEDSHE);
    cc->Enable(SCHEMESWITCH);

    std::cout << "CKKS scheme is using ring dimension " << cc->GetRingDimension();
    std::cout << ", and number of slots " << slots << ", and supports a depth of " << multDepth << std::endl
              << std::endl;

    // Generate encryption keys
    auto keys = cc->KeyGen();

    // Step 2: Prepare the FHEW cryptocontext and keys for FHEW and scheme switching
    SchSwchParams params;
    params.SetSecurityLevelCKKS(sl);
    params.SetSecurityLevelFHEW(slBin);
    params.SetCtxtModSizeFHEWLargePrec(logQ_ccLWE);
    params.SetNumSlotsCKKS(slots);
    params.SetNumValues(numValues);
    params.SetComputeArgmin(true);
    auto privateKeyFHEW = cc->EvalSchemeSwitchingSetup(params);
    auto ccLWE          = cc->GetBinCCForSchemeSwitch();

    cc->EvalSchemeSwitchingKeyGen(keys, privateKeyFHEW);

    std::cout << "FHEW scheme is using lattice parameter " << ccLWE->GetParams()->GetLWEParams()->Getn();
    std::cout << ", logQ " << logQ_ccLWE;
    std::cout << ", and modulus q " << ccLWE->GetParams()->GetLWEParams()->Getq() << std::endl << std::endl;

    // Scale the inputs to ensure their difference is correctly represented after switching to FHEW
    double scaleSign = 512.0;
    auto modulus_LWE = 1 << logQ_ccLWE;
    auto beta        = ccLWE->GetBeta().ConvertToInt();
    auto pLWE        = modulus_LWE / (2 * beta);  // Large precision
    // This formulation is for clarity


    auto start2 = std::chrono::high_resolution_clock::now();
    cc->EvalCompareSwitchPrecompute(pLWE, scaleSign);
    auto end2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff2 = end2 - start2;
    std::cout << "Time to compute compare switch precompute: " << diff2.count() << " s" << std::endl;



    // But we can also include the scaleSign in pLWE (here we use the fact both pLWE and scaleSign are powers of two)
    // cc->EvalCompareSwitchPrecompute(pLWE / scaleSign, 1);

    // Step 3: Encoding and encryption of inputs
    // Inputs
    int n = 128;
    std::vector<double> x1(n);
    std::srand(static_cast<unsigned>(std::time(0))); // Seed random number generator
    
    for (int i = 0; i < n; ++i) {
        x1[i] = std::rand() % 16; // Random value in range [0, 255]
    }
    // std::vector<double> x1 = {-1.125, -1.12, 5.0,  6.0,  -1.0, 2.0,  8.0,   -1.0,
    //                           9.0,    10.0,  11.0, 12.0, 13.0, 14.0, 15.25, 15.30};
    if (x1.size() < numValues) {
        std::vector<int> zeros(numValues - x1.size(), 0);
        x1.insert(x1.end(), zeros.begin(), zeros.end());
    }

    std::cout << "Expected minimum value " << *(std::min_element(x1.begin(), x1.begin() + numValues)) << " at location "
              << std::min_element(x1.begin(), x1.begin() + numValues) - x1.begin() << std::endl;
    std::cout << "Expected maximum value " << *(std::max_element(x1.begin(), x1.begin() + numValues)) << " at location "
              << std::max_element(x1.begin(), x1.begin() + numValues) - x1.begin() << std::endl
              << std::endl;

    // Encoding as plaintexts
    Plaintext ptxt1 = cc->MakeCKKSPackedPlaintext(x1);  // Only if we we set batchsize
    // Plaintext ptxt1 = cc->MakeCKKSPackedPlaintext(x1, 1, 0, nullptr, slots); // If batchsize is not set

    // Encrypt the encoded vectors
    auto c1 = cc->Encrypt(keys.publicKey, ptxt1);


    auto start = std::chrono::high_resolution_clock::now();

    // Step 4: Argmin evaluation
    auto result = cc->EvalMinSchemeSwitching(c1, keys.publicKey, numValues, slots);

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    std::cout << "Time to compute argmin: " << diff.count() << " s" << std::endl;

    Plaintext ptxtMin;
    cc->Decrypt(keys.secretKey, result[0], &ptxtMin);
    ptxtMin->SetLength(1);
    std::cout << "Minimum value: " << ptxtMin << std::endl;
    cc->Decrypt(keys.secretKey, result[1], &ptxtMin);
    if (oneHot) {
        ptxtMin->SetLength(numValues);
        std::cout << "Argmin indicator vector: " << ptxtMin << std::endl;
    }
    else {
        ptxtMin->SetLength(1);
        std::cout << "Argmin: " << ptxtMin << std::endl;
    }

    // result = cc->EvalMaxSchemeSwitching(c1, keys.publicKey, numValues, slots);

    // Plaintext ptxtMax;
    // cc->Decrypt(keys.secretKey, result[0], &ptxtMax);
    // ptxtMax->SetLength(1);
    // std::cout << "Maximum value: " << ptxtMax << std::endl;
    // cc->Decrypt(keys.secretKey, result[1], &ptxtMax);
    // if (oneHot) {
    //     ptxtMax->SetLength(numValues);
    //     std::cout << "Argmax indicator vector: " << ptxtMax << std::endl;
    // }
    // else {
    //     ptxtMax->SetLength(1);
    //     std::cout << "Argmax: " << ptxtMax << std::endl;
    // }
}




void decomp() {
    // Sample Program: Step 1: Set CryptoContext
    auto ccLWE = std::make_shared<BinFHEContext>();
    auto logQ = 12;
    auto N = 1 << 11;
    // ccLWE->BinFHEContext::GenerateBinFHEContext(TOY, false, logQ_ccLWE, 0, GINX, false);
    // auto cc = BinFHEContext();
    ccLWE->BinFHEContext::GenerateBinFHEContext(STD128, false, logQ, N, GINX);


    auto ccbaseks = ccLWE->GetParams()->GetLWEParams()->GetBaseKS();
    std::cout << "ccbaseks: " << ccbaseks << std::endl;
    auto ccqks = ccLWE->GetParams()->GetLWEParams()->GetqKS();
    std::cout << "ccqks: " << ccqks << std::endl;
    // auto ccbaseg = ccLWE->GetParams()->GetLWEParams()->GetDgg()

    auto ccn = ccLWE->GetParams()->GetLWEParams()->Getn();
    std::cout << "ccn: " << ccn << std::endl;

    auto ccq = ccLWE->GetParams()->GetLWEParams()->Getq();
    std::cout << "ccq: " << ccq << std::endl;
    auto ccN = ccLWE->GetParams()->GetLWEParams()->GetN();
    std::cout << "N: " << ccN << std::endl;
    // Sample Program: Step 2: Key Generation
    // uint32_t Q = 1 << logQ;

    int q      = ccq.ConvertToInt();                                               // q
    int factor = 1 << int(logQ - log2(q));                           // Q/q
    uint64_t P = ccLWE->GetMaxPlaintextSpace().ConvertToInt() * factor;  // Obtain the maximum plaintext space

    std::cout << "P: " << P << std::endl;
    uint64_t p = ccLWE->GetMaxPlaintextSpace().ConvertToInt();
    std::cout << "p: " << p << std::endl;
    // Generate the secret key
    auto sk = ccLWE->KeyGen();

    std::cout << "Generating the bootstrapping keys..." << std::endl;

    // Generate the bootstrapping keys (refresh, switching and public keys)
    ccLWE->BTKeyGen(sk, PUB_ENCRYPT);

    auto pk = ccLWE->GetPublicKey();

    std::cout << "Completed the key generation." << std::endl;

    int n = 256;
    int bit = 4;
    int num = 1 << bit;
    std::vector<int> array(n);
    std::srand(static_cast<unsigned>(std::time(0))); // Seed random number generator
    
    for (int i = 0; i < n; ++i) {
        array[i] = std::rand() % num; // Random value in range [0, 255]
    }

    vector<vector<LWECiphertext>> LWEarray;
    int index = 0;
    for (int num : array) {
        std::vector<int> base4Digits;

        // Extract base 4 digits
        while (num > 0) {
            base4Digits.push_back(num % 4);
            num /= 4;
        }

        // Ensure the number has exactly 4 digits by padding with zeros
        while (base4Digits.size() < bit/2) {
            base4Digits.push_back(0); // Pad with zeros
        }

        // Print the original number and its base 4 digits
        std::cout << "Number: " << array[index] << ", Base 4 digits: ";
        for (int digit : base4Digits) {
            std::cout << digit << " ";
        }
        std::cout << std::endl;

        // Encrypt the digits
        vector<LWECiphertext> encryptedDigits;
        for (int digit : base4Digits) {
            auto encryptedDigit = ccLWE->Encrypt(sk, digit, LARGE_DIM, p);
            encryptedDigits.push_back(encryptedDigit);
        }

        // Add the encrypted digits to LWEarray
        LWEarray.push_back(encryptedDigits);
        index++;
    }

    auto fp_prime = [](NativeInteger m, NativeInteger p1) -> NativeInteger {
        auto im = m.ConvertToInt();
        im = im & 0b0111;
        return NativeInteger((im/4)%4);
        // return (m/4)%4;
    };
    auto lut_prime = ccLWE->GenerateLUTviaFunction(fp_prime, p);

    auto fmul_prime = [](NativeInteger m, NativeInteger p1) -> NativeInteger {
        auto im = m.ConvertToInt();
        im = im & 0b0111;
        auto l = (im>>1)%4;
        auto r = im%2;
        return NativeInteger(r*l);
    };

    auto lutmul_prime = ccLWE->GenerateLUTviaFunction(fmul_prime, p);



    auto fp = [](NativeInteger m, NativeInteger p1) -> NativeInteger {
        return (m/4)%4;
    };

    
    auto lut = ccLWE->GenerateLUTviaFunction(fp, p);

    auto fmul = [](NativeInteger m, NativeInteger p1) -> NativeInteger {
        auto l = (m>>1)%4;
        auto r = m%2;
        return r*l;
    };

    auto lutmul = ccLWE->GenerateLUTviaFunction(fmul, p);



    int size = LWEarray.size()-1;
    vector<vector<LWECiphertext>> primes(size, vector<LWECiphertext>(bit/2));
    vector<LWECiphertext> dres_prime(size);
    for (size_t i = 0; i < size; i++) {
        for (size_t j = 0; j < bit/2; ++j) {
            primes[i][j] = ccLWE->Encrypt(sk, 3, LARGE_DIM, p);
        }
        dres_prime[i] = ccLWE->Encrypt(sk, 1, LARGE_DIM, p);
    }











    auto start = std::chrono::high_resolution_clock::now();

    // for (int m = 0; m <log(size)+1; m++){
    // #pragma omp parallel for
    for (int i = 0; i < size; i++) {
        auto& digits1 = LWEarray[i];
        auto& digits2 = LWEarray[i + 1];

        for (size_t j = 0; j < digits1.size(); ++j) {
            ccLWE->GetLWEScheme()->EvalSubEq(primes[i][j], digits1[j]);
        }

        // Perform digit-wise operations
        ccLWE->GetLWEScheme()->EvalAddEq(primes[i][0], digits2[0]);
        auto dres = ccLWE->EvalFunc(primes[i][0], lut_prime);
        for (size_t j = 1; j < digits1.size(); ++j) {
            ccLWE->GetLWEScheme()->EvalAddEq(primes[i][j], digits2[j]);
            ccLWE->GetLWEScheme()->EvalAddEq(primes[i][j], dres);
            dres = ccLWE->EvalFunc(primes[i][j], lut_prime);
        }

        ccLWE->GetLWEScheme()->EvalSubEq(dres_prime[i], dres);

        // #pragma omp parallel for
        for (size_t j = 0; j < digits2.size(); ++j) {
            ccLWE->GetLWEScheme()->EvalMultConstEq(digits2[j], 2);
            ccLWE->GetLWEScheme()->EvalAddEq(digits2[j], dres_prime[i]);
            auto temp2 = ccLWE->EvalFunc(digits2[j], lutmul_prime);

            ccLWE->GetLWEScheme()->EvalMultConstEq(digits1[j], 2);
            ccLWE->GetLWEScheme()->EvalAddEq(digits1[j], dres);
            auto temp1 = ccLWE->EvalFunc(digits1[j], lutmul_prime);

            ccLWE->GetLWEScheme()->EvalAddEq(temp1,temp2);
            LWEarray[i + 1][j] = temp1;
        }
    }
    // }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "Time taken: " << duration << " milliseconds" << std::endl;


    cout << "Min: ";
    auto minElement = *std::min_element(array.begin(), array.end());
    cout << minElement << endl;

    cout << "Result: ";
    for (auto elem:LWEarray[size]){
        LWEPlaintext resd;
        ccLWE->Decrypt(sk, elem, &resd, p);
        std::cout <<  resd << " ";
    }
    cout << endl;
    


    // A. Specify main parameters
    ScalingTechnique scTech = FIXEDAUTO;
    // for r = 3 in FHEWtoCKKS, Chebyshev max depth allowed is 9, 1 more level for postscaling
    uint32_t multDepth = 3 + 9 + 1;
    if (scTech == FLEXIBLEAUTOEXT)
        multDepth += 1;
    uint32_t scaleModSize = 50;
    uint32_t ringDim      = 8192;
    SecurityLevel sl      = HEStd_NotSet;  // If this is not HEStd_NotSet, ensure ringDim is compatible
    // uint32_t logQ_ccLWE   = 28;

    // uint32_t slots = ringDim/2; // Uncomment for fully-packed
    uint32_t slots     = 16;  // sparsely-packed
    uint32_t batchSize = slots;

    CCParams<CryptoContextCKKSRNS> parameters;
    parameters.SetMultiplicativeDepth(multDepth);
    parameters.SetScalingModSize(scaleModSize);
    parameters.SetScalingTechnique(scTech);
    parameters.SetSecurityLevel(sl);
    parameters.SetRingDim(ringDim);
    parameters.SetBatchSize(batchSize);

    CryptoContext<DCRTPoly> cc_ck = GenCryptoContext(parameters);

    // Enable the features that you wish to use
    cc_ck->Enable(PKE);
    cc_ck->Enable(KEYSWITCH);
    cc_ck->Enable(LEVELEDSHE);
    cc_ck->Enable(ADVANCEDSHE);
    cc_ck->Enable(SCHEMESWITCH);

    std::cout << "CKKS scheme is using ring dimension " << cc_ck->GetRingDimension();
    std::cout << ", number of slots " << slots << ", and supports a multiplicative depth of " << multDepth << std::endl
              << std::endl;

    // Generate encryption keys.
    auto keys = cc_ck->KeyGen();

    
    cc_ck->SetBinCCForSchemeSwitch(ccLWE);


    // auto num1d1 = cc.Encrypt(sk, 1, LARGE_DIM, 16);
    // auto num1d2 = cc.Encrypt(sk, 2, LARGE_DIM, 16);
    // auto num1d3 = cc.Encrypt(sk, 3, LARGE_DIM, 16);
    // auto num1d4 = cc.Encrypt(sk, 2, LARGE_DIM, 16);

    // auto num2d1 = cc.Encrypt(sk, 3, LARGE_DIM, 16);
    // auto num2d2 = cc.Encrypt(sk, 0, LARGE_DIM, 16);
    // auto num2d3 = cc.Encrypt(sk, 1, LARGE_DIM, 16);
    // auto num2d4 = cc.Encrypt(sk, 2, LARGE_DIM, 16);


    // auto time = std::chrono::high_resolution_clock::now();

  
    // auto num1d1_prime = cc.Encrypt(sk, 3, LARGE_DIM, 16);
    // auto num1d2_prime = cc.Encrypt(sk, 3, LARGE_DIM, 16);
    // auto num1d3_prime = cc.Encrypt(sk, 3, LARGE_DIM, 16);
    // auto num1d4_prime = cc.Encrypt(sk, 3, LARGE_DIM, 16);


    // cc.GetLWEScheme()->EvalSubEq(num1d1_prime,num1d1);
    // cc.GetLWEScheme()->EvalSubEq(num1d2_prime,num1d2);
    // cc.GetLWEScheme()->EvalSubEq(num1d3_prime,num1d3);
    // cc.GetLWEScheme()->EvalSubEq(num1d4_prime,num1d4);



    


    // cc.GetLWEScheme()->EvalAddEq(num1d1_prime,num2d1);
    // auto dres1 = cc.EvalFunc(num1d1_prime, lut);

    // cc.GetLWEScheme()->EvalAddEq(num1d2_prime,num2d2);
    // cc.GetLWEScheme()->EvalAddEq(num1d2_prime, dres1);
    // auto dres2 = cc.EvalFunc(num1d2_prime, lut);

    // cc.GetLWEScheme()->EvalAddEq(num1d3_prime,num2d3);
    // cc.GetLWEScheme()->EvalAddEq(num1d3_prime, dres2);
    // auto dres3 = cc.EvalFunc(num1d3_prime, lut);

    // cc.GetLWEScheme()->EvalAddEq(num1d4_prime,num2d4);
    // cc.GetLWEScheme()->EvalAddEq(num1d4_prime, dres3);
    // auto dres4 = cc.EvalFunc(num1d4_prime, lut);

    // auto dres4_prime = cc.Encrypt(sk, 1, LARGE_DIM, 16);
    // cc.GetLWEScheme()->EvalSubEq(dres4_prime, dres4);


    // cc.GetLWEScheme()->EvalMultConstEq(num2d1, 2);
    // cc.GetLWEScheme()->EvalAddEq(num2d1,dres4_prime);
    // auto num1d1_tmp = cc.EvalFunc(num2d1, lutmul);

    // cc.GetLWEScheme()->EvalMultConstEq(num2d2, 2);
    // cc.GetLWEScheme()->EvalAddEq(num2d2,dres4_prime);
    // auto num1d2_tmp = cc.EvalFunc(num2d2, lutmul);

    // cc.GetLWEScheme()->EvalMultConstEq(num2d3, 2);
    // cc.GetLWEScheme()->EvalAddEq(num2d3,dres4_prime);
    // auto num1d3_tmp = cc.EvalFunc(num2d3, lutmul);

    // cc.GetLWEScheme()->EvalMultConstEq(num2d4, 2);
    // cc.GetLWEScheme()->EvalAddEq(num2d4,dres4_prime);
    // auto num1d4_tmp = cc.EvalFunc(num2d4, lutmul);





    // auto time2 = std::chrono::high_resolution_clock::now();
    // auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(time2 - time).count();
    // std::cout << "Time taken: " << duration << " milliseconds" << std::endl;


    // LWEPlaintext resd;
    // cc.Decrypt(sk, num1d1_tmp, &resd, p);
    // std::cout << "Decrypted result: " << resd << std::endl;

    // cc.GetLWEScheme()->ModSwitch(qks, dres);

    // auto d1newmod = dres->GetModulus();
    // std::cout << "d1newmod: " << d1newmod << std::endl;


    // cc.GetLWEScheme()->EvalMultConstEq(dres, 2);
    // LWEPlaintext resd1;
    // cc.Decrypt(sk, dres, &resd1, 128);
    // std::cout << "Decrypted result: " << resd1 << std::endl;


    // auto tempctxt2 = cc.Encrypt(sk, 0, SMALL_DIM, p*2, q);

    // cc.GetLWEScheme()->ModSwitch(q, tempctxt2);

    // auto original_a = tempctxt2->GetA();
    // auto original_b = tempctxt2->GetB();
    // // multiply by Q_LWE/Q' and round to Q_LWE
    // NativeVector a_round(ccn, q);
    // for (uint32_t j = 0; j < ccn; ++j) {
    //     a_round[j] = RoundqQAlter(original_a[j], q, q*2);
    // }
    // NativeInteger b_round = RoundqQAlter(original_b, q, q*2);
    // tempctxt2     = std::make_shared<LWECiphertextImpl>(std::move(a_round), std::move(b_round));


    // auto tempq = tempctxt2->GetModulus();
    // std::cout << "tempq: " << tempq << std::endl;

    // auto dresq = dres->GetModulus();
    // std::cout << "dresq: " << dresq << std::endl;


    // cc.GetLWEScheme()->EvalAddEq(tempctxt2,d2);
    // auto templut = cc.GenerateLUTviaFunction(fp, p*2);
    // auto ctxtres = cc.EvalFunc(tempctxt2,templut);
    // // cc.GetLWEScheme()->EvalMultConstEq(tempctxt2,2);
    // LWEPlaintext restemp2;
    // cc.Decrypt(sk, ctxtres, &restemp2, p*2);
    // std::cout << "restemp2: " << restemp2 << std::endl;

    // auto lut2 = cc.GenerateLUTviaFunction(fp, P);
    // auto dres2 = cc.EvalFunc(ct2, lut2);
    // LWEPlaintext resd2;
    // cc.Decrypt(sk, ct2, &resd2, P);
    // std::cout << "Decrypted result high precision: " << resd2 << std::endl;

}



int main() {
    // ArgminViaSchemeSwitching();

    decomp();

    return 0;
}