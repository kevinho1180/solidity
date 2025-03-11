interface I {
    function f(uint[] calldata) external pure;
}
contract A is I {
    function f(uint[] memory) public pure {}
}
contract C {
    function f() public {
        I i = I(new A());
        i.f(new uint[](1));
    }
}
// ----
// Warning 8887: (183-190): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
