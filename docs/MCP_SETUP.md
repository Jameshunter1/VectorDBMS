# GitHub MCP Server Setup Guide

This guide explains how to set up and use the GitHub Model Context Protocol (MCP) server with the VectorDBMS project.

## What is the GitHub MCP Server?

The GitHub MCP server enables AI assistants and development tools to interact with GitHub repositories through a standardized protocol. It provides capabilities for:

- Repository management and navigation
- Code search and analysis
- Issue and pull request operations
- Workflow automation
- Repository insights and statistics

## Prerequisites

Before setting up the GitHub MCP server, ensure you have:

1. **Node.js** (v16 or higher) and **npm** installed
2. **GitHub Personal Access Token (PAT)** with appropriate permissions

## Installation

### 1. Install the GitHub MCP Server

The server has already been installed globally:

```bash
npm install -g @modelcontextprotocol/server-github
```

### 2. Create a GitHub Personal Access Token

1. Go to [GitHub Settings → Developer Settings → Personal Access Tokens](https://github.com/settings/tokens)
2. Click **"Generate new token (classic)"**
3. Give your token a descriptive name (e.g., "MCP Server Access")
4. Select the following scopes based on your needs:
   - `repo` - Full control of private repositories
   - `read:org` - Read org and team membership
   - `read:user` - Read user profile data
   - `workflow` - Update GitHub Action workflows (optional)
5. Click **"Generate token"**
6. **Copy the token immediately** - you won't be able to see it again!

### 3. Configure the MCP Server

The project includes a `.mcp-config.json` file in the root directory. Update it with your GitHub PAT:

```json
{
  "mcpServers": {
    "github": {
      "command": "npx",
      "args": [
        "-y",
        "@modelcontextprotocol/server-github"
      ],
      "env": {
        "GITHUB_PERSONAL_ACCESS_TOKEN": "your-actual-token-here"
      }
    }
  }
}
```

> [!IMPORTANT]
> **Security**: Never commit your actual GitHub PAT to version control. The `.mcp-config.json` file should be added to `.gitignore` to prevent accidental exposure.

### 4. Using the MCP Server

The GitHub MCP server can be used with various MCP-compatible clients:

#### VS Code with Copilot
1. Install the GitHub Copilot extension
2. Enable "Agent mode" in Copilot Chat
3. The MCP server will automatically connect using your configuration

#### Claude Desktop or Other MCP Clients
1. Configure the client to use the MCP server
2. Point it to the `.mcp-config.json` file or configure manually
3. The server will provide GitHub integration capabilities

## Available Capabilities

Once configured, the GitHub MCP server provides:

- **Repository Operations**: Clone, create, list repositories
- **File Operations**: Read, search, and navigate code
- **Issue Management**: Create, update, search issues
- **Pull Requests**: Create, review, merge PRs
- **Branch Operations**: Create, list, switch branches
- **Search**: Code search across repositories
- **Insights**: Repository statistics and analytics

## Troubleshooting

### Token Permission Issues
If you encounter permission errors:
- Verify your PAT has the required scopes
- Regenerate the token with additional permissions if needed
- Ensure the token hasn't expired

### Connection Issues
If the server fails to connect:
- Verify Node.js and npm are properly installed
- Check that the MCP server package is installed globally
- Ensure your network allows connections to GitHub's API

### Configuration Issues
If the configuration isn't recognized:
- Verify the JSON syntax in `.mcp-config.json`
- Ensure the file path is correct
- Check that environment variables are properly set

## Security Best Practices

1. **Never commit tokens**: Always use environment variables or secure configuration files
2. **Use minimal permissions**: Only grant the PAT scopes you actually need
3. **Rotate tokens regularly**: Regenerate your PAT periodically
4. **Revoke unused tokens**: Delete old tokens from GitHub settings
5. **Use separate tokens**: Create different tokens for different purposes

## Additional Resources

- [GitHub MCP Server Documentation](https://github.com/modelcontextprotocol/servers/tree/main/src/github)
- [Model Context Protocol Specification](https://modelcontextprotocol.io/)
- [GitHub Personal Access Token Guide](https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/creating-a-personal-access-token)

## Support

For issues specific to the VectorDBMS project, please [open an issue](https://github.com/Jameshunter1/VectorDBMS/issues) on GitHub.

For MCP server issues, refer to the [official MCP repository](https://github.com/modelcontextprotocol/servers).
