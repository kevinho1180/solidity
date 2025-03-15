contract A {
    uint constant x = 10;
}

contract C is A layout at A.x { }
// ----
// TypeError 1505: (68-71): The base slot expression cannot be evaluated during compilation.
