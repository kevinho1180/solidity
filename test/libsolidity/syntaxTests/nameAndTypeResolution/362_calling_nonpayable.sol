contract receiver { function nopay() public {} }
contract test {
    function f() public { (new receiver()).nopay{value: 10}(); }
    function g() public { (new receiver()).nopay.value(10)(); }
}
// ----
// Warning 8887: (92-106): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
// TypeError 7006: (91-124): Cannot set option "value" on a non-payable function type.
// Warning 8887: (157-171): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
// TypeError 8820: (156-184): Member "value" is only available for payable functions.
