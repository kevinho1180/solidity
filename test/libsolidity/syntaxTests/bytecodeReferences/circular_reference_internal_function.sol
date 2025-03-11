contract C
{
	// Internal uncalled function should not cause a cyclic dep. error
	function foo() internal { new D(); }
	function callFoo() virtual public { foo(); }
}

contract D is C
{
	function callFoo() override public {}
}
// ----
// Warning 8887: (108-115): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
