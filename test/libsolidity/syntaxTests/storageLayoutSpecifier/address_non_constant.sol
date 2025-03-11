contract A {}

contract C layout at address(new A()) {}
contract D layout at uint160(address(this)) {}
// ----
// Warning 8887: (44-51): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
// TypeError 1139: (36-52): The base slot of the storage layout must be a compile-time constant expression.
// TypeError 1139: (77-99): The base slot of the storage layout must be a compile-time constant expression.
