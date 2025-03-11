contract C {
    function balance() public returns (uint) {
        this.balance; // to avoid pureness warning
        return 1;
    }
    function transfer(uint amount) public {
        payable(this).transfer(amount); // to avoid pureness warning
    }
    receive() payable external {
    }
}
contract D {
    function f() public {
        uint x = (new C()).balance();
        x;
        (new C()).transfer(5);
    }
}
// ----
// Warning 8887: (352-359): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
// Warning 8887: (392-399): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
// Warning 2018: (17-134): Function state mutability can be restricted to view
