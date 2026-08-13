# Contributing to MemoryDriverAndComm

Thank you for your interest in contributing to this educational Windows driver prototype. This document outlines guidelines for contributions.

## Project Nature

This is an **archival educational prototype** demonstrating Windows kernel/user-mode communication patterns. It is not production software and should not be deployed in live environments.

## Areas of Contribution

### Welcome Contributions

1. **Documentation Improvements**: Clarify explanations, add examples, improve code comments
2. **Code Quality**: Fix typos, improve naming consistency, enhance comments
3. **Educational Content**: Add explanatory notes about Windows internals
4. **Modernization**: Suggest improvements toward supported patterns (IOCTL-based design)
5. **Bug Fixes**: Issues related to compilation, documentation errors, or clarity

### Not Accepted

1. **Offensive Capabilities**: No additions that strengthen stealth, evasion, or security-control bypass
2. **Production Deployability**: This is archival code; not seeking production readiness
3. **New Transport Mechanisms**: Protocol changes should be discussed first
4. **Hooking Expansion**: No additional hooking or manual mapping techniques

## Code Style Guidelines

### Naming Conventions

- Use neutral, descriptive terminology such as "target" and "client"
- Avoid profanity or informal language in comments and identifiers
- Use descriptive names for variables and functions

### Code Quality

- Include null/argument checks before dereferencing pointers
- Handle NTSTATUS return values appropriately
- Clean up resources (handles, MDLs, threads) properly
- Add bounds checking for arrays and buffers
- Use consistent formatting (4-space indentation)

### Comments

- Explain Windows-specific concepts for educational value
- Document NTSTATUS codes and their meanings
- Note any reliance on undocumented internals
- Flag code that may break on future Windows versions

## Pull Request Process

1. **Fork and Branch**: Create a feature branch from `main`
2. **Descriptive Title**: Use clear, concise PR titles
3. **Description**: Explain the change and its educational value
4. **No Breaking Changes**: Maintain compatibility with existing protocol
5. **Review**: Address feedback and update documentation as needed

## Testing Disclaimer

Windows compilation requires Visual Studio 2022 and the WDK; runtime research additionally requires an isolated, explicitly authorized lab. Contributors should report the exact Windows toolchain used and must not submit compiled driver artifacts.

## License

By contributing, you agree that your contributions will be licensed under the MIT License.

## Questions?

Open an issue for questions or start a discussion for larger changes.
