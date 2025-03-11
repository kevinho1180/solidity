contract D {}
contract C {
    function f() public { new D(); }
}
// ----
// Warning 8887: (53-60): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
