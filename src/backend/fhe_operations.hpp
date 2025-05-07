#ifndef FHE_OPERATIONS_HPP
#define FHE_OPERATIONS_HPP

#include "fhe_types.hpp"

class FHEcontext {
public:
    FHEcontext() : ck(), cg() {}
    FHEcontext(CKKS::Context_CKKS ck, CGGI::Context_CGGI cg) : ck(ck), cg(cg) {}
    CKKS::Context_CKKS getCKKS() { return ck; }
    CGGI::Context_CGGI getCGGI() { return cg; }
private:
    CKKS::Context_CKKS ck;
    CGGI::Context_CGGI cg;
    
};

namespace CGGI {
    class CGGI_scheme {
    public:
        CGGI_scheme(CKKS::Context_CKKS context);
        Context_CGGI getContext();
    private:
        Context_CGGI context;
    };

    FHEi32 FHEaddi(FHEcontext* ctx, FHEi32 a, FHEi32 b);
    FHEi32 FHEsubi(FHEcontext* ctx, FHEi32 a, FHEi32 b);

    // Arithmetic Operations with Plaintext
    FHEi32 FHEaddiP(FHEcontext* ctx, FHEi32 a, int b);
    FHEi32 FHEsubiP(FHEcontext* ctx, FHEi32 a, int b);
    FHEi32 FHEsubiP(FHEcontext* ctx, int b, FHEi32 a);
    FHEi32 FHEmuliP(FHEcontext* ctx, FHEi32 a, int b);

    // Boolean Operations for FHEi32
    FHEi32 FHEand(FHEcontext* ctx, FHEi32 a, FHEi32 b);
    FHEi32 FHEor(FHEcontext* ctx, FHEi32 a, FHEi32 b);
    FHEi32 FHExor(FHEcontext* ctx, FHEi32 a, FHEi32 b);
    FHEi32 FHExnor(FHEcontext* ctx, FHEi32 a, FHEi32 b);
    FHEi32 FHEnot(FHEcontext* ctx, FHEi32 a);

    FHEplaini FHEdecrypt(FHEcontext* ctx, const FHEi32 a, int p);
}

namespace CKKS {
    class CKKS_scheme {
    public:
        CKKS_scheme(int multDepth = 17, int scaleModSize = 50, int batchSize = 1);
        Context_CKKS getContext();
        std::vector<CGGI::FHEi32> FHEsign(std::vector<CGGI::FHEi32> lwes);

    private:
        Context_CKKS context;
        int multDepth;
        int scaleModSize;
        int batchSize;
        int ringDimension;
    };

    FHEdouble FHEvectorSum(FHEcontext* ctx, CKKS::FHEdouble a);
    FHEdouble FHEbroadcast(FHEcontext* ctx, CKKS::FHEdouble a);
    FHEdouble FHErotate(FHEcontext* ctx, CKKS::FHEdouble a, int n);

    FHEdouble FHEaddf(FHEcontext* ctx, const FHEdouble a, const FHEdouble b);
    FHEdouble FHEsubf(FHEcontext* ctx, const FHEdouble a, const FHEdouble b);
    FHEdouble FHEmulf(FHEcontext* ctx, const FHEdouble a, const FHEdouble b);
    FHEdouble FHEdivf(FHEcontext* ctx, const FHEdouble a, const FHEdouble b);

    // Arithmetic Operations with Plaintext
    FHEdouble FHEaddfP(FHEcontext* ctx, const FHEdouble a, double b);
    FHEdouble FHEsubfP(FHEcontext* ctx, const FHEdouble a, double b);
    FHEdouble FHEsubfP(FHEcontext* ctx, double b, const FHEdouble a);
    FHEdouble FHEmulfP(FHEcontext* ctx, const FHEdouble a, double b);
    FHEdouble FHEdivfP(FHEcontext* ctx, const FHEdouble a, double b);
    FHEdouble FHEdivfP(FHEcontext* ctx, double b, const FHEdouble a);

    FHEplainf FHEencode(FHEcontext* ctx, const std::vector<double>& a);
    FHEdouble FHEencrypt(FHEcontext* ctx, const CKKS::FHEplainf p);
    FHEdouble FHEencrypt(FHEcontext* ctx, const std::vector<double>& a);
    CKKS::FHEdouble FHEencrypt(FHEcontext* ctx, const double a);
    FHEplainf FHEdecrypt(FHEcontext* ctx, const FHEdouble a);
}


std::vector<CGGI::FHEi32> CKKStoCGGI(FHEcontext* ctx, CKKS::FHEdouble a);
CKKS::FHEdouble CGGItoCKKS(FHEcontext* ctx, std::vector<CGGI::FHEi32> a);


CKKS::FHEdouble FHEeq(FHEcontext* ctx, CKKS::FHEdouble a, CKKS::FHEdouble b);
CKKS::FHEdouble FHEselect(FHEcontext* ctx, CKKS::FHEdouble sign, CKKS::FHEdouble value1, CKKS::FHEdouble value2);


#endif // FHE_OPERATIONS_HPP
