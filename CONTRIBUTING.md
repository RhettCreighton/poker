# Contributing to Poker Platform

Thank you for your interest in contributing to the Poker Platform project!

## Code of Conduct

Please note that this project is released with a Contributor Code of Conduct. By participating in this project you agree to abide by its terms.

## How to Contribute

### Reporting Bugs

Before creating bug reports, please check existing issues to avoid duplicates. When creating a bug report, include:

- A clear and descriptive title
- Steps to reproduce the issue
- Expected behavior
- Actual behavior
- System information (OS, compiler version, etc.)
- Relevant logs or error messages

### Suggesting Enhancements

Enhancement suggestions are welcome! Please include:

- A clear and descriptive title
- Detailed description of the proposed feature
- Use cases and benefits
- Possible implementation approach

### Pull Requests

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Make your changes following our coding standards
4. Add or update tests as needed
5. Update documentation
6. Commit your changes (`git commit -m 'Add amazing feature'`)
7. Push to the branch (`git push origin feature/amazing-feature`)
8. Open a Pull Request

## Development Setup

```bash
# Clone the repository
git clone https://github.com/yourusername/poker.git
cd poker

# Build the project
./build.sh

# Run tests
cd build && ctest --output-on-failure
```

## Coding Standards

### C Code Style

- Use C99 standard
- 4 spaces for indentation (no tabs)
- Opening braces on same line for functions
- Maximum line length: 100 characters
- Use snake_case for functions and variables
- Use UPPER_CASE for constants and macros
- Add SPDX license headers to all new files:

```c
/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */
```

### Code Quality

- Write self-documenting code with clear variable names
- Add comments for complex algorithms
- Keep functions focused and small
- Check return values and handle errors appropriately
- Free all allocated memory
- No memory leaks or undefined behavior

### Testing

- Write tests for new functionality
- Ensure all tests pass before submitting PR
- Aim for high code coverage
- Test edge cases and error conditions

### Documentation

- Update relevant documentation for new features
- Add inline documentation for public APIs
- Include examples in documentation
- Keep README files current

## Project Structure

```
poker/
├── ai/              # AI player implementation
├── client/          # Client-side code
├── common/          # Shared libraries
├── demos/           # Demo applications
├── docs/            # Documentation
├── network/         # Network protocol
├── server/          # Server implementation
├── tests/           # Test suites
└── variants/        # Poker game variants
```

## Review Process

All submissions require review before merging:

1. Code must compile without warnings
2. All tests must pass
3. Code coverage should not decrease
4. Documentation must be updated
5. Code follows project style guidelines

## Questions?

Feel free to open an issue for any questions about contributing.