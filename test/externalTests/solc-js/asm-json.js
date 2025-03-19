const tape = require('tape');
const fs = require('fs');
const solc = require('../index.js');

tape('ASM Json output consistency', function (t) {
    t.test('Nested structure', function(st) {
        const testdir = 'test/';
        const output = JSON.parse(solc.compile(JSON.stringify({
            language: 'Solidity',
            settings: {
                viaIR: true,
                outputSelection: {
                    '*': {
                        '*': ['evm.legacyAssembly']
                    }
                }
            },
            sources: {
                C: {
                    content: fs.readFileSync(testdir + 'code_access_runtime.sol', 'utf8')
                }
            }
        })));
        st.ok(output);

        function containsTarget(obj, target) {
            if (Array.isArray(obj))
                return obj.some(item => containsTarget(item, target));
            else if (typeof obj === 'object' && obj !== null) {
                if (obj.name === target.name && obj.value === target.value)
                    return true;
                return Object.values(obj).some(value => containsTarget(value, target));
            }
            return false;
        }

        // regression test that there is indeed a negative subassembly index in there
        // and it is based on a 64 bit uint
        const targetObject = {
            "name": "PUSH #[$]",
            "value": "000000000000000000000000000000000000000000000000ffffffffffffffff"
        };
        st.equal(containsTarget(output, targetObject), true)
    });
});
