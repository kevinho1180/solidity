contract A {
    function f() public {
        new B();
    }
}


contract B {
    function f() public {}
}


contract C {
    function f() public {
        new B();
    }
}
// ----
// constructor() ->
// gas irOptimized: 56915
// gas irOptimized code: 43200
