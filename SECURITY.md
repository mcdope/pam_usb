# Security Policy

## Supported Versions

At every point in time only the most recent release is supported.

| Version | Supported          |
|---------| ------------------ |
| 0.8.7   | ✅                 |
| < 0.8.7 | ❌                 |

## Reporting a Vulnerability

Please email me at tobiasbaeumer@gmail.com, since Github issues are public.

## What Happens After You Report

After receiving your report, I will review it and create a GitHub Security Advisory (GHSA) to track the vulnerability privately. For critical findings I will also request a CVE identifier. You will be credited in the advisory unless you prefer to remain anonymous.

## Submitting a Fix

Once a vulnerability has been reported via email and coordinated with the maintainer, submit the fix as a pull request following the standard [CONTRIBUTING](CONTRIBUTING) guidelines with these additions:

- Reference the GitHub Security Advisory (GHSA) identifier in the PR description instead of a public issue (e.g., `Refs GHSA-xxxx-xxxx-xxxx`). If no advisory exists yet, reference issue #55 as a placeholder.
- **Do not** include the full attack scenario in the public PR description until the advisory has been published. A brief note such as "Fixes a vulnerability disclosed privately — details in the security advisory" is sufficient until then.
- Once the advisory is published, the PR description **should** be updated to include the full rationale and attack scenario as required by CONTRIBUTING.

## Note on versions <= 0.8.6

There are multiple published security advisories for these versions.

If you're running anything except 0.8.7 please update immediately!
