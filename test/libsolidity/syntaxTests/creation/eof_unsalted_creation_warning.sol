contract C {}

contract D {
    C c1 = new C();                          // warning
    C c2 = new C{salt: 0}();                 // ok
    C c3 = new C{salt: keccak256("salt")}(); // ok
}
// ====
// bytecodeFormat: >=EOFv1
// ----
// Warning 8887: (39-46): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
