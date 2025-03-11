abstract contract X { function test() external virtual returns (uint256); }
contract Y is X {
    uint256 public override test = 42;
}
contract T {
    constructor() { new Y(); }
}
// ----
// Warning 8887: (168-175): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
