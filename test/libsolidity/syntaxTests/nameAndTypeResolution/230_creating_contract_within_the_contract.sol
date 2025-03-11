contract Test {
    function f() public { Test x = new Test(); }
}
// ----
// Warning 8887: (51-61): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
// TypeError 7813: (51-59): Circular reference to contract bytecode either via "new" or "type(...).creationCode" / "type(...).runtimeCode".
