LGX Runtime Core - Arch User Repository (AUR) Setup Guide
===========================================================

This guide explains how to publish LGX Runtime Core to the Arch User
Repository (AUR) for easy installation on Arch Linux and derivatives.

PREREQUISITES

  - Arch Linux system (or Arch-based distribution)
  - AUR account (https://aur.archlinux.org)
  - SSH key registered with AUR
  - Git installed
  - makepkg and base-devel installed

INITIAL SETUP

  1. Create AUR account:
     Visit https://aur.archlinux.org/register
     Create account and verify email

  2. Generate SSH key (if you don't have one):
     $ ssh-keygen -t ed25519 -C "your.email@example.com"
     $ cat ~/.ssh/id_ed25519.pub

  3. Add SSH key to AUR:
     Visit https://aur.archlinux.org/account/
     Paste public key in "SSH Public Key" field

  4. Test SSH connection:
     $ ssh -T aur@aur.archlinux.org
     Expected: "Hi YOUR_USERNAME! You've successfully authenticated..."

  5. Install base-devel (if not already installed):
     $ sudo pacman -S base-devel git

CREATING AUR PACKAGE

  1. Clone AUR repository (first time):
     $ git clone ssh://aur@aur.archlinux.org/lgx-runtime.git
     $ cd lgx-runtime

     Note: Repository will be empty on first clone

  2. Copy PKGBUILD from packaging directory:
     $ cp ../packaging/arch/PKGBUILD .

  3. Review and update PKGBUILD:
     Edit PKGBUILD to ensure correct version:

     pkgname=lgx-runtime
     pkgver=1.0.1
     pkgrel=1
     pkgdesc="High-performance Linux gaming runtime with specialized memory allocators"
     arch=('x86_64')
     url="https://github.com/dream1290/LGX"
     license=('Apache')
     depends=('glibc' 'vulkan-icd-loader' 'jemalloc')
     makedepends=('cmake' 'gcc' 'vulkan-headers')
     source=("$pkgname-$pkgver.tar.gz::https://github.com/dream1290/LGX/archive/v$pkgver.tar.gz")
     sha256sums=('SKIP')  # Update with actual checksum

     build() {
         cd "LGX-$pkgver"
         cmake -B build \
             -DCMAKE_BUILD_TYPE=Release \
             -DCMAKE_INSTALL_PREFIX=/usr
         cmake --build build
     }

     check() {
         cd "LGX-$pkgver/build"
         ctest --output-on-failure
     }

     package() {
         cd "LGX-$pkgver"
         DESTDIR="$pkgdir" cmake --install build
     }

  4. Generate checksums:
     $ updpkgsums

     This updates sha256sums in PKGBUILD with actual checksums

  5. Test build locally:
     $ makepkg -si

     This builds and installs the package locally for testing

  6. Generate .SRCINFO:
     $ makepkg --printsrcinfo > .SRCINFO

     This file is required by AUR and contains package metadata

UPLOADING TO AUR

  1. Add files to git:
     $ git add PKGBUILD .SRCINFO

  2. Commit changes:
     $ git commit -m "Update to version 1.0.1"

     Commit message should describe changes:
     - "Initial upload of lgx-runtime"
     - "Update to version 1.0.1"
     - "Fix build dependencies"
     - "Update checksums"

  3. Push to AUR:
     $ git push origin master

  4. Verify upload:
     Visit https://aur.archlinux.org/packages/lgx-runtime
     Package should appear within a few minutes

INSTALLATION FROM AUR

  Users can install using an AUR helper:

  Using yay:
  $ yay -S lgx-runtime

  Using paru:
  $ paru -S lgx-runtime

  Manual installation:
  $ git clone https://aur.archlinux.org/lgx-runtime.git
  $ cd lgx-runtime
  $ makepkg -si

UPDATING PACKAGE

  1. Update version in PKGBUILD:
     pkgver=1.0.2
     pkgrel=1

  2. Update source URL if needed:
     source=("$pkgname-$pkgver.tar.gz::https://github.com/dream1290/LGX/archive/v$pkgver.tar.gz")

  3. Update checksums:
     $ updpkgsums

  4. Test build:
     $ makepkg -si

  5. Regenerate .SRCINFO:
     $ makepkg --printsrcinfo > .SRCINFO

  6. Commit and push:
     $ git add PKGBUILD .SRCINFO
     $ git commit -m "Update to version 1.0.2"
     $ git push origin master

AUTOMATION SCRIPT

  Create a script to automate AUR updates:

  #!/bin/bash
  # update-aur.sh

  VERSION="$1"
  if [ -z "$VERSION" ]; then
      echo "Usage: $0 <version>"
      exit 1
  fi

  AUR_DIR="lgx-runtime-aur"

  # Clone or update AUR repository
  if [ ! -d "$AUR_DIR" ]; then
      git clone ssh://aur@aur.archlinux.org/lgx-runtime.git "$AUR_DIR"
  fi

  cd "$AUR_DIR"
  git pull

  # Update PKGBUILD version
  sed -i "s/^pkgver=.*/pkgver=$VERSION/" PKGBUILD
  sed -i "s/^pkgrel=.*/pkgrel=1/" PKGBUILD

  # Update checksums
  updpkgsums

  # Test build
  echo "Testing build..."
  makepkg -sf || exit 1

  # Generate .SRCINFO
  makepkg --printsrcinfo > .SRCINFO

  # Commit and push
  git add PKGBUILD .SRCINFO
  git commit -m "Update to version $VERSION"
  git push origin master

  echo "Successfully updated AUR package to version $VERSION"
  echo "Visit: https://aur.archlinux.org/packages/lgx-runtime"

PKGBUILD BEST PRACTICES

  - Use $pkgname and $pkgver variables in source URLs
  - Always include sha256sums (never use 'SKIP' in production)
  - Test build in clean chroot: makepkg -sc
  - Include check() function for running tests
  - Follow Arch packaging standards
  - Keep dependencies minimal
  - Use proper license identifiers

SPLIT PACKAGES

  To create separate runtime and development packages:

  pkgbase=lgx-runtime
  pkgname=('lgx-runtime' 'lgx-runtime-dev')
  pkgver=1.0.1
  pkgrel=1

  package_lgx-runtime() {
      pkgdesc="LGX Runtime Core - Runtime library"
      depends=('glibc' 'vulkan-icd-loader' 'jemalloc')
      
      cd "LGX-$pkgver"
      DESTDIR="$pkgdir" cmake --install build --component runtime
  }

  package_lgx-runtime-dev() {
      pkgdesc="LGX Runtime Core - Development files"
      depends=('lgx-runtime')
      
      cd "LGX-$pkgver"
      DESTDIR="$pkgdir" cmake --install build --component development
  }

TROUBLESHOOTING

  Build failures:
    - Check dependencies in depends and makedepends
    - Test build in clean chroot: makepkg -sc
    - Review build log for errors
    - Verify source URL is accessible

  Checksum mismatches:
    - Run updpkgsums to regenerate checksums
    - Ensure source tarball hasn't changed
    - Clear package cache: rm -rf src/ pkg/

  SSH authentication errors:
    - Verify SSH key is added to AUR account
    - Test connection: ssh -T aur@aur.archlinux.org
    - Check SSH key permissions: chmod 600 ~/.ssh/id_ed25519

  .SRCINFO out of sync:
    - Always regenerate after PKGBUILD changes
    - Run: makepkg --printsrcinfo > .SRCINFO
    - Commit both PKGBUILD and .SRCINFO together

  Version conflicts:
    - Increment pkgrel if pkgver stays same
    - Reset pkgrel to 1 when pkgver increases
    - Never decrease version numbers

MAINTAINING AUR PACKAGE

  - Respond to comments on AUR page
  - Monitor out-of-date flags
  - Update package within 1-2 days of upstream release
  - Test on clean Arch installation periodically
  - Keep dependencies up to date
  - Follow Arch packaging guidelines

ORPHANING/DISOWNING

  If you can no longer maintain the package:

  1. Visit https://aur.archlinux.org/packages/lgx-runtime
  2. Click "Disown Package"
  3. Package becomes available for adoption by other users

  To adopt an orphaned package:
  1. Visit package page
  2. Click "Adopt Package"

REFERENCES

  - AUR Submission Guidelines: https://wiki.archlinux.org/title/AUR_submission_guidelines
  - PKGBUILD: https://wiki.archlinux.org/title/PKGBUILD
  - Arch Package Guidelines: https://wiki.archlinux.org/title/Arch_package_guidelines
  - makepkg: https://wiki.archlinux.org/title/Makepkg

SUPPORT

  For issues with AUR:
  - AUR Discussion: https://bbs.archlinux.org/viewforum.php?id=4
  - Arch Wiki: https://wiki.archlinux.org/

  For LGX Runtime issues:
  - GitHub: https://github.com/dream1290/LGX/issues
