uint constant N = 100;
contract C layout at N / 0 {}
// ----
// TypeError 1505: (44-49): The base slot expression cannot be evaluated during compilation.
