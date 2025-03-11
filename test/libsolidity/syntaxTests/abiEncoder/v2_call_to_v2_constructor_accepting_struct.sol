==== Source: A ====
pragma abicoder               v2;

contract C {
    struct Item {
        uint x;
    }

    constructor(Item memory _item) {}
}
==== Source: B ====
pragma abicoder               v2;

import "A";

contract Test {
    function foo() public {
        new C(C.Item(5));
    }
}
// ----
// Warning 8887: (B:100-116): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
