abstract contract X { function test() internal virtual returns (uint256); }
contract Y is X {
    uint256 public override test = 42;
}
contract T {
    constructor() { new Y(); }
}
// ----
// TypeError 5225: (98-131): Public state variables can only override functions with external visibility.
// Warning 8887: (168-175): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
