bytes32 constant b = "Solidity";
contract C layout at uint8(b[1]) {}
// ----
// TypeError 1505: (54-65): The base slot expression cannot be evaluated during compilation.
