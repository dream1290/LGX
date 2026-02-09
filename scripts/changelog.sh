#!/bin/bash
# Changelog generation script for LGX Runtime Core
# Generates changelog from git commits following Conventional Commits

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CHANGELOG_FILE="$PROJECT_ROOT/CHANGELOG.md"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Get version
get_version() {
    "$SCRIPT_DIR/version.sh" show
}

# Get previous tag
get_previous_tag() {
    git describe --tags --abbrev=0 HEAD^ 2>/dev/null || echo ""
}

# Parse commit message
parse_commit() {
    local commit=$1
    local message=$(git log -1 --pretty=%s "$commit")
    local body=$(git log -1 --pretty=%b "$commit")
    local author=$(git log -1 --pretty="%an" "$commit")
    local date=$(git log -1 --pretty=%cd --date=short "$commit")
    local hash=$(git log -1 --pretty=%h "$commit")
    
    # Parse conventional commit format: type(scope): description
    local type=""
    local scope=""
    local description=""
    local breaking=false
    
    if [[ $message =~ ^([a-z]+)(\(([^)]+)\))?:\ (.+)$ ]]; then
        type="${BASH_REMATCH[1]}"
        scope="${BASH_REMATCH[3]}"
        description="${BASH_REMATCH[4]}"
    else
        type="other"
        description="$message"
    fi
    
    # Check for breaking changes
    if [[ $message =~ BREAKING\ CHANGE ]] || [[ $body =~ BREAKING\ CHANGE ]]; then
        breaking=true
    fi
    
    echo "$type|$scope|$description|$breaking|$hash|$author|$date"
}

# Categorize commits
categorize_commits() {
    local from_ref=$1
    local to_ref=${2:-HEAD}
    
    declare -A categories
    categories[feat]="### ✨ Features"
    categories[fix]="### 🐛 Bug Fixes"
    categories[perf]="### ⚡ Performance"
    categories[docs]="### 📚 Documentation"
    categories[style]="### 💄 Style"
    categories[refactor]="### ♻️ Refactoring"
    categories[test]="### ✅ Tests"
    categories[build]="### 🔧 Build System"
    categories[ci]="### 👷 CI/CD"
    categories[chore]="### 🔨 Chores"
    categories[revert]="### ⏪ Reverts"
    categories[breaking]="### ⚠️ BREAKING CHANGES"
    categories[other]="### 📝 Other Changes"
    
    # Get commits
    local range=""
    if [ -n "$from_ref" ]; then
        range="${from_ref}..${to_ref}"
    else
        range="$to_ref"
    fi
    
    local commits=$(git log --pretty=%H "$range" --reverse)
    
    # Parse and categorize
    declare -A commit_lists
    
    for commit in $commits; do
        local parsed=$(parse_commit "$commit")
        IFS='|' read -r type scope description breaking hash author date <<< "$parsed"
        
        local entry="- $description ([${hash}](https://github.com/lgx-platform/LGX/commit/${hash}))"
        if [ -n "$scope" ]; then
            entry="- **${scope}**: $description ([${hash}](https://github.com/lgx-platform/LGX/commit/${hash}))"
        fi
        
        if [ "$breaking" = "true" ]; then
            commit_lists[breaking]+="$entry"$'\n'
        fi
        
        commit_lists[$type]+="$entry"$'\n'
    done
    
    # Generate changelog section
    local output=""
    
    # Breaking changes first
    if [ -n "${commit_lists[breaking]}" ]; then
        output+="${categories[breaking]}"$'\n\n'
        output+="${commit_lists[breaking]}"$'\n'
    fi
    
    # Then other categories in order
    for type in feat fix perf docs style refactor test build ci chore revert other; do
        if [ -n "${commit_lists[$type]}" ]; then
            output+="${categories[$type]}"$'\n\n'
            output+="${commit_lists[$type]}"$'\n'
        fi
    done
    
    echo "$output"
}

# Generate changelog for version
generate_version_changelog() {
    local version=$1
    local previous_tag=$(get_previous_tag)
    
    echo -e "${YELLOW}Generating changelog for version $version...${NC}"
    
    if [ -z "$previous_tag" ]; then
        echo -e "${YELLOW}No previous tag found, generating full changelog${NC}"
    else
        echo -e "${YELLOW}Changes since $previous_tag${NC}"
    fi
    
    local changes=$(categorize_commits "$previous_tag" "HEAD")
    
    # Create changelog entry
    local date=$(date +%Y-%m-%d)
    local entry="## [${version}] - ${date}"$'\n\n'
    
    if [ -n "$changes" ]; then
        entry+="$changes"
    else
        entry+="No changes recorded."$'\n'
    fi
    
    echo "$entry"
}

