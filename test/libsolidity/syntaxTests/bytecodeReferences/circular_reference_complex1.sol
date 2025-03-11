contract A { function foo() public { new D(); } }
contract C { function foo() public { new A(); } }
contract D is C {}
// ----
// Warning 8887: (37-44): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
// Warning 8887: (87-94): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
// TypeError 7813: (37-42): Circular reference to contract bytecode either via "new" or "type(...).creationCode" / "type(...).runtimeCode".
// TypeError 7813: (87-92): Circular reference to contract bytecode either via "new" or "type(...).creationCode" / "type(...).runtimeCode".
