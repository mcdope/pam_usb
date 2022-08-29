#!/usr/bin/bash

echo -e "Test:\t\t\tpamusb-check verifies the config previous tests created / reports granted"
pamusb-check --debug `whoami` && echo -e "Result:\t\t\tPASSED!" || exit 1