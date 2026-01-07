# GitHub Repository Setup Guide

## Repository Details

**Repository Name**: `vectis-db`  
**Visibility**: Private  
**Description**: High-performance page-oriented vector database engine in C++20

## Quick Setup

### 1. Create GitHub Repository

Go to: https://github.com/new

**Settings:**
- Repository name: `vectis-db`
- Description: "Production-grade page-oriented vector database engine for AI/ML applications"
- Visibility: **Private** ✅
- Initialize: **DO NOT** add README, .gitignore, or license (already exists locally)

### 2. Update Remote URL

```powershell
# Remove old remote
git remote remove origin

# Add new remote (replace YOUR-USERNAME)
git remote add origin https://github.com/YOUR-USERNAME/vectis-db.git

# Verify
git remote -v
```

### 3. Push All Branches

```powershell
# Push master branch
git push -u origin master

# Push develop branch
git checkout develop
git push -u origin develop

# Push feature branch
git checkout feature/buffer-pool-manager
git push -u origin feature/buffer-pool-manager

# Return to master
git checkout master
```

## Branch Structure

### `master` (Protected)
- **Purpose**: Stable releases only
- **Updates**: Quarterly milestones (Year 1 Q1, Q2, Q3, Q4)
- **Protection Rules**:
  - Require pull request reviews
  - Require status checks to pass (CI)
  - No direct pushes

### `develop` (Default Branch)
- **Purpose**: Integration branch for active development
- **Updates**: Feature merges, daily development
- **Merge Strategy**: Squash and merge from feature branches

### `feature/*`
- **Naming**: `feature/buffer-pool-manager`, `feature/lru-k-replacer`
- **Purpose**: Individual component development
- **Lifetime**: Created from `develop`, merged back when complete
- **Examples**:
  - `feature/buffer-pool-manager` (Year 1 Q2)
  - `feature/lru-k-replacer` (Year 1 Q3)
  - `feature/write-ahead-log` (Year 1 Q4)

### `hotfix/*` (When Needed)
- **Purpose**: Critical bug fixes for production
- **Branch From**: `master`
- **Merge To**: Both `master` and `develop`

## GitHub Repository Configuration

### Settings → General

**Default Branch**: `develop`

**Pull Requests**:
- ✅ Allow squash merging
- ✅ Automatically delete head branches
- ❌ Allow merge commits
- ❌ Allow rebase merging

### Settings → Branches

**Branch Protection Rules for `master`**:
1. ✅ Require pull request before merging
2. ✅ Require approvals: 0 (solo project, but keeps history clean)
3. ✅ Require status checks to pass (when CI is set up)
4. ✅ Require branches to be up to date
5. ✅ Do not allow bypassing the above settings

**Branch Protection Rules for `develop`**:
1. ✅ Require pull request before merging
2. ✅ Require status checks to pass

### Settings → Actions

**Actions Permissions**:
- ✅ Allow all actions and reusable workflows

(CI workflow already exists in `.github/workflows/ci.yml`)

## Labels

Create labels for issue tracking:

| Label | Color | Description |
|-------|-------|-------------|
| `year-1-q1` | `#0E8A16` | Year 1 Q1: Page & DiskManager |
| `year-1-q2` | `#0E8A16` | Year 1 Q2: BufferPoolManager |
| `year-1-q3` | `#0E8A16` | Year 1 Q3: LRU-K Replacer |
| `year-1-q4` | `#0E8A16` | Year 1 Q4: Write-Ahead Logging |
| `bug` | `#D73A4A` | Something isn't working |
| `enhancement` | `#A2EEEF` | New feature or request |
| `documentation` | `#0075CA` | Improvements to documentation |
| `performance` | `#FBCA04` | Performance optimization |
| `good-first-issue` | `#7057FF` | Good for newcomers |

## Milestone Setup

Create milestones for tracking quarterly progress:

### Year 1 Q1: Storage Foundation ✅
- **Due Date**: Completed January 2026
- **Description**: Page structure, DiskManager, O_DIRECT I/O
- **Issues**: Page design, CRC32 validation, aligned I/O

### Year 1 Q2: Buffer Pool (Current)
- **Due Date**: April 2026
- **Description**: BufferPoolManager, PageTable, pin/unpin semantics
- **Issues**: Frame allocation, dirty page tracking, thread safety

### Year 1 Q3: Eviction Policy
- **Due Date**: July 2026
- **Description**: LRU-K replacer, intelligent caching
- **Issues**: K-distance calculation, eviction algorithm

### Year 1 Q4: Durability
- **Due Date**: October 2026
- **Description**: Write-Ahead Logging, ARIES recovery
- **Issues**: Log format, group commit, checkpoint

## Project Board (Optional)

**Columns**:
1. 📋 Backlog
2. 🎯 Current Milestone
3. 🚧 In Progress
4. 👀 In Review
5. ✅ Done

## Wiki Pages

Create wiki pages for:
- Architecture Overview
- Year 1 Implementation Guide
- Performance Benchmarks
- API Examples

## After Push Verification

```powershell
# Verify all branches pushed
git branch -r

# Expected output:
# origin/master
# origin/develop
# origin/feature/buffer-pool-manager
```

## Next Steps

1. ✅ Create private repo on GitHub
2. ✅ Update remote URL
3. ✅ Push all branches
4. ⏳ Set default branch to `develop`
5. ⏳ Configure branch protection rules
6. ⏳ Create labels and milestones
7. ⏳ Set up project board (optional)
8. ⏳ Begin Year 1 Q2 development!

---

**Repository will be at**: `https://github.com/YOUR-USERNAME/vectis-db`
