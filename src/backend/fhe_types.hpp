#ifndef FHE_TYPES_HPP
#define FHE_TYPES_HPP

#include <openfhe.h>

namespace CKKS {

class Context_CKKS {
public:
    Context_CKKS() : cc(nullptr), keys(nullptr) {
    }
    Context_CKKS(const lbcrypto::CryptoContext<lbcrypto::DCRTPoly>& cc, const lbcrypto::KeyPair<lbcrypto::DCRTPoly>& keys) : cc(cc), keys(keys) {}
    lbcrypto::CryptoContext<lbcrypto::DCRTPoly> getCryptoContext() const { return cc; }
    void setCryptoContext(const lbcrypto::CryptoContext<lbcrypto::DCRTPoly>& cc) { this->cc = cc; }
    lbcrypto::KeyPair<lbcrypto::DCRTPoly> getKeys() const { return keys; }
    void setKeys(const lbcrypto::KeyPair<lbcrypto::DCRTPoly>& keys) { this->keys = keys; }
    int getBatchSize() const { return batchSize; }
    void setBatchSize(int batchSize) { this->batchSize = batchSize; }
    
    lbcrypto::CryptoContext<lbcrypto::DCRTPoly> cc;
    lbcrypto::KeyPair<lbcrypto::DCRTPoly> keys;
    int batchSize;
};

class FHEdouble {
public:
    FHEdouble() : ciphertext(nullptr) {
    }
    FHEdouble(const lbcrypto::Ciphertext<lbcrypto::DCRTPoly>& cipher) : ciphertext(cipher) {}
    lbcrypto::Ciphertext<lbcrypto::DCRTPoly> getCiphertext() const { return ciphertext; }

private:
    lbcrypto::Ciphertext<lbcrypto::DCRTPoly> ciphertext;
};

class FHEplainf {
public:
    FHEplainf() = default;
    FHEplainf(const lbcrypto::Plaintext& ptxt) : plaintext(ptxt) {}
    lbcrypto::Plaintext getPlaintext() const { return plaintext; }

private:
    lbcrypto::Plaintext plaintext;
};

} 

namespace CGGI {

class Context_CGGI {
public:
    Context_CGGI() : ccLWE(nullptr), keysLWE(nullptr) {
    }
    Context_CGGI(const std::shared_ptr<lbcrypto::BinFHEContext>& ccLWE, const lbcrypto::LWEPrivateKey& keys) : ccLWE(ccLWE), keysLWE(keys) {}
    std::shared_ptr<lbcrypto::BinFHEContext> getCryptoContext() const { return ccLWE; }
    void setCryptoContext(const std::shared_ptr<lbcrypto::BinFHEContext>& ccLWE) { this->ccLWE = ccLWE; }
    lbcrypto::LWEPrivateKey getKeys() const { return keysLWE; }
    void setKeys(const lbcrypto::LWEPrivateKey& keys) { this->keysLWE = keys; }
private:
    std::shared_ptr<lbcrypto::BinFHEContext> ccLWE;
    lbcrypto::LWEPrivateKey keysLWE;
};


class FHEi32 {
public:
    FHEi32(lbcrypto::LWECiphertext& cipher) : ciphertext(cipher) {}
    lbcrypto::LWECiphertext& getCiphertext() { return ciphertext; }

private:
    lbcrypto::LWECiphertext ciphertext;
};

class FHEplaini {
public:
    FHEplaini() = default;
    FHEplaini(const lbcrypto::LWEPlaintext& ptxt) : plaintext(ptxt) {}
    lbcrypto::LWEPlaintext getPlaintext() const { return plaintext; }

private:
    lbcrypto::LWEPlaintext plaintext;
};

} // namespace CGGI

#endif // FHE_TYPES_HPP
