contract base { function foo() public virtual; }
contract derived {
    base b;
    function foo() public { b = new base(); }
}
// ----
// TypeError 3656: (0-48): Contract "base" should be marked as abstract.
// Warning 8887: (112-122): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
