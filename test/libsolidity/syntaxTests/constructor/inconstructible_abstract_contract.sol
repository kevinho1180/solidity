abstract contract C {
	constructor() {}
}
contract D {
	function f() public { C c = new C(); c; }
}
// ----
// TypeError 4614: (84-89): Cannot instantiate an abstract contract.
// Warning 8887: (84-91): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