# Update CHANGELOG.md
update_changelog() {
    local version=$1
    local new_entry=$2
    
    echo -e "${YELLOW}Updating CHANGELOG.md...${NC}"
    
    local temp_file=$(mktemp)
    
    # Create header if file doesn't exist
    if [ ! -f "$CHANGELOG_FILE" ]; then
        cat > "$temp_file" << 'EOF'
# Changelog

All notable changes to LGX Runtime Core will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

EOF
    else
        # Copy existing header (everything before first version)
        sed -n '1,/^## \[/p' "$CHANGELOG_FILE" | sed '$d' > "$temp_file"
    fi
    
    # Add new entry
    echo "$new_entry" >> "$temp_file"
    echo "" >> "$temp_file"
    
    # Add existing entries
    if [ -f "$CHANGELOG_FILE" ]; then
        sed -n '/^## \[/,$p' "$CHANGELOG_FILE" >> "$temp_file"
    fi
    
    mv "$temp_file" "$CHANGELOG_FILE"
    
    echo -e "${GREEN}✓ CHANGELOG.md updated${NC}"
}

# Generate full changelog
generate_full_changelog() {
    echo -e "${YELLOW}Generating full changelog from git history...${NC}"
    
    local temp_file=$(mktemp)
    
    cat > "$temp_file" << 'EOF'
# Changelog

All notable changes to LGX Runtime Core will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

EOF
    
    # Get all tags in reverse chronological order
    local tags=$(git tag -l --sort=-version:refname)
    
    if [ -z "$tags" ]; then
        # No tags, generate from all commits
        local version=$(get_version)
        local changes=$(categorize_commits "" "HEAD")
        echo "## [${version}] - $(date +%Y-%m-%d)" >> "$temp_file"
        echo "" >> "$temp_file"
        echo "$changes" >> "$temp_file"
    else
        # Generate for each tag
        local previous_tag=""
        for tag in $tags; do
            local version=${tag#v}
            local tag_date=$(git log -1 --pretty=%cd --date=short "$tag")
            
            echo "## [${version}] - ${tag_date}" >> "$temp_file"
            echo "" >> "$temp_file"
            
            local changes=$(categorize_commits "$previous_tag" "$tag")
            echo "$changes" >> "$temp_file"
            echo "" >> "$temp_file"
            
            previous_tag=$tag
        done
    fi
    
    mv "$temp_file" "$CHANGELOG_FILE"
    
    echo -e "${GREEN}✓ Full changelog generated${NC}"
}

# Show usage
usage() {
    cat << EOF
Usage: $0 <command> [options]

Commands:
  generate [version]    Generate changelog entry for version
  update [version]      Update CHANGELOG.md with new entry
  full                  Regenerate full changelog from git history
  show [version]        Show changelog for version

Examples:
  $0 generate 1.2.3     # Generate changelog for version 1.2.3
  $0 update 1.2.3       # Update CHANGELOG.md with version 1.2.3
  $0 full               # Regenerate full changelog
  $0 show 1.2.3         # Show changelog for version 1.2.3

Conventional Commits:
  This script parses git commits following Conventional Commits format:
  
  <type>(<scope>): <description>
  
  Types:
    feat:     New feature
    fix:      Bug fix
    perf:     Performance improvement
    docs:     Documentation changes
    style:    Code style changes
    refactor: Code refactoring
    test:     Test changes
    build:    Build system changes
    ci:       CI/CD changes
    chore:    Maintenance tasks
    revert:   Revert previous commit
  
  Breaking changes:
    Add "BREAKING CHANGE:" in commit body or use "!" after type/scope

EOF
}

# Main
main() {
    if [ $# -eq 0 ]; then
        usage
        exit 1
    fi
    
    case $1 in
        generate)
            local version=${2:-$(get_version)}
            generate_version_changelog "$version"
            ;;
        update)
            local version=${2:-$(get_version)}
            local entry=$(generate_version_changelog "$version")
            update_changelog "$version" "$entry"
            ;;
        full)
            generate_full_changelog
            ;;
        show)
            local version=${2:-$(get_version)}
            if [ -f "$CHANGELOG_FILE" ]; then
                sed -n "/## \[${version}\]/,/## \[/p" "$CHANGELOG_FILE" | sed '$d'
            else
                echo -e "${RED}Error: CHANGELOG.md not found${NC}"
                exit 1
            fi
            ;;
        *)
            echo -e "${RED}Error: Unknown command: $1${NC}"
            usage
            exit 1
            ;;
    esac
}

main "$@"
