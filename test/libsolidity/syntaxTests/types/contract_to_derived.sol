contract B {}
contract A is B {}
contract C {
  function f() public pure {
    A a = A(new B());
  }
}
// ----
// Warning 8887: (87-94): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
// TypeError 9640: (85-95): Explicit type conversion not allowed from "contract B" to "contract A".
