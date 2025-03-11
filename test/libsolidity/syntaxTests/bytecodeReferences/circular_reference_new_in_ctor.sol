contract C {
	constructor() { new C(); }
}
// ----
// Warning 8887: (30-37): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
// TypeError 7813: (30-35): Circular reference to contract bytecode either via "new" or "type(...).creationCode" / "type(...).runtimeCode".
