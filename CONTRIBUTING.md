# Contributing to Feather

Thank you for your interest in contributing to Feather! This document provides guidelines and instructions for contributing to the project.

## Code of Conduct

By participating in this project, you agree to abide by the following code of conduct:

- Be respectful and inclusive towards all community members
- Use welcoming and inclusive language
- Be empathetic towards other community members
- Focus on what is best for the community
- Show courtesy and respect towards other community members
- Accept constructive criticism gracefully
- Help create a positive environment for everyone

Unacceptable behavior includes:
- The use of sexualized language or imagery
- Trolling, insulting/derogatory comments, and personal or political attacks
- Public or private harassment
- Publishing others' private information without explicit permission
- Other unethical or unprofessional conduct

Project maintainers have the right and responsibility to remove, edit, or reject comments, commits, code, wiki edits, issues, and other contributions that are not aligned to this Code of Conduct.

## How to Contribute

### Reporting Bugs

Before creating a bug report, please check if the issue has already been reported. If it has, add a comment to the existing issue instead of creating a new one.

When creating a bug report, please include:
- A clear description of the issue
- Steps to reproduce the problem
- Expected behavior
- Actual behavior
- Your environment (OS, compiler version, etc.)
- Any relevant code snippets

### Suggesting Enhancements

Enhancement suggestions are tracked as GitHub issues. When creating an enhancement suggestion, please include:
- A clear description of the feature
- Why this enhancement would be useful
- Any specific implementation ideas you have

### Pull Requests

1. Fork the repository
2. Create a new branch (`git checkout -b feature/amazing-feature`)
3. Make your changes
4. Run the tests (`make test`)
5. Commit your changes (`git commit -m 'Add some amazing feature'`)
6. Push to the branch (`git push origin feature/amazing-feature`)
7. Open a Pull Request

### Development Setup

1. Clone the repository:
```bash
git clone https://github.com/yourusername/feather.git
cd feather
```

2. Set up your development environment:
```bash
export PATH="$PWD/include:$PWD/bin:$PATH"
```

3. Run tests:
```bash
make test
```

## Coding Standards

### C++ Style Guide

- Follow modern C++ best practices
- Use C++17 or later features when appropriate
- Follow the existing code style, (aiming for functional) in the project
- Use meaningful variable and function names
- Add comments for complex logic
- Keep functions focused and small

### Commit Messages

- Use the present tense ("add: feature" not "Added feature")
- Use the imperative mood ("Move cursor to..." not "Moves cursor to...")
- Limit the first line to 72 characters or less
- Reference issues and pull requests liberally after the first line

## Testing

- Write unit tests for new features
- Ensure all tests pass before submitting a PR
- Add integration tests for significant changes
- Update existing tests when modifying functionality

## Documentation

- Update the README.md if needed
- Add inline documentation for new functions
- Update the API documentation
- Include examples for new features

## Review Process

1. All pull requests require at least one review
2. Address review comments promptly
3. Keep the PR focused and manageable
4. Update the PR description as needed

## Questions?

If you have any questions, feel free to:
1. Open an issue
2. Join our community discussions
3. Contact the maintainers

Thank you for contributing to Feather! 