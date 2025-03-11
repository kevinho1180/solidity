contract D {}
contract C is D {}
contract E is D
{
	function foo() public { new C(); }
}
// ----
// Warning 8887: (76-83): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
