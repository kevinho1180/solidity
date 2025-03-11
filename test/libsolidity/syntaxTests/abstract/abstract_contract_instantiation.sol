abstract contract AbstractContract {
    constructor() { }
    function utterance() public returns (bytes32) { return "miaow"; }
}

contract Test {
    function create() public {
       AbstractContract ac = new AbstractContract();
    }
}
// ----
// TypeError 4614: (208-228): Cannot instantiate an abstract contract.
// Warning 8887: (208-230): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
