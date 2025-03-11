// Previously, the type information for A was not yet available at the point of
// "new A".
contract B {
	A a;
	constructor() {
		a = new A(address(this));
	}
}
contract A {
	constructor(address) internal {}
}
// ----
// Warning 8887: (134-154): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
// DeclarationError 1845: (175-207): Non-abstract contracts cannot have internal constructors. Remove the "internal" keyword and make the contract abstract to fix this.
