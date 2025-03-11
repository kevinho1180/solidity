// Checks that error is triggered no matter which order
contract D {
    function f() public {
        l();
    }
}
contract C {
    constructor() { new D(); }
}
function l() {
    s();
}
function s() {
    new C();
}
// ----
// Warning 8887: (149-156): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
// Warning 8887: (207-214): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
// TypeError 7813: (207-212): Circular reference to contract bytecode either via "new" or "type(...).creationCode" / "type(...).runtimeCode".
// TypeError 7813: (149-154): Circular reference to contract bytecode either via "new" or "type(...).creationCode" / "type(...).runtimeCode".
