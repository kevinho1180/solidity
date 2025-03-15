uint constant x = uint(42);
contract C layout at x {}
// ----
// TypeError 1505: (49-50): The base slot expression cannot be evaluated during compilation.
