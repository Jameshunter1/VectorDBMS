# Security Policy

## Supported Versions

The following versions of VectorDBMS are currently being supported with security updates:

| Version | Supported          |
| ------- | ------------------ |
| 1.x.x   | :white_check_mark: |
| < 1.0   | :x:                |

## Reporting a Vulnerability

We take the security of VectorDBMS seriously. If you discover a security vulnerability, please follow these steps:

### 1. **Do Not** Open a Public Issue

Security vulnerabilities should not be reported through public GitHub issues. This helps protect users who may be running vulnerable versions.

### 2. Report Privately

Please report security vulnerabilities by emailing:

**📧 security@vectordbms.dev** (or james.hunter@vectordbms.dev)

Include the following information:

- **Description**: Clear description of the vulnerability
- **Impact**: What can an attacker do with this vulnerability?
- **Steps to Reproduce**: Detailed steps to reproduce the issue
- **Affected Versions**: Which versions are affected?
- **Proof of Concept**: Code or commands demonstrating the vulnerability (if applicable)
- **Suggested Fix**: If you have ideas on how to fix it (optional)

### 3. Response Timeline

- **Initial Response**: Within 48 hours
- **Confirmation**: Within 7 days
- **Fix Timeline**: Depending on severity
  - Critical: 7-14 days
  - High: 14-30 days
  - Medium: 30-60 days
  - Low: 60-90 days

### 4. Disclosure Process

1. We'll acknowledge receipt of your report
2. We'll investigate and confirm the vulnerability
3. We'll develop a fix and test it thoroughly
4. We'll release a security patch
5. We'll credit you in the security advisory (unless you prefer to remain anonymous)
6. After the patch is released, we'll publish a security advisory

## Security Best Practices

When using VectorDBMS:

### Authentication & Authorization

- Always use strong authentication mechanisms
- Implement proper access controls
- Regularly rotate credentials
- Use HTTPS/TLS for network communication

### Data Protection

- Encrypt sensitive data at rest
- Use secure backup procedures
- Implement proper key management
- Regularly audit access logs

### Configuration

- Follow the security hardening guide (coming soon)
- Keep software up to date
- Disable unnecessary features
- Use least privilege principles

### Network Security

- Use firewalls to restrict access
- Implement rate limiting
- Monitor for suspicious activity
- Use secure protocols (TLS 1.3+)

## Known Security Considerations

### Storage Security

- Database files are stored unencrypted by default
- File system permissions should be properly configured
- Consider using full-disk encryption for sensitive data

### Network Communication

- The web interface (dbweb) runs on HTTP by default
- For production, always use a reverse proxy with HTTPS
- Implement proper authentication before exposing to networks

### Input Validation

- All user inputs are validated
- SQL-injection-like attacks are not applicable (key-value store)
- Size limits are enforced to prevent DoS

## Security Audit History

- **2026-01**: Initial security review conducted
- More audits to be scheduled

## Third-Party Dependencies

We regularly monitor and update third-party dependencies for security vulnerabilities. See our [dependency scanning workflow](.github/workflows/dependency-review.yml).

## Bug Bounty Program

We currently do not have a bug bounty program, but we greatly appreciate responsible disclosure and will credit researchers in our security advisories.

## Contact

For security-related questions or concerns:

- **Email**: security@vectordbms.dev
- **PGP Key**: Coming soon
- **Security Advisories**: https://github.com/Jameshunter1/VectorDBMS/security/advisories

---

Thank you for helping keep VectorDBMS and its users safe! 🛡️
