Commit this, push it and create a new PR. Wait till the CI has passed, if not: fix it.

When CI passed:
- Wait until gemini-code-assist has responded. Then get all comments.
- Review them, fix, commit, push, resolve comments (note that you must answer each comment, even if repetitive).
- Post "/gemini review" in the PR after that and wait until gemini-code-assist has responded. Then address the feedback.

Repeat this until gemini-code-assist (or DevSkim, or github-advanced-security, or the Maintainer) has no further feedback. If comments are unclear, respond with a detailed request for clarification.
