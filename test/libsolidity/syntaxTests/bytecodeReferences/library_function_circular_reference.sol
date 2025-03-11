library L {
    function f() internal {
        new C();
    }
}

contract D {
    function f() public {
        L.f();
    }
}
contract C {
    constructor() { new D(); }
}

// ----
// Warning 8887: (48-55): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
// Warning 8887: (161-168): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
// TypeError 7813: (48-53): Circular reference to contract bytecode either via "new" or "type(...).creationCode" / "type(...).runtimeCode".
// TypeError 7813: (161-166): Circular reference to contract bytecode either via "new" or "type(...).creationCode" / "type(...).runtimeCode".
