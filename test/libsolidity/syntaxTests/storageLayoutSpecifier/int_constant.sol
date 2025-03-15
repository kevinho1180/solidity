int constant x = -42;
contract C layout at x {}
// ----
// TypeError 1481: (43-44): The base slot expression must have an unsigned integer type.
