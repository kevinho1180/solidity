contract C {
    constructor() payable {}
}
contract D {
    function createC() public returns (C) {
        C c = (new C).value(2)();
        return c;
    }
}
// ----
// TypeError 1621: (115-128): Using ".value(...)" is deprecated. Use "{value: ...}" instead.
// Warning 8887: (115-133): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
