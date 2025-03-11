// Checks that error is triggered no matter which order
function l() {
    s();
}
function s() {
	new C();
}
contract D {
	function f() public {
		l();
	}
}
contract C {
	constructor() { new D(); }
}
// ----
// Warning 8887: (98-105): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
// Warning 8887: (187-194): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
// TypeError 7813: (98-103): Circular reference to contract bytecode either via "new" or "type(...).creationCode" / "type(...).runtimeCode".
// TypeError 7813: (187-192): Circular reference to contract bytecode either via "new" or "type(...).creationCode" / "type(...).runtimeCode".
