contract A {
    function a() public pure {
    }
}
contract B {
    constructor(C) {
    }
    function b(C) public returns (A) {
        return new A();
    }
}
contract C {
    B m_b;
    C m_c;
    constructor(C other_c) {
        m_c = other_c;
        m_b = new B(this);
        m_b.b(this).a();
        g(this).f();
        other_c.f();
        m_c.f();
    }
    function f() public pure {
    }
    function g(C) public view returns (C) {
        return m_c;
    }
}
// ----
// Warning 8887: (146-153): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
// Warning 8887: (264-275): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
