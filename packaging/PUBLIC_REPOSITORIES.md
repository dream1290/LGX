LGX Runtime Core - Public Repository Distribution Guide
========================================================

This document provides an overview of distributing LGX Runtime Core
through public package repositories for easy installation across
major Linux distributions.

OVERVIEW

  LGX Runtime Core is available through three major public repositories:

  1. Ubuntu PPA (Personal Package Archive)
     - For Ubuntu and Debian-based distributions
     - Easy installation with apt

  2. Fedora Copr (Cool Other Package Repo)
     - For Fedora, RHEL, Rocky Linux, AlmaLinux
     - Easy installation with dnf

  3. Arch User Repository (AUR)
     - For Arch Linux and derivatives
     - Installation via AUR helpers or makepkg

QUICK INSTALLATION

  Ubuntu/Debian (via PPA):
  $ sudo add-apt-repository ppa:YOUR_USERNAME/lgx-runtime
  $ sudo apt update
  $ sudo apt install lgx-runtime lgx-runtime-dev

  Fedora/RHEL (via Copr):
  $ sudo dnf copr enable YOUR_USERNAME/lgx-runtime
  $ sudo dnf install lgx-runtime lgx-runtime-devel

  Arch Linux (via AUR):
  $ yay -S lgx-runtime
  # or
  $ paru -S lgx-runtime

REPOSITORY SETUP GUIDES

  Detailed setup instructions for each repository:

  - PPA_SETUP.md    - Ubuntu PPA configuration and upload
  - COPR_SETUP.md   - Fedora Copr configuration and build
  - AUR_SETUP.md    - Arch AUR package creation and maintenance

DISTRIBUTION COMPARISON

  Feature                 | PPA      | Copr     | AUR
  ------------------------|----------|----------|----------
  Binary packages         | Yes      | Yes      | No*
  Automatic updates       | Yes      | Yes      | Manual
  Multi-version support   | Yes      | Yes      | No
  Build on upload         | Yes      | Yes      | User builds
  Signing required        | GPG      | Optional | Optional
  Review process          | None     | None     | Community
  Hosting                 | Launchpad| Fedora   | AUR
  
  * AUR provides build scripts, users compile locally

RELEASE WORKFLOW

  When releasing a new version of LGX Runtime Core:

  1. Update version in source:
     - CMakeLists.txt
     - include/lgx_version.h
     - CHANGELOG.md

  2. Create Git tag:
     $ git tag -a v1.0.1 -m "Release version 1.0.1"
     $ git push origin v1.0.1

  3. Create GitHub release:
     - Visit https://github.com/dream1290/LGX/releases/new
     - Select tag v1.0.1
     - Add release notes
     - Attach source tarball

  4. Update PPA:
     $ cd packaging
     $ dch -v 1.0.1-1ubuntu1 "New upstream release"
     $ debuild -S -sa -k YOUR_GPG_KEY
     $ dput ppa:YOUR_USERNAME/lgx-runtime ../lgx-runtime_1.0.1-1ubuntu1_source.changes

  5. Update Copr:
     $ copr-cli buildscm lgx-runtime \
         --clone-url https://github.com/dream1290/LGX.git \
         --commit v1.0.1 \
         --subdir packaging/rpm \
         --spec lgx-runtime.spec

  6. Update AUR:
     $ cd lgx-runtime-aur
     $ sed -i "s/^pkgver=.*/pkgver=1.0.1/" PKGBUILD
     $ updpkgsums
     $ makepkg --printsrcinfo > .SRCINFO
     $ git add PKGBUILD .SRCINFO
     $ git commit -m "Update to version 1.0.1"
     $ git push origin master

