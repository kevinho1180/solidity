// SPDX-License-Identifier: GPL-3.0
pragma solidity > 0.0;

contract A {}

contract Storage {
    uint256 public number;
    bytes public code = type(A).creationCode;

    function store(uint256 num) public {
        number = num;
    }
}
