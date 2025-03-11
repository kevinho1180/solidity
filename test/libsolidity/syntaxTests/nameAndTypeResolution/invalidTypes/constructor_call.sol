contract C {
    constructor(bytes32 _arg) {
    }
}

contract A {
    function f() public {
        new  C((1234));
    }
}
// ----
// TypeError 9553: (108-114): Invalid type for argument in function call. Invalid implicit conversion from int_const 1234 to bytes32 requested.
// Warning 8887: (101-115): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
