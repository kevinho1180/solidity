interface I {}

contract C layout at uint(bytes32(type(I).interfaceId)) { }
// ----
// TypeError 1505: (37-71): The base slot expression cannot be evaluated during compilation.
