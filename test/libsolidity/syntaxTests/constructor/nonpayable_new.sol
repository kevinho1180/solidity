contract A1 { constructor() {} }
contract B1 is A1 {}

contract A2 { constructor() payable {} }
contract B2 is A2 {}

contract B3 {}

contract B4 { constructor() {} }

contract C {
	function f() public payable {
		new B1{value: 10}();
		new B2{value: 10}();
		new B3{value: 10}();
		new B4{value: 10}();
	}
}
// ----
// TypeError 7006: (214-231): Cannot set option "value", since the constructor of contract B1 is not payable.
// Warning 8887: (214-233): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
// TypeError 7006: (237-254): Cannot set option "value", since the constructor of contract B2 is not payable.
// Warning 8887: (237-256): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
// TypeError 7006: (260-277): Cannot set option "value", since the constructor of contract B3 is not payable.
// Warning 8887: (260-279): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
// TypeError 7006: (283-300): Cannot set option "value", since the constructor of contract B4 is not payable.
// Warning 8887: (283-302): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
