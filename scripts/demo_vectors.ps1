# demo_vectors.ps1
# Populates the Vectis database with sample vector embeddings
# Demonstrates vector similarity search with realistic use cases

$ErrorActionPreference = "Stop"
$baseUrl = "http://localhost:8080"

Write-Host "`n=== Vectis Vector Database Population Demo ===" -ForegroundColor Cyan
Write-Host "This script populates the database with sample vector embeddings"
Write-Host "representing different domains: users, documents, and images`n"

# Helper function to PUT a vector
function Add-Vector {
    param(
        [string]$key,
        [float[]]$vector
    )
    $vectorStr = $vector -join ","
    $uri = "$baseUrl/api/vector/put?key=$key&vector=$vectorStr"
    try {
        Invoke-RestMethod -Uri $uri -Method Post -ContentType "application/json" | Out-Null
        return $true
    } catch {
        Write-Host "Error inserting $key : $_" -ForegroundColor Red
        return $false
    }
}

# Helper function to search similar vectors
function Search-Similar {
    param(
        [float[]]$query,
        [int]$k = 5
    )
    $vectorStr = $query -join ","
    $uri = "$baseUrl/api/vector/search?vector=$vectorStr&k=$k"
    try {
        $response = Invoke-RestMethod -Uri $uri -Method Get
        return $response.results
    } catch {
        Write-Host "Error searching: $_" -ForegroundColor Red
        return $null
    }
}

# Check if server is running
try {
    Invoke-RestMethod -Uri "$baseUrl/api/health" -Method Get | Out-Null
    Write-Host "✓ Server is running" -ForegroundColor Green
} catch {
    # If health check fails, try stats endpoint which we know exists
    try {
        Invoke-RestMethod -Uri "$baseUrl/api/vector/stats" -Method Get | Out-Null
        Write-Host "✓ Server is running" -ForegroundColor Green
    } catch {
        Write-Host "✗ Server not accessible at $baseUrl" -ForegroundColor Red
        Write-Host "  Please start the server with: .\build\windows-vs2022-x64-debug\Debug\dbweb.exe" -ForegroundColor Yellow
        exit 1
    }
}

Write-Host "`n=== Step 1: Populate User Profile Embeddings ===" -ForegroundColor Yellow
Write-Host "Simulating user embeddings from a recommendation system - 128 dimensions"

# Generate realistic user embeddings (128 dimensions)
$users = @{
    "user:alice" = @(0.8, 0.6, 0.3, 0.9, 0.2, 0.7, 0.5, 0.4) + @(0.1) * 120
    "user:bob" = @(0.7, 0.7, 0.4, 0.8, 0.3, 0.6, 0.6, 0.5) + @(0.15) * 120
    "user:charlie" = @(0.2, 0.3, 0.8, 0.1, 0.9, 0.4, 0.3, 0.7) + @(0.2) * 120
    "user:diana" = @(0.3, 0.2, 0.9, 0.2, 0.8, 0.3, 0.4, 0.6) + @(0.18) * 120
    "user:eve" = @(0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5) + @(0.12) * 120
}

$userCount = 0
foreach ($userKey in $users.Keys) {
    if (Add-Vector -key $userKey -vector $users[$userKey]) {
        $userCount++
        Write-Host "  ✓ Inserted $userKey" -ForegroundColor Green
    }
}
Write-Host "Inserted $userCount user profiles`n" -ForegroundColor Cyan

Write-Host "`n=== Step 2: Populate Document Embeddings ===" -ForegroundColor Yellow
Write-Host "Simulating document embeddings from a text search system - 128 dimensions"

$documents = @{
    "doc:ai_intro" = @(0.9, 0.8, 0.7, 0.6, 0.2, 0.3, 0.4, 0.5) + @(0.25) * 120
    "doc:ml_basics" = @(0.85, 0.75, 0.65, 0.55, 0.25, 0.35, 0.45, 0.55) + @(0.24) * 120
    "doc:deep_learning" = @(0.87, 0.77, 0.67, 0.57, 0.23, 0.33, 0.43, 0.53) + @(0.26) * 120
    "doc:sports_news" = @(0.1, 0.2, 0.9, 0.8, 0.7, 0.3, 0.2, 0.4) + @(0.3) * 120
    "doc:football" = @(0.15, 0.25, 0.85, 0.75, 0.65, 0.35, 0.25, 0.45) + @(0.31) * 120
    "doc:basketball" = @(0.12, 0.22, 0.88, 0.78, 0.68, 0.32, 0.22, 0.42) + @(0.29) * 120
    "doc:cooking_recipes" = @(0.4, 0.5, 0.3, 0.2, 0.6, 0.7, 0.8, 0.9) + @(0.35) * 120
    "doc:italian_food" = @(0.38, 0.48, 0.32, 0.22, 0.58, 0.68, 0.78, 0.88) + @(0.34) * 120
}

