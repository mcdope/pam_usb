#!/usr/bin/bash

echo -e "Test:\t\t\tpamusb-check verifies the config previous tests created / reports granted"
echo "DEBUG: pamusb-check output ->"
pamusb-check --debug `whoami` && echo "error level 0 (success)" || echo "error level 1 (fail)"
echo "DEBUG: pamusb-check output -> grep -> "
pamusb-check `whoami` --debug | grep "granted" && echo "error level 0 (success)" || echo "error level 1 (fail)"
echo "DEBUG: actual check with exit level"
pamusb-check `whoami` 2>&1 | grep "granted" && echo -e "Result:\t\t\tPASSED!" || exit 1