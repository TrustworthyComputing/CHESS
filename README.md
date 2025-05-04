<h1 align="center">CHESS: Compiling Homomorphic Encryption with Scheme Switching <a href="https://github.com/TrustworthyComputing/gpt-thief/blob/main/LICENSE"><img src="https://img.shields.io/badge/license-MIT-blue.svg"></a> </h1>

## Overview
This project implements the scheme switching methodology introduced in CHESS (IEEE HOST'25), enabling secure and efficient computations by seamlessly bridging the CKKS and CGGI fully homomorphic encryption (FHE) schemes. By adopting the MLIR framework, the CHESS compiler translates high-level C++ code into an intermediate representation (IR) that orchestrates dynamic, mid-execution switching between FHE schemes. This approach ensures that arithmetic-intensive operations benefit from CKKS batching, while nonlinear functions fully exploit the strengths of the CGGI scheme. In addition to core switching techniques, this repository introduces innovative methods to ensure ciphertext compatibility, enable high-precision homomorphic operations, and generate optimized FHE code for an OpenFHE backend.


### How to cite this work
This work has been presented at the 2025 IEEE Hardware Oriented Security and Trust (HOST) symposium. You can cite this work as follows:
```
@inproceedings{shokri2025chess,
    author = {Rostin Shokri and Nektarios Georgios Tsoutsos},
    title = {{CHESS: Compiling Homomorphic Encryption with Scheme Switching}},
    booktitle={International Symposium on Hardware Oriented Security and Trust (HOST)},
    pages={1--11},
    year={2025},
    organization={IEEE}
}
```

## Acknowledgments
This work was supported by the National Science Foundation (Award #2239334).

<p align="center">
    <img src="./logos/twc.png" height="20%" width="20%">
</p>
<h4 align="center">Trustworthy Computing Group</h4>
