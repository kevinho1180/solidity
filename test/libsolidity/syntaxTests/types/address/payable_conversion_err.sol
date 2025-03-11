contract C {
    function f() public {
        address payable a = address(new D());

        // This conversion makes no sense anyway.
        address payable b = address(D);
    }
}

contract D {
    receive() external payable {
    }
}
// ----
// Warning 8887: (75-82): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
// TypeError 9574: (47-83): Type address is not implicitly convertible to expected type address payable.
// TypeError 9640: (164-174): Explicit type conversion not allowed from "type(contract D)" to "address".
// TypeError 9574: (144-174): Type address is not implicitly convertible to expected type address payable.