$docCount = 0
foreach ($docKey in $documents.Keys) {
    if (Add-Vector -key $docKey -vector $documents[$docKey]) {
        $docCount++
        Write-Host "  ✓ Inserted $docKey" -ForegroundColor Green
    }
}
Write-Host "Inserted $docCount documents`n" -ForegroundColor Cyan

Write-Host "`n=== Step 3: Populate Image Embeddings ===" -ForegroundColor Yellow
Write-Host "Simulating image embeddings from a computer vision model - 128 dimensions"

$images = @{
    "img:cat_001" = @(0.7, 0.3, 0.2, 0.8, 0.9, 0.1, 0.4, 0.6) + @(0.4) * 120
    "img:cat_002" = @(0.68, 0.32, 0.22, 0.78, 0.88, 0.12, 0.42, 0.58) + @(0.39) * 120
    "img:dog_001" = @(0.6, 0.4, 0.3, 0.7, 0.8, 0.2, 0.5, 0.5) + @(0.38) * 120
    "img:dog_002" = @(0.62, 0.38, 0.28, 0.72, 0.82, 0.18, 0.48, 0.52) + @(0.37) * 120
    "img:sunset_001" = @(0.9, 0.8, 0.1, 0.2, 0.3, 0.7, 0.6, 0.5) + @(0.45) * 120
    "img:beach_001" = @(0.85, 0.75, 0.15, 0.25, 0.35, 0.65, 0.55, 0.45) + @(0.43) * 120
    "img:mountain_001" = @(0.3, 0.4, 0.8, 0.7, 0.2, 0.5, 0.6, 0.9) + @(0.48) * 120
}

$imgCount = 0
foreach ($imgKey in $images.Keys) {
    if (Add-Vector -key $imgKey -vector $images[$imgKey]) {
        $imgCount++
        Write-Host "  ✓ Inserted $imgKey" -ForegroundColor Green
    }
}
Write-Host "Inserted $imgCount images`n" -ForegroundColor Cyan

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

Write-Host "=== Step 5: Similarity Search Demonstrations ===" -ForegroundColor Yellow

# Demo 1: Find similar users to Alice
Write-Host "`n1. Users similar to Alice (tech enthusiast):" -ForegroundColor Cyan
$aliceQuery = $users["user:alice"]
$similarUsers = Search-Similar -query $aliceQuery -k 3
if ($similarUsers) {
    foreach ($result in $similarUsers) {
        Write-Host "  $($result.key) - distance: $([math]::Round($result.distance, 4))" -ForegroundColor White
    }
}

# Demo 2: Find documents related to AI/ML
Write-Host "`n2. Documents similar to 'AI Introduction':" -ForegroundColor Cyan
$aiQuery = $documents["doc:ai_intro"]
$similarDocs = Search-Similar -query $aiQuery -k 3
if ($similarDocs) {
    foreach ($result in $similarDocs) {
        Write-Host "  $($result.key) - distance: $([math]::Round($result.distance, 4))" -ForegroundColor White
    }
}

# Demo 3: Find similar images to cat
Write-Host "`n3. Images similar to 'cat_001':" -ForegroundColor Cyan
$catQuery = $images["img:cat_001"]
$similarImages = Search-Similar -query $catQuery -k 4
if ($similarImages) {
    foreach ($result in $similarImages) {
        Write-Host "  $($result.key) - distance: $([math]::Round($result.distance, 4))" -ForegroundColor White
    }
}

# Demo 4: Cross-domain search
Write-Host "`n4. Documents matching Charlie's interests (sports fan):" -ForegroundColor Cyan
$charlieQuery = $users["user:charlie"]
$matchingDocs = Search-Similar -query $charlieQuery -k 5
if ($matchingDocs) {
    $docsOnly = $matchingDocs | Where-Object { $_.key -like "doc:*" }
    foreach ($result in $docsOnly) {
        Write-Host "  $($result.key) - distance: $([math]::Round($result.distance, 4))" -ForegroundColor White
    }
}

Write-Host "`n=== Summary ===" -ForegroundColor Green
Write-Host "Successfully populated database with:"
Write-Host "  • $userCount user profile embeddings"
Write-Host "  • $docCount document embeddings"
Write-Host "  • $imgCount image embeddings"
Write-Host "  • Total: $($userCount + $docCount + $imgCount) vectors (128-dimensional)"
Write-Host "`nVector similarity search is now ready to use!"
Write-Host "Access the web UI at: $baseUrl`n" -ForegroundColor Cyan
