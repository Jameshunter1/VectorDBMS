# demo_vectors_simple.ps1
# Simplified vector database population demo

$ErrorActionPreference = "Stop"
$baseUrl = "http://localhost:8080"

Write-Host "`n=== Vectis Vector Database Population Demo ===" -ForegroundColor Cyan

# Check if server is running
try {
    $null = Invoke-RestMethod -Uri "$baseUrl/api/stats" -Method Get -ErrorAction Stop
    Write-Host "✓ Server is running`n" -ForegroundColor Green
} catch {
    Write-Host "✗ Server not accessible at $baseUrl" -ForegroundColor Red
    exit 1
}

# Helper function to create a random vector
function New-RandomVector {
    param([int]$dim = 128)
    $vec = @()
    for ($i = 0; $i -lt $dim; $i++) {
        $vec += [math]::Round((Get-Random -Minimum -1.0 -Maximum 1.0), 4)
    }
    return $vec
}

# Helper function to PUT a vector
function Add{
    param(
        [string]$key,
        [float[]]$vector
    )
    $vectorStr = $vector -join ", "
    $uri = "$baseUrl/api/vector/put?key=$key&vector=$vectorStr"
    try {
        Invoke-RestMethod -Uri $uri -Method Post -ContentType "application/json" | Out-Null
        return $true
    } catch {
        Write-Host "Error inserting $key : $_" -ForegroundColor Red
        return $false
    }
}

Write-Host "=== Step 1: Populate User Profile Embeddings ===" -ForegroundColor Yellow
Write-Host "Inserting 5 user vectors - 128 dimensions..."
Write-Host ""

$users = @("user:alice", "user:bob", "user:charlie", "user:diana", "user:eve")
$userCount = 0
foreach ($user in $users) {
    $vec = New-RandomVector -dim 128
    $result = Add-Vector -key $user -vector $vec
    if ($result -eq $true) {
        $userCount++
        Write-Host "  ✓ Inserted $user" -ForegroundColor Green
    }
}
Write-Host "Inserted $userCount user profiles"
Write-Host ""

Write-Host "=== Step 2: Populate Document Embeddings ===" -ForegroundColor Yellow
Write-Host "Inserting 8 document vectors - 128 dimensions..."
Write-Host ""

$docs = @(
    "doc:ai_intro", "doc:ml_basics", "doc:deep_learning",
    "doc:sports_news", "doc:football", "doc:basketball",
    "doc:cooking_recipes", "doc:italian_food"
)
$docCount = 0
foreach ($doc in $docs) {
    $vec = New-RandomVector -dim 128
    $result = Add-Vector -key $doc -vector $vec
    if ($result -eq $true) {
        $docCount++
        Write-Host "  ✓ Inserted $doc" -ForegroundColor Green
    }
}
Write-Host "Inserted $docCount documents"
Write-Host ""

Write-Host "=== Step 3: Populate Image Embeddings ===" -ForegroundColor Yellow
Write-Host "Inserting 7 image vectors - 128 dimensions..."
Write-Host ""

$images = @(
    "img:cat_001", "img:cat_002", "img:dog_001", "img:dog_002",
    "img:sunset_001", "img:beach_001", "img:mountain_001"
)
$imgCount = 0
foreach ($img in $images) {
    $vec = New-RandomVector -dim 128
    $result = Add-Vector -key $img -vector $vec
    if ($result -eq $true) {
        $imgCount++
        Write-Host "  ✓ Inserted $img" -ForegroundColor Green
    }
}
Write-Host "Inserted $imgCount images"
Write-Host ""

Write-Host "=== Step 4: Vector Database Statistics ===" -ForegroundColor Yellow
try {
    $stats = Invoke-RestMethod -Uri "$baseUrl/api/vector/stats" -Method Get
    Write-Host "Vector Index Status:" -ForegroundColor Cyan
    Write-Host "  Enabled: $($stats.index_enabled)"
    Write-Host "  Total Vectors: $($stats.num_vectors)"
    Write-Host "  Dimension: $($stats.dimension)"
    Write-Host "  Distance Metric: $($stats.metric)"
    Write-Host "  HNSW Layers: $($stats.num_layers)"
    Write-Host "  Avg Connections: $([math]::Round($stats.avg_connections, 2))`n"
} catch {
    Write-Host "Could not retrieve vector statistics`n" -ForegroundColor Yellow
}

Write-Host "=== Step 5: Test Vector Search ===" -ForegroundColor Yellow
Write-Host "`nSearching for similar vectors to first user..."

# Generate a random query
$queryVec = New-RandomVector -dim 128
$queryStr = $queryVec -join ","
try {
    $results = Invoke-RestMethod -Uri "$baseUrl/api/vector/search?vector=$queryStr&k=5" -Method Get
    Write-Host "`nTop 5 similar vectors:" -ForegroundColor Cyan
    foreach ($result in $results.results) {
        $distStr = [math]::Round($result.distance, 4)
        Write-Host "  $($result.key) - distance: $distStr" -ForegroundColor White
    }
} catch {
    Write-Host "Search failed: $_" -ForegroundColor Red
}

Write-Host "`n=== Summary ===" -ForegroundColor Green
$total = $userCount + $docCount + $imgCount
Write-Host "Successfully populated database with:"
Write-Host "  • $userCount user profile embeddings"
Write-Host "  • $docCount document embeddings"
Write-Host "  • $imgCount image embeddings"
Write-Host "  • Total: $total vectors (128-dimensional)"
Write-Host "`nVector similarity search is ready!"
Write-Host "Access the web UI at: $baseUrl`n" -ForegroundColor Cyan
