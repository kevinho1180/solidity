contract D {}
contract C {
    function f() public view { new D(); }
}
// ----
// Warning 8887: (58-65): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
// TypeError 8961: (58-65): Function cannot be declared as view because this expression (potentially) modifies the state.
