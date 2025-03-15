bool constant x = false;
contract C layout at x {}
// ----
// TypeError 6396: (46-47): The base slot of the storage layout must evaluate to a rational number.
