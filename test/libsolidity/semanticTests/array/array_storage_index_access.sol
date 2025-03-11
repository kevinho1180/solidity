contract C {
    uint[] storageArray;
    function test_indices(uint256 len) public
    {
        while (storageArray.length < len)
            storageArray.push();
        while (storageArray.length > len)
            storageArray.pop();
        for (uint i = 0; i < len; i++)
            storageArray[i] = i + 1;

        for (uint i = 0; i < len; i++)
            require(storageArray[i] == i + 1);
    }
}
// ----
// test_indices(uint256): 1 ->
// test_indices(uint256): 129 ->
// gas irOptimized: 2994583
// gas legacy: 3038668
// gas legacyOptimized: 2995964
// test_indices(uint256): 5 ->
// gas irOptimized: 573884
// gas legacy: 573821
// gas legacyOptimized: 571847
// test_indices(uint256): 10 ->
// gas irOptimized: 156398
// gas legacy: 160122
// gas legacyOptimized: 156996
// test_indices(uint256): 15 ->
// gas irOptimized: 170593
// gas legacy: 175987
// gas legacyOptimized: 171596
// test_indices(uint256): 0xFF ->
// gas irOptimized: 5629033
// gas legacy: 5715762
// gas legacyOptimized: 5632556
// test_indices(uint256): 1000 ->
// gas irOptimized: 18009740
// gas legacy: 18347824
// gas legacyOptimized: 18037248
// test_indices(uint256): 129 ->
// gas irOptimized: 4117214
// gas legacy: 4140124
// gas legacyOptimized: 4108272
// test_indices(uint256): 128 ->
// gas irOptimized: 390418
// gas legacy: 433512
// gas legacyOptimized: 400909
// test_indices(uint256): 1 ->
// gas irOptimized: 577899
// gas legacy: 576726
// gas legacyOptimized: 575542
