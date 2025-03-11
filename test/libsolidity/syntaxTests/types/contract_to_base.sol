contract A {}
contract B is A {}
contract C {
  function f() public {
    A a = new B();
    a;
  }
}
// ----
// Warning 8887: (80-87): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
