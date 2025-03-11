contract C { function foo() internal { new D(); } }
contract D { function foo() internal { new C(); } }
// ----
// Warning 8887: (39-46): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
// Warning 8887: (91-98): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
