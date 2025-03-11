library L1 {
    function foo() internal { new A(); }
}
library L2 {
    function foo() internal { L1.foo(); }
}
contract A {
    function f() public pure { L2.foo(); }
}
// ----
// Warning 8887: (43-50): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
// TypeError 7813: (43-48): Circular reference to contract bytecode either via "new" or "type(...).creationCode" / "type(...).runtimeCode".