AUTOMATION

  Consider automating releases with GitHub Actions:

  .github/workflows/release.yml:

  name: Release to Public Repositories
  on:
    release:
      types: [published]

  jobs:
    ppa:
      runs-on: ubuntu-latest
      steps:
        - uses: actions/checkout@v3
        - name: Upload to PPA
          run: ./packaging/scripts/upload-ppa.sh
          env:
            GPG_KEY: ${{ secrets.GPG_PRIVATE_KEY }}
            PPA_NAME: ${{ secrets.PPA_NAME }}

    copr:
      runs-on: ubuntu-latest
      steps:
        - uses: actions/checkout@v3
        - name: Build on Copr
          run: ./packaging/scripts/upload-copr.sh
          env:
            COPR_TOKEN: ${{ secrets.COPR_API_TOKEN }}

    aur:
      runs-on: ubuntu-latest
      steps:
        - uses: actions/checkout@v3
        - name: Update AUR
          run: ./packaging/scripts/update-aur.sh
          env:
            AUR_SSH_KEY: ${{ secrets.AUR_SSH_PRIVATE_KEY }}

MONITORING

  Monitor package status across repositories:

  PPA:
  - Build status: https://launchpad.net/~YOUR_USERNAME/+archive/ubuntu/lgx-runtime
  - Download stats: Available on Launchpad
  - User feedback: Launchpad bugs and questions

  Copr:
  - Build status: https://copr.fedorainfracloud.org/coprs/YOUR_USERNAME/lgx-runtime
  - Download stats: Available on Copr dashboard
  - User feedback: Copr comments and issues

  AUR:
  - Package page: https://aur.archlinux.org/packages/lgx-runtime
  - Vote count: Indicates popularity
  - Comments: User feedback and issues
  - Out-of-date flags: Notifications when update needed

MAINTENANCE

  Regular maintenance tasks:

  Weekly:
  - Check for build failures
  - Respond to user comments and issues
  - Monitor download statistics

  Monthly:
  - Review and update dependencies
  - Check for security updates
  - Update documentation

  Per Release:
  - Update all three repositories
  - Test installation on each platform
  - Update release notes
  - Announce on relevant channels

BEST PRACTICES

  Version numbering:
  - Use semantic versioning (MAJOR.MINOR.PATCH)
  - Increment MAJOR for breaking changes
  - Increment MINOR for new features
  - Increment PATCH for bug fixes

  Changelog:
  - Keep detailed changelog
  - Include all user-visible changes
  - Reference issue numbers
  - Credit contributors

  Testing:
  - Test builds locally before uploading
  - Verify installation on clean systems
  - Run test suite on each platform
  - Check for dependency issues

  Communication:
  - Announce releases on GitHub
  - Update documentation
  - Respond to user feedback promptly
  - Maintain active presence in communities

TROUBLESHOOTING

  Build failures across all platforms:
  - Check source tarball integrity
  - Verify CMake configuration
  - Review build dependencies
  - Test local build first

  Platform-specific failures:
  - Check distribution-specific dependencies
  - Review packaging files (control, spec, PKGBUILD)
  - Test on that specific distribution
  - Consult platform documentation

  User installation issues:
  - Verify repository is properly configured
  - Check for conflicting packages
  - Review dependency resolution
  - Provide clear installation instructions

SUPPORT CHANNELS

  For packaging issues:
  - PPA: Launchpad help and Ubuntu forums
  - Copr: Fedora discussion and Copr issues
  - AUR: Arch forums and AUR comments

  For LGX Runtime issues:
  - GitHub Issues: https://github.com/dream1290/LGX/issues
  - GitHub Discussions: https://github.com/dream1290/LGX/discussions

REFERENCES

  - PPA_SETUP.md - Detailed PPA setup guide
  - COPR_SETUP.md - Detailed Copr setup guide
  - AUR_SETUP.md - Detailed AUR setup guide
  - README.md - General packaging information
  - QUICKSTART.md - Quick start guide

GETTING STARTED

  1. Read the setup guide for your target platform
  2. Set up accounts and authentication
  3. Test build locally
  4. Upload to repository
  5. Monitor build status
  6. Announce availability

For questions or assistance, open an issue on GitHub:
https://github.com/dream1290/LGX/issues
