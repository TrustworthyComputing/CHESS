#include "openfhe.h"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <random>

using namespace lbcrypto;
using namespace std;

shared_ptr<lbcrypto::BinFHEContext> setup(enum lbcrypto::BINFHE_PARAMSET parm, bool arb, int N){
    // Sample Program: Step 1: Set CryptoContext
    auto ccLWE = std::make_shared<BinFHEContext>();
    auto logQ = 25;
    // ccLWE->BinFHEContext::GenerateBinFHEContext(TOY, false, logQ_ccLWE, 0, GINX, false);
    // auto cc = BinFHEContext();
    ccLWE->BinFHEContext::GenerateBinFHEContext(parm, arb ,logQ, N, GINX);


    auto ccn = ccLWE->GetParams()->GetLWEParams()->Getn();
    std::cout << "ccn: " << ccn << std::endl;

    auto ccq = ccLWE->GetParams()->GetLWEParams()->Getq();
    std::cout << "ccq: " << ccq << std::endl;
    auto ccN = ccLWE->GetParams()->GetLWEParams()->GetN();
    std::cout << "N: " << ccN << std::endl;

    uint64_t p = ccLWE->GetMaxPlaintextSpace().ConvertToInt();
    std::cout << "p: " << p << std::endl;
    // Generate the secret key
    auto sk = ccLWE->KeyGen();

    std::cout << "Generating the bootstrapping keys..." << std::endl;

    // Generate the bootstrapping keys (refresh, switching and public keys)
    ccLWE->BTKeyGen(sk, PUB_ENCRYPT);

    std::cout << "Completed the key generation." << std::endl;
    return ccLWE;
}

vector<vector<LWECiphertext>> create_array(shared_ptr<lbcrypto::BinFHEContext> ccLWE, int n, int num){
    auto sk = ccLWE->KeyGen();
    std::vector<int> array(n);
    std::srand(static_cast<unsigned>(std::time(0))); // Seed random number generator
    
    for (int i = 0; i < n; ++i) {
        array[i] = std::rand() % num; // Random value in range [0, 255]
    }

    vector<vector<LWECiphertext>> LWEarray;
    int index = 0;
    const size_t digit_count = static_cast<size_t>(std::log2(num) / 2);
    for (int number : array) {
        std::vector<int> base4Digits;

        // Extract base 4 digits
        while (number > 0) {
            base4Digits.push_back(number % 4);
            number /= 4;
        }

        // Ensure the number has exactly 4 digits by padding with zeros
        cout << digit_count << endl;
        while (base4Digits.size() < digit_count) {
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
            auto encryptedDigit = ccLWE->Encrypt(sk, digit, LARGE_DIM, 32);
            encryptedDigits.push_back(encryptedDigit);
        }

        // Add the encrypted digits to LWEarray
        LWEarray.push_back(encryptedDigits);
        index++;
    }
    return LWEarray;
}
NativeInteger RoundqQAlter(const NativeInteger& v, const NativeInteger& q, const NativeInteger& Q) {
    return NativeInteger(
               (BasicInteger)std::floor(0.5 + v.ConvertToDouble() * q.ConvertToDouble() / Q.ConvertToDouble()))
        .Mod(q);
}

