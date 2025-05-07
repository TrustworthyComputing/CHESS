<h1>The CHESS Compiler</h1>
Our CHESS compiler framework comprises a frontend, which holds compiler logic to read MLIR input code, and a backend that implements logic for our 
custom wrapper language around the OpenFHE API.


<h2>Frontend</h2>

This part of our framework includes a file named `compiler.cpp`, which holds the logic for our compiler frontend. It converts arithmetic and bitwise
operations that are in the form of `arith.add` and `arith.xor`. It also incorporates comparison operations such as `arith.cmp` and
additional flags showing what type of comparison operation is being used, like `eq` for equality. 

In CHESS, we also support ternary operations, which are in the form `arith.select` operation, which has a select bit, and are based on the
bit chooses between the two values.
To support our vectorization pass, we run the MLIR vectorization pass below:
```
mlir-opt -affine-super-vectorize="virtual-vector-size=10 test-fastest-varying=0 vectorize-reductions=true" 
```
where the vector size can be changed to the batch size supported by the corresponding CKKS parameters.
During this process, we convert the vector operations to their homomorphic equivalent.
We also include a `test.mlir`, where it compiles to `output.mlir` and with the vectorization pass to `output1.mlir`.




<h2>Backend</h2>

This part of the framework includes custom wrapper functions around the OpenFHE API. In particular, we employ the BinFHE API from OpenFHE for the CGGI (TFHE) types of operations,
as well as the PKE API from OpenFHE for the CKKS operations.
The `fhe_type.hpp` and `fhe_operations.hpp` include logic for the class objects and functions for the CKKS and CGGI wrapper functions.
Moreover, the `cggi_operations.cpp` and `ckks_operations.cpp` hold the function logic for the CGGI and CKKS schemes respectively.
