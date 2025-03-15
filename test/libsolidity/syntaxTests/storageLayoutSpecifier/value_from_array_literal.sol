contract A layout at [1, 2, 3][0] {}
contract B layout at 255 + [1, 2, 3][0] {}
// ----
// TypeError 1505: (21-33): The base slot expression cannot be evaluated during compilation.
// TypeError 1505: (58-76): The base slot expression cannot be evaluated during compilation.
