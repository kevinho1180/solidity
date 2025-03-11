contract A {}
contract B is A {}
contract C is B {}
contract D {
  function f() public {
    A a = new C();
    a;
  }
}
// ----
// Warning 8887: (99-106): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
