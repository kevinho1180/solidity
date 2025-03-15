bytes32 constant x = "ABC";
contract A layout at x {}
contract C layout at x[1] {}
// ----
// TypeError 6396: (49-50): The base slot of the storage layout must evaluate to a rational number.
// TypeError 6396: (75-79): The base slot of the storage layout must evaluate to a rational number.
