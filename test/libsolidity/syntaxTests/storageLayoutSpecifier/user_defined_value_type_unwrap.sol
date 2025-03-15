type MyUint is uint128;
MyUint constant x = MyUint.wrap(42);
contract C layout at MyUint.unwrap(x) {}
// ----
// TypeError 1505: (82-98): The base slot expression cannot be evaluated during compilation.
