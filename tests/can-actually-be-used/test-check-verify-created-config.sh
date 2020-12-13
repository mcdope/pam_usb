#!/usr/bin/bash

echo -e "Test:\t\t\tpamusb-check verifies the config previous tests created / reports granted"
pamusb-check --debug `whoami`
pamusb-check `whoami` 2>&1 | grep "granted" && echo -e "Result:\t\t\tPASSED!" || exit 1