void min_index() {
    auto ccLWE = setup(TOY, false, 1<<11);
    auto sk = ccLWE->KeyGen();
    uint64_t p = ccLWE->GetMaxPlaintextSpace().ConvertToInt();
    auto q = ccLWE->GetParams()->GetLWEParams()->Getq();
    auto logQ = 12;

    int n = 16;
    int num = 1 << 8;
    auto LWEarray = create_array(ccLWE,n,num);



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
        // auto im = m.ConvertToInt();
        // im = im & 
        return (m/4)%4;
    };

    
    auto lut = ccLWE->GenerateLUTviaFunction(fp, p);

        auto fmul = [](NativeInteger m, NativeInteger p1) -> NativeInteger {
        auto l = (m>>1)%4;
        auto r = m%2;
        return r*l;
    };

    auto lutmul = ccLWE->GenerateLUTviaFunction(fmul, p);


    auto start = std::chrono::high_resolution_clock::now();

    size_t size = LWEarray.size() - 1;
    int count = 0;
    int min = 0;

    for (size_t m = 0; (static_cast<size_t>(1) << m) <= size; ++m){
    const size_t offset = static_cast<size_t>(1) << m;
    const size_t stride = static_cast<size_t>(1) << (m + 1);
    const size_t limit = size - offset + 1;
    #pragma omp parallel for
    for(size_t i = offset - 1; i < limit; i += stride) {
        count +=1;
        // Grab two adjacent encrypted arrays
        const auto& digits1 = LWEarray[i];
        const auto& digits2 = LWEarray[i + offset];

        vector<LWECiphertext> primes(digits1.size());

        for (size_t j = 0; j < digits1.size(); ++j) {
            primes[j] = ccLWE->Encrypt(sk, 3, LARGE_DIM, p);
        }


        // #pragma omp parallel for
        for (size_t j = 0; j < digits1.size(); ++j) {
            ccLWE->GetLWEScheme()->EvalSubEq(primes[j], digits1[j]);
        }

        // Perform digit-wise operations
        ccLWE->GetLWEScheme()->EvalAddEq(primes[0], digits2[0]);
        auto dres = ccLWE->EvalFunc(primes[0], lut);
        // #pragma omp parallel for
        for (size_t j = 1; j < digits1.size(); ++j) {
            ccLWE->GetLWEScheme()->EvalAddEq(primes[j], digits2[j]);
            ccLWE->GetLWEScheme()->EvalAddEq(primes[j], dres);
            dres = ccLWE->EvalFunc(primes[j], lut);
        }

        auto dres_prime = ccLWE->Encrypt(sk, 1, LARGE_DIM, p);
        ccLWE->GetLWEScheme()->EvalSubEq(dres_prime, dres);

        const size_t digits2_size = digits2.size();
        #pragma omp parallel for
        for (size_t j = 0; j < digits2_size; ++j) {
            auto mutableDigits2 = digits2;
            ccLWE->GetLWEScheme()->EvalMultConstEq(mutableDigits2[j], 2);
            ccLWE->GetLWEScheme()->EvalAddEq(mutableDigits2[j], dres_prime);


            auto sag2 = ccLWE->GetLWEScheme()->ModSwitch(q/2, digits2[j]);


            auto temp2 = ccLWE->EvalFunc(sag2, lutmul);


            auto mutableDigits1 = digits1;
            
            ccLWE->GetLWEScheme()->EvalMultConstEq(mutableDigits1[j], 2);
            ccLWE->GetLWEScheme()->EvalAddEq(mutableDigits1[j], dres);

            auto sag1 = ccLWE->GetLWEScheme()->ModSwitch(q/2, digits1[j]);
            auto temp1 = ccLWE->EvalFunc(sag1, lutmul);


            ccLWE->GetLWEScheme()->EvalAddEq(temp1,temp2);

            auto tempres = ccLWE->GetLWEScheme()->ModSwitch(q, temp1);

            LWEarray[i + 1][j] = tempres;
            min = i+1;
        }
    }
    }
    cout <<"Min: ";
    for (auto elem: LWEarray[min]){
        LWEPlaintext resd;
        ccLWE->Decrypt(sk, elem, &resd, p);
        std::cout << resd << " ";
    }
    cout << endl;
    cout << "count: " << count << endl;

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "Time taken: " << duration << " milliseconds" << std::endl;
    


    // A. Specify main parameters
    ScalingTechnique scTech = FIXEDAUTO;
    // for r = 3 in FHEWtoCKKS, Chebyshev max depth allowed is 9, 1 more level for postscaling
    uint32_t multDepth = 3 + 9 + 1;
    if (scTech == FLEXIBLEAUTOEXT)
        multDepth += 1;
    uint32_t scaleModSize = 50;
    // uint32_t ringDim      = 8192;
    SecurityLevel sl      = lbcrypto::HEStd_128_classic;  // If this is not HEStd_NotSet, ensure ringDim is compatible
    // uint32_t logQ_ccLWE   = 28;

    // uint32_t slots = ringDim/2; // Uncomment for fully-packed
    uint32_t slots     = 256;  // sparsely-packed
    uint32_t batchSize = slots;

    CCParams<CryptoContextCKKSRNS> parameters;
    parameters.SetMultiplicativeDepth(multDepth);
    parameters.SetScalingModSize(scaleModSize);
    parameters.SetScalingTechnique(scTech);
    parameters.SetSecurityLevel(sl);
    // parameters.SetRingDim(ringDim);
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

    cc_ck->EvalFHEWtoCKKSSetup(ccLWE, slots, logQ);
    cc_ck->SetBinCCForSchemeSwitch(ccLWE);

    cc_ck->EvalFHEWtoCKKSKeyGen(keys, sk);


    auto start2 = std::chrono::high_resolution_clock::now();

    
    auto cTemp = cc_ck->EvalFHEWtoCKKS(LWEarray[size-1], slots);

    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::milliseconds>(end2 - start2).count();
    std::cout << "Time taken: " << duration2 << " milliseconds" << std::endl;


}

int main(){
    min_index();
    return 0;
}
