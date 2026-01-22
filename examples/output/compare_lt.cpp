#include "../../src/backend/fhe_operations.hpp"
#include "../../src/backend/fhe_types.hpp"
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using namespace CKKS;
using namespace CGGI;

FHEdouble compare_lt(FHEcontext* v1, FHEdouble v2, FHEdouble v3) {
  FHEdouble v4 = FHElt(v1, v2, v3);
  return v4;
}



int main(int argc, char **argv) {
    std::string security = "secure";
    if (argc > 1) {
        security = argv[1];
    }
    CKKS_scheme ck(15, 59, 1, security);
    CGGI_scheme cg(ck.getContext(), security);
    FHEcontext* ctx = new FHEcontext(ck.getContext(), cg.getContext());

    double arg1_plain = 5;
    FHEdouble arg1 = FHEencrypt(ctx, arg1_plain);

    double arg2_plain = 0;
    FHEdouble arg2 = FHEencrypt(ctx, arg2_plain);

    FHEdouble result = compare_lt(ctx, arg1, arg2);

    FHEplainf result_plain = FHEdecrypt(ctx, result);
    std::cout << "Result: " << result_plain.getPlaintext() << std::endl;

    return 0;
}
