contract C {
    uint constant x = erc7201("abc");
    function f() public pure returns(uint) {
        return x;
    }
}
// ----
// UnimplementedFeatureError 1834: (0-121): Codegen does not support erc7201 builtin yet.
