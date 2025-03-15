==== Source: A ====
uint constant x = 77;

==== Source: B ====
import "A" as M;
contract C layout at M.x{ }
// ----
// TypeError 1505: (B:38-41): The base slot expression cannot be evaluated during compilation.
