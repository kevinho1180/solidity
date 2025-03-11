contract A {}
contract B {}
contract C {
  function f() public pure {
    B b = B(new A());
  }
}
// ----
// Warning 8887: (82-89): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
// TypeError 9640: (80-90): Explicit type conversion not allowed from "contract A" to "contract B".
