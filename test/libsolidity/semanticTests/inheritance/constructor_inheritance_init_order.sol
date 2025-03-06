contract A {
    uint x;
    constructor() {
        x = 42;
    }
    function f() public returns(uint256) {
        return x;
    }
}
contract B is A {
    uint public y = f();
}
// ====
// compileViaYul: true
// ----
// constructor() ->
// gas irOptimized: 99391
// gas irOptimized code: 19400
// y() -> 42
