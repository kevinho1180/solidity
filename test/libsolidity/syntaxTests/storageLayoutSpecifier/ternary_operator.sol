contract A layout at true ? 42 : 94 {}
contract B layout at 255 + (true ? 1 : 0) {}
// ----
// TypeError 1505: (21-35): The base slot expression cannot be evaluated during compilation.
// TypeError 1505: (60-80): The base slot expression cannot be evaluated during compilation.
