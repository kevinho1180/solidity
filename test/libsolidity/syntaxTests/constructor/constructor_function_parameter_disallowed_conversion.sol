contract D {
    constructor(function() external pure returns (uint) ) {
    }
}

library L {
    function f() public pure returns (uint) {
        return 5;
    }
}

contract C {
    function f() public returns (uint r) {
        // An assert used to fail if the function types are not exactly equal (pure, view) v/s
        // default
        // ok
        new D(this.testPure);
        // not okay
        new D(this.testView);
        // not okay
        new D(this.testDefault);
        // not okay
        new D(this.testDifferentSignature);
        // not okay
        new D(gasleft);
        // not okay
        new D(L.f);
        // not okay
        new D(testInternalFunction);
        // not okay
        new D(this.testInternalFunction);
    }

    function testPure() public pure returns (uint) {
    }

    function testView() public view returns (uint) {
        block.timestamp;
    }

    function testDefault() public returns (uint) {
        selfdestruct(payable(address(this)));
    }

    function testDifferentSignature(uint a) public pure returns (uint) {
    }

    function testInternalFunction() internal pure returns (uint) {
        return 10;
    }
}
// ----
// Warning 8887: (359-379): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
// TypeError 9553: (415-428): Invalid type for argument in function call. Invalid implicit conversion from function () view external returns (uint256) to function () pure external returns (uint256) requested.
// Warning 8887: (409-429): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
// TypeError 9553: (465-481): Invalid type for argument in function call. Invalid implicit conversion from function () external returns (uint256) to function () pure external returns (uint256) requested.
// Warning 8887: (459-482): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
// TypeError 9553: (518-545): Invalid type for argument in function call. Invalid implicit conversion from function (uint256) pure external returns (uint256) to function () pure external returns (uint256) requested.
// Warning 8887: (512-546): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
// TypeError 9553: (582-589): Invalid type for argument in function call. Invalid implicit conversion from function () view returns (uint256) to function () pure external returns (uint256) requested. Special functions cannot be converted to function types.
// Warning 8887: (576-590): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
// TypeError 9553: (626-629): Invalid type for argument in function call. Invalid implicit conversion from function () pure returns (uint256) to function () pure external returns (uint256) requested. Special functions cannot be converted to function types.
// Warning 8887: (620-630): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
// TypeError 9553: (666-686): Invalid type for argument in function call. Invalid implicit conversion from function () pure returns (uint256) to function () pure external returns (uint256) requested. Special functions cannot be converted to function types.
// Warning 8887: (660-687): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
// TypeError 9582: (723-748): Member "testInternalFunction" not found or not visible after argument-dependent lookup in contract C.
