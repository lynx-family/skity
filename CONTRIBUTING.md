# Contributing to Skity

First off, thank you for considering contributing to Skity!
We welcome you to join Lynx Authors and become a member.
It's people like you who make this project great.

## How Can I Contribute?

### Reporting Bugs

If you find a bug, please open an issue with the following details:

- A clear and descriptive title for the issue.
- A description of the steps to reproduce the issue.
- Any additional information or screenshots that might help us understand the issue better.

### Suggesting Enhancements

We’re always open to new ideas! If you have a suggestion, please:

- Use the “Feature Request” issue template or create a new issue.
- Describe the enhancement you’d like and explain why it would be useful.

### Your First Code Contribution

Unsure where to start? You can find beginner-friendly issues using the “good first issue” label.
Working on these issues helps you get familiar with the codebase before tackling more complex problems.

### Pull Requests

When you’re ready to make a code change, please create a Pull Request:

1. Fork the repository and clone it to your local machine.
2. Create a new branch: `git checkout -b name-of-your-branch`.
3. Make your changes.
4. Once you have finished the necessary tests and verifications locally,
   commit the changes.
5. Push the changes to your remote branch and start a Pull Request.
   > We encourage the submission of small patches and only accept PRs that contain a single commit. Therefore, please
   split your PR into separate ones if it contains multiple commits, or squash them into a single commit if there are
   not too many changes.
   > The CI workflow will reject any PR that contains more than one commit.
6. Make sure that your Pull Request adheres to the style guide and is properly documented.

## Verifying and Reviewing Pull Requests

A Pull Request needs to be verified by the CI workflows and reviewed by the Lynx authors before being merged.

Once you submit a Pull Request, you can invite the contributors of the repository to review your changes.
If you have no idea whom to invite to review your changes,
the GitHub branch protection rules and `git blame` are the right places to start.

While any contributor can review your changes, at least one of the authors from
[default reviewers](./DEFAULT_REVIEWERS) should be on the reviewer list.
Default reviewers can help trigger the CI workflow run to verify the changes
(if this is your first PR for Skity, you'll see a pending approval on the PR discussion area after you submit the PR)
and then start the landing process.

> The workflow run needs to be triggered by default reviewers so they can ensure the new changes
  will not introduce any risks.

Typically, a Pull Request will be reviewed **within one week**.
The landing process will be triggered manually if the changes pass all CI checks and are ready to be merged.


## Code Style

Our project adheres to the coding style guidelines provided by Google.
You can find the detailed guidelines here: [Google's Style Guides](https://google.github.io/styleguide/).

Following these style guides helps ensure that our code is consistent, clear, and of high quality.

Special thanks to Google for making these comprehensive style guides available for developers!

## Testing

Make sure your changes are covered by tests, if applicable.
Run the existing tests to ensure everything works as expected.

## Code of Conduct

Please note that all participants in this project are expected to uphold our [Code of Conduct](CODE_OF_CONDUCT.md).
By participating, you agree to abide by its terms.

We're excited to see your contributions! Thank you!
