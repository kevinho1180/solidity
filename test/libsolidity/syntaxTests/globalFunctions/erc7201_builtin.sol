contract C {
    function f() public pure returns (uint) {
        return erc7201("ABC");
    }
}
// ----
// UnimplementedFeatureError 1834: (0-97): Codegen does not support erc7201 builtin yet.
