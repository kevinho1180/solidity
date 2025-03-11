abstract contract X { function test() private virtual returns (uint256); }
contract Y is X {
    uint256 public override test = 42;
}
contract T {
    constructor() { new Y(); }
}
// ----
// TypeError 5225: (97-130): Public state variables can only override functions with external visibility.
// TypeError 3942: (22-72): "virtual" and "private" cannot be used together.
// Warning 8887: (167-174): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
