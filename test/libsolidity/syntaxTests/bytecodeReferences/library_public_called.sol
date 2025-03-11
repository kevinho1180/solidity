library L1 {
    function foo() internal { new A(); }
}
library L2 {
    function foo() public { L1.foo(); }
}
contract A {
    function f() public { L2.foo(); }
}
// ----
// Warning 8887: (43-50): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
