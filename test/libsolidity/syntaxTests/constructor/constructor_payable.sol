contract C {
    constructor() payable { }
}

contract D {
    function createC() public returns (C) {
       C c = (new C){value: 1}();
       return c;
    }
}
// ----
// Warning 8887: (116-135): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
