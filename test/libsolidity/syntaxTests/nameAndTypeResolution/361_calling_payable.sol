contract receiver { function pay() payable public {} }
contract test {
    function f() public { (new receiver()).pay{value: 10}(); }
    receiver r = new receiver();
    function h() public { r.pay{value: 10}(); }
}
// ----
// Warning 8887: (98-112): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
// Warning 8887: (151-165): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
