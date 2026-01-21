#include "../../src/backend/fhe_operations.hpp"
#include "../../src/backend/fhe_types.hpp"
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

using namespace CKKS;
using namespace CGGI;

void min_index(FHEcontext* v1, FHEdouble* v2, FHEdouble* v3) {
  size_t v4 = 0;
  FHEdouble v5 = FHEloadf(v1, v2, v4);
  size_t v6 = 1;
  size_t v7 = 10;
  size_t v8 = 1;
  FHEdouble v9;
  FHEdouble v10 = v5;
  for (size_t v11 = v6; v11 < v7; v11 += v8) {
    FHEdouble v12 = FHEloadf(v1, v2, v11);
    FHEdouble v13 = FHElt(v1, v12, v10);
    FHEdouble v14 = FHEselect(v1, v13, v12, v10);
    v10 = v14;
  }
  v9 = v10;
  FHEdouble v15 = FHEbroadcast(v1, v9);
  size_t v16 = 0;
  size_t v17 = 10;
  size_t v18 = 10;
  for (size_t v19 = v16; v19 < v17; v19 += v18) {
    double v20 = (double)0.0e+00;
    FHEdouble v21 = FHEencrypt(v1, v20);
    FHEdouble v22 = FHEloadf(v1, v2, v19);
    FHEdouble v23 = FHEeq(v1, v22, v15);
    FHEstoref(v1, v23, v3, v19);
  }
  return;
}



int main() {
    CKKS_scheme ck(15, 50, 16);
    CGGI_scheme cg(ck.getContext());
    FHEcontext* ctx = new FHEcontext(ck.getContext(), cg.getContext());

    constexpr size_t kDefaultArraySize = 10;

    const size_t arg1_size = kDefaultArraySize;
    std::vector<double> arg1_plain(arg1_size);
    for (size_t i = 0; i < arg1_plain.size(); ++i) {
        arg1_plain[i] = static_cast<double>(i);
    }
    std::vector<FHEdouble> arg1(arg1_size);
    for (size_t i = 0; i < arg1.size(); ++i) {
        arg1[i] = FHEencrypt(ctx, arg1_plain[i]);
    }

    const size_t arg2_size = kDefaultArraySize;
    std::vector<double> arg2_plain(arg2_size);
    for (size_t i = 0; i < arg2_plain.size(); ++i) {
        arg2_plain[i] = static_cast<double>(i);
    }
    std::vector<FHEdouble> arg2(arg2_size);
    for (size_t i = 0; i < arg2.size(); ++i) {
        arg2[i] = FHEencrypt(ctx, arg2_plain[i]);
    }

    min_index(ctx, arg1.data(), arg2.data());

    std::cout << "arg1: ";
    for (size_t i = 0; i < arg1.size(); ++i) {
        FHEplainf out_plain = FHEdecrypt(ctx, arg1[i]);
        std::cout << out_plain.getPlaintext();
        if (i + 1 < arg1.size()) {
            std::cout << " ";
        }
    }
    std::cout << std::endl;

    std::cout << "arg2: ";
    for (size_t i = 0; i < arg2.size(); ++i) {
        FHEplainf out_plain = FHEdecrypt(ctx, arg2[i]);
        std::cout << out_plain.getPlaintext();
        if (i + 1 < arg2.size()) {
            std::cout << " ";
        }
    }
    std::cout << std::endl;

    return 0;
}
