contract C {
	constructor() internal {}
}
contract D {
	function f() public { C c = new C(); c; }
}
// ----
// DeclarationError 1845: (14-39): Non-abstract contracts cannot have internal constructors. Remove the "internal" keyword and make the contract abstract to fix this.
// Warning 8887: (84-91): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
