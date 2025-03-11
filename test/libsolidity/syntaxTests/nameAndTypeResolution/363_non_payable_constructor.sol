contract C {
    constructor() { }
}
contract D {
    function f() public returns (uint) {
        (new C){value: 2}();
        return 2;
    }
}
// ----
// TypeError 7006: (99-116): Cannot set option "value", since the constructor of contract C is not payable.
// Warning 8887: (99-118): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
