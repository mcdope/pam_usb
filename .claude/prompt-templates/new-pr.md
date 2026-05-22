Commit this, push it and create a new PR. Wait till the CI has passed, if not: fix it.

When CI passed:
- Wait until gemini-code-assist has responded. Then get all comments. Note that Gemini sometimes responds as a review, sometimes as a plain comment.
- Review them, fix, commit, push. Reply to every comment thread (even if repetitive). Then resolve all threads using GraphQL:

```bash
# Get thread IDs
gh api graphql -f query='{ repository(owner: "mcdope", name: "pam_usb") { pullRequest(number: PR_NUMBER) { reviewThreads(first: 50) { nodes { id isResolved } } } } }'

# Resolve each unresolved thread
gh api graphql -f query='mutation { resolveReviewThread(input: {threadId: "THREAD_ID"}) { thread { isResolved } } }'
```

Repeat this until gemini-code-assist (or DevSkim, or github-advanced-security, or the Maintainer) has no further feedback.
If comments are unclear, respond with a detailed request for clarification. After each change update the PR description.
