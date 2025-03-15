contract A {
    function f() public {}
}
contract C is A layout at uint32(this.f.selector) {}
// ----
// TypeError 1505: (68-91): The base slot expression cannot be evaluated during compilation.
