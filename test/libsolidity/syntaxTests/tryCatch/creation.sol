contract D {
}
contract C {
    function f() public {
        try new D() {
        } catch (bytes memory x) {
            x;
        }
    }
}
// ====
// EVMVersion: >=byzantium
// ----
// Warning 8887: (66-73): Contract creation on EOF does not use account nonce and will deploy to the same address when invoked multiple times with the same contract.Explicitly set the "salt" call option to generate unique addresses.
