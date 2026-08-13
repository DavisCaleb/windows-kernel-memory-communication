# Security Policy

## Important Notice

This repository contains Windows kernel-mode code and is intended for **educational purposes only**. Kernel-mode software carries significant risks including system instability, security vulnerabilities, and potential data loss.

## Supported Versions

This is an archival prototype. No official security updates or patches are provided.

| Version | Status | Notes |
|---------|--------|-------|
| Prototype | Archival | No security support; educational reference only |

## Security Considerations

### Known Risks

1. **Kernel-Mode Execution**: Code runs with highest system privileges; bugs can cause system crashes (BSOD)
2. **Undocumented APIs**: Relies on undocumented Windows structures that may change between OS versions
3. **Memory Operations**: Cross-process memory access can be exploited if misused
4. **Incomplete Validation**: Basic checks exist, but the prototype lacks a production-grade capture, authorization, and synchronization model

### Responsible Disclosure

If you discover security vulnerabilities in this code for educational analysis:

1. **DO NOT** exploit vulnerabilities in production systems
2. **DO NOT** use this code for unauthorized access to systems
3. **DO** report findings to the maintainers for documentation purposes
4. **DO** include proof-of-concept analysis in controlled environments

## Safe Usage Guidelines

1. **Prefer Review Over Execution**: Treat the repository as source for code review and architecture study
2. **Controlled Environment Only**: If authorized research is necessary, use isolated disposable systems with appropriate debugging and recovery plans
3. **No Production Deployment**: Do not load this prototype on production or daily-use systems
4. **No Published Binaries**: Do not redistribute compiled driver artifacts from this archive

## Legal Notice

This software is provided "as is" for educational purposes. Users are responsible for ensuring compliance with all applicable laws and regulations. Unauthorized use of kernel-mode code to bypass security controls or gain unauthorized access is illegal.

## Contact

For security-related questions regarding this educational prototype, please open an issue on the repository.
