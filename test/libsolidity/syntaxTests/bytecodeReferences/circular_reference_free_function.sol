function f()
{
	new D();
}

contract D
{
	receive() external payable { f; }
}
// ----
// Warning 8887: (16-23): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
// TypeError 7813: (16-21): Circular reference to contract bytecode either via "new" or "type(...).creationCode" / "type(...).runtimeCode".
