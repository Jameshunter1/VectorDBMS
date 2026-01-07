#!/usr/bin/env pwsh
# Quick setup script for creating GitHub repository

Write-Host "`n╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  VECTIS DATABASE - GITHUB REPOSITORY SETUP SCRIPT            ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

# Step 1: Verify current status
Write-Host "📊 Current Repository Status:" -ForegroundColor Yellow
git log --oneline -3
Write-Host ""

# Step 2: Instructions
Write-Host "🚀 MANUAL STEPS REQUIRED:" -ForegroundColor Yellow
Write-Host ""
Write-Host "1️⃣  CREATE GITHUB REPOSITORY" -ForegroundColor Cyan
Write-Host "   Go to: https://github.com/new" -ForegroundColor White
Write-Host "   - Name: vectis-db" -ForegroundColor Gray
Write-Host "   - Visibility: Private ✅" -ForegroundColor Gray
Write-Host "   - Description: Production-grade page-oriented vector database engine" -ForegroundColor Gray
Write-Host "   - DO NOT initialize with README/license (already exists)" -ForegroundColor Gray
Write-Host ""

# Step 3: Get username
Write-Host "2️⃣  ENTER YOUR GITHUB USERNAME" -ForegroundColor Cyan
$username = Read-Host "   GitHub username"

if ($username) {
    $repoUrl = "https://github.com/$username/vectis-db.git"
    
    Write-Host "`n3️⃣  CONFIGURE REMOTE" -ForegroundColor Cyan
    Write-Host "   Setting remote URL to: $repoUrl" -ForegroundColor Gray
    git remote set-url origin $repoUrl
    
    Write-Host "`n4️⃣  VERIFY REMOTE" -ForegroundColor Cyan
    git remote -v
    
    Write-Host "`n5️⃣  PUSH ALL BRANCHES" -ForegroundColor Cyan
    $confirm = Read-Host "   Ready to push? (yes/no)"
    
    if ($confirm -eq "yes") {
        Write-Host "   Pushing master..." -ForegroundColor Gray
        git push -u origin master
        
        Write-Host "   Pushing develop..." -ForegroundColor Gray
        git push -u origin develop
        
        Write-Host "   Pushing feature/buffer-pool-manager..." -ForegroundColor Gray
        git push -u origin feature/buffer-pool-manager
        
        Write-Host "`n✅ SUCCESS! Repository pushed to GitHub" -ForegroundColor Green
        Write-Host ""
        Write-Host "🔗 Repository URL: https://github.com/$username/vectis-db" -ForegroundColor Cyan
        Write-Host ""
        Write-Host "📝 NEXT STEPS:" -ForegroundColor Yellow
        Write-Host "   1. Go to repository Settings → Branches" -ForegroundColor White
        Write-Host "   2. Change default branch to 'develop'" -ForegroundColor White
        Write-Host "   3. Set up branch protection rules (see GITHUB_SETUP.md)" -ForegroundColor White
        Write-Host "   4. Create labels and milestones (see GITHUB_SETUP.md)" -ForegroundColor White
        Write-Host ""
        Write-Host "🎯 Ready to start Year 1 Q2: BufferPoolManager!" -ForegroundColor Green
    } else {
        Write-Host "`n⏸️  Push cancelled. Run again when ready." -ForegroundColor Yellow
    }
} else {
    Write-Host "`n❌ No username provided. Exiting." -ForegroundColor Red
}

Write-Host ""
