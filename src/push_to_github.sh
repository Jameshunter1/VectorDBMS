#!/bin/bash
# Quick Push Script for VectorDBMS Repository (Linux/macOS)

# Colors
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
WHITE='\033[1;37m'
NC='\033[0m' # No Color

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}VectorDBMS Repository Setup${NC}"
echo -e "${GREEN}========================================${NC}"

# Step 1: Navigate to project directory (if needed)
# cd /path/to/VectorDBMS/src

# Step 2: Check if git is initialized
if [ ! -d .git ]; then
    echo -e "${GREEN}Initializing Git repository...${NC}"
    git init
fi

# Step 3: Add remote
if ! git remote | grep -q "origin"; then
    echo -e "${GREEN}Adding remote repository...${NC}"
    git remote add origin https://github.com/Jameshunter1/VectorDBMS.git
fi

# Step 4: Check current status
echo -e "${CYAN}Current Git Status:${NC}"
git status

# Step 5: Add all files
echo -e "${GREEN}Staging all files...${NC}"
git add .

# Step 6: Create initial commit
echo -e "${GREEN}Creating initial commit...${NC}"
git commit -m "feat: Professional repository setup with comprehensive CI/CD

## Documentation
- Add LICENSE (MIT)
- Add CONTRIBUTING.md with detailed guidelines
- Add CODE_OF_CONDUCT.md
- Add SECURITY.md with vulnerability reporting
- Add CHANGELOG.md for version tracking
- Add BUILDING.md with multi-platform instructions
- Add comprehensive README enhancements

## GitHub Configuration
- Add issue templates (bug, feature, performance)
- Add pull request template
- Add funding configuration
- Enhance .gitignore for C++ projects

## CI/CD Workflows
- Add main CI/CD pipeline (Windows/Linux/macOS)
- Add code coverage workflow
- Add sanitizer workflows (ASan, UBSan, TSan, MSan)
- Add dependency review workflow
- Add security scanning (CodeQL)

## Features
- Multi-platform build support (MSVC, GCC, Clang)
- Automated testing on all platforms
- Code coverage tracking
- Memory safety checks
- Performance benchmarking
- Automated releases

BREAKING CHANGE: Initial professional repository structure"

# Step 7: Set main branch
echo -e "${GREEN}Setting main branch...${NC}"
git branch -M main

# Step 8: Push to GitHub
echo -e "${GREEN}Pushing to GitHub...${NC}"
echo -e "${YELLOW}This may take a moment...${NC}"
git push -u origin main

# Step 9: Success message
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}✅ SUCCESS! Repository pushed to GitHub${NC}"
echo -e "${GREEN}========================================${NC}"
echo -e "${CYAN}Repository URL: https://github.com/Jameshunter1/VectorDBMS${NC}"
echo -e "${YELLOW}Next Steps:${NC}"
echo -e "${WHITE}1. Visit your repository on GitHub${NC}"
echo -e "${WHITE}2. Configure repository settings (see SETUP_INSTRUCTIONS.md)${NC}"
echo -e "${WHITE}3. Enable branch protection rules${NC}"
echo -e "${WHITE}4. Enable security features${NC}"
echo -e "${WHITE}5. Add repository topics${NC}"
echo -e "${CYAN}For detailed instructions, see REPOSITORY_SUMMARY.md${NC}"
