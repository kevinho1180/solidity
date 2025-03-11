contract C {
    function getArray() internal returns (uint256[10][1] storage x) {
        assembly {
            x.slot := sub(0, 1)
        }
    }
    function f() public returns (uint256[10] memory) {
        uint256[10][1] storage x = getArray();
        for (uint i = 0; i < 10; i++)
            x[0][i] = i;
        delete x[0];
        return x[0];
    }
}
// ----
// f() -> 0, 1, 2, 3, 4, 5, 6, 7, 8, 9
// gas legacy: 228661
