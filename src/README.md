<h1>The CHESS Compiler</h1>
Our Compiler framework consists of a frontend, which holds compiler logic to read MLIR input code. The backend has logic for our 
custom wrapper language around the OpenFHE API.


<h2>Frontend</h2>
It includes a file named "compiler.cpp", which holds the logic for our compiler frontend. It converts arithmetic and bitwise
operations which are in the form of "arith.add" and "arith.xor". It also has comparison operations such as "arith.cmp" and an
additional flag showing what type of comparison operation is being used, like "eq" for equality. 

We also support ternary operations, which are in the form "arith.select" operation, where it has a select bit, and based on the
bit chooses between the two values.
To support the vectorization pass, we run the MLIR vectorization pass below:
```
mlir-opt -affine-super-vectorize="virtual-vector-size=10 test-fastest-varying=0 vectorize-reductions=true" 
```
where the vector size can be changed to the batch size supported by the CKKS parameters.
We convert the vector operations to their homomorphic equivalent.
We also include a test.mlir, where it compiles to output.mlir and with the vectorization pass to output1.mlir.




<h2>Backend</h2>
It includes wrapper functions around the OpenFHE API. We use the BinFHE API from OpenFHE for the CGGI (TFHE) types of operations,
and use the PKE API from OpenFHE for the CKKS operations.
The "fhe_type.hpp" and "fhe_operations.hpp" include logic for the class objects and functions for the CKKS and CGGI wrapper functions.
The cggi_operations.cpp and ckks_operations.cpp hold the function logic for the CGGI and CKKS schemes respectively.
