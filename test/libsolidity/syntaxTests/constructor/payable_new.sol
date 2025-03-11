contract A1 {}
contract B1 is A1 { constructor() payable {} }

contract A2 { constructor() {} }
contract B2 is A2 { constructor() payable {} }

contract B3 { constructor() payable {} }

contract C {
	function f() public payable {
		new B1{value: 10}();
		new B2{value: 10}();
		new B3{value: 10}();
	}
}
// ----
// Warning 8887: (232-251): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
// Warning 8887: (255-274): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
// Warning 8887: (278-297): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
