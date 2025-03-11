contract C {
    function f() public {
        address payable a = payable(address(new D()));
        address payable b = payable(new E());
        address payable c = payable(new F());

        a;
        b;
        c;
    }
}

// A contract that cannot receive Ether
contract D {}

// A contract that can receive Ether
contract E {
    receive() external payable {
    }
}

// A contract that can receive Ether using the fallback
contract F {
    fallback() external payable {

    }
}

// ----
// Warning 8887: (83-90): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
// Warning 8887: (130-137): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
// Warning 8887: (176-183): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
