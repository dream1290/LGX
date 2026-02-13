LGX Runtime Core - Fedora Copr Setup Guide
===========================================

This guide explains how to publish LGX Runtime Core to Fedora Copr
(Cool Other Package Repo) for easy installation on Fedora, RHEL, and
compatible distributions.

PREREQUISITES

  - Fedora Account System (FAS) account
  - Copr account (https://copr.fedorainfracloud.org)
  - copr-cli tool installed
  - RPM build environment

INITIAL SETUP

  1. Create Fedora Account:
     Visit https://accounts.fedoraproject.org/
     Create account and verify email

  2. Create Copr account:
     Visit https://copr.fedorainfracloud.org/
     Sign in with Fedora Account
     Accept terms of service

  3. Install copr-cli:
     $ sudo dnf install copr-cli

  4. Configure copr-cli:
     Visit https://copr.fedorainfracloud.org/api/
     Copy API token

     Create ~/.config/copr:

     [copr-cli]
     login = YOUR_FAS_USERNAME
     username = YOUR_FAS_USERNAME
     token = YOUR_API_TOKEN
     copr_url = https://copr.fedorainfracloud.org

  5. Create Copr project:
     $ copr-cli create lgx-runtime \
         --chroot fedora-38-x86_64 \
         --chroot fedora-39-x86_64 \
         --chroot fedora-40-x86_64 \
         --chroot epel-8-x86_64 \
         --chroot epel-9-x86_64 \
         --description "High-performance Linux gaming runtime" \
         --instructions "sudo dnf copr enable YOUR_USERNAME/lgx-runtime && sudo dnf install lgx-runtime"

     Or create via web interface:
     https://copr.fedorainfracloud.org/coprs/add/

BUILDING RPM PACKAGE

  1. Ensure spec file is up to date:
     $ cd packaging/rpm
     $ cat lgx-runtime.spec

  2. Update version in spec file:
     Edit packaging/rpm/lgx-runtime.spec:

     Version:        1.0.1
     Release:        1%{?dist}

     %changelog
     * Thu Feb 13 2026 Your Name <your.email@example.com> - 1.0.1-1
     - New upstream release 1.0.1
     - Added comprehensive benchmark suite
     - Fixed compilation warnings in Release builds
     - Enhanced security audit compliance

  3. Create source tarball:
     $ cd ../..  # Back to project root
     $ git archive --format=tar.gz --prefix=lgx-runtime-1.0.1/ \
         -o packaging/rpm/lgx-runtime-1.0.1.tar.gz v1.0.1

     Or create from current state:
     $ tar czf packaging/rpm/lgx-runtime-1.0.1.tar.gz \
         --transform 's,^,lgx-runtime-1.0.1/,' \
         --exclude='.git' \
         --exclude='build*' \
         --exclude='packaging/output' \
         .

  4. Test build locally (optional but recommended):
     $ cd packaging/rpm
     $ rpmbuild -ba lgx-runtime.spec

UPLOADING TO COPR

  Method 1: Build from spec file and source tarball

    $ cd packaging/rpm
    $ copr-cli build lgx-runtime lgx-runtime.spec \
        --srpm lgx-runtime-1.0.1.tar.gz

  Method 2: Build from SRPM

    1. Create SRPM:
       $ rpmbuild -bs lgx-runtime.spec

    2. Upload SRPM:
       $ copr-cli build lgx-runtime ~/rpmbuild/SRPMS/lgx-runtime-1.0.1-1.fc*.src.rpm

  Method 3: Build from Git (recommended)

    1. Push to GitHub:
       $ git push origin main
       $ git push origin v1.0.1

    2. Build from Git:
       $ copr-cli buildscm lgx-runtime \
           --clone-url https://github.com/dream1290/LGX.git \
           --commit v1.0.1 \
           --subdir packaging/rpm \
           --spec lgx-runtime.spec \
           --type git \
           --method make_srpm

MONITORING BUILDS

  1. Check build status:
     $ copr-cli status lgx-runtime

  2. View build details:
     Visit https://copr.fedorainfracloud.org/coprs/YOUR_USERNAME/lgx-runtime/builds/

  3. Monitor build logs:
     Click on individual builds to view logs
     Builds are performed for each enabled chroot (Fedora 38, 39, 40, EPEL 8, 9)

  4. Download built packages:
     $ copr-cli download-build BUILD_ID

INSTALLATION FROM COPR

  Once published, users can install with:

  Fedora:
  $ sudo dnf copr enable YOUR_USERNAME/lgx-runtime
  $ sudo dnf install lgx-runtime lgx-runtime-devel

  RHEL/Rocky/Alma (requires EPEL):
  $ sudo dnf install epel-release
  $ sudo dnf copr enable YOUR_USERNAME/lgx-runtime
  $ sudo dnf install lgx-runtime lgx-runtime-devel

AUTOMATION SCRIPT

  Create a script to automate Copr builds:

  #!/bin/bash
  # upload-to-copr.sh

  VERSION="1.0.1"
  PROJECT="lgx-runtime"
  GIT_URL="https://github.com/dream1290/LGX.git"
  GIT_TAG="v${VERSION}"

  echo "Building LGX Runtime ${VERSION} on Copr..."

  # Build from Git
  copr-cli buildscm "$PROJECT" \
      --clone-url "$GIT_URL" \
      --commit "$GIT_TAG" \
      --subdir packaging/rpm \
      --spec lgx-runtime.spec \
      --type git \
      --method make_srpm

  echo "Build submitted. Check status at:"
  echo "https://copr.fedorainfracloud.org/coprs/$(whoami)/${PROJECT}/builds/"

MANAGING COPR PROJECT

  Enable/disable chroots:
  $ copr-cli edit-chroot lgx-runtime/fedora-41-x86_64 --enable
  $ copr-cli edit-chroot lgx-runtime/fedora-37-x86_64 --disable

  Delete old builds:
  $ copr-cli delete-build BUILD_ID

  Modify project settings:
  $ copr-cli modify lgx-runtime \
      --description "Updated description" \
      --instructions "Updated installation instructions"

  Delete project (careful!):
  $ copr-cli delete lgx-runtime

TROUBLESHOOTING

  Build failures:
    - Check build log on Copr web interface
    - Verify BuildRequires in spec file
    - Test build locally: rpmbuild -ba lgx-runtime.spec
    - Ensure source tarball is accessible

  Missing dependencies:
    - Add to BuildRequires in spec file
    - Check if dependencies are in Fedora/EPEL repos
    - May need to add additional Copr repos as build dependencies

  Authentication errors:
    - Verify ~/.config/copr configuration
    - Regenerate API token if expired
    - Check FAS account is active

  Version conflicts:
    - Increment Release number in spec file
    - Use %{?dist} macro for distribution-specific releases
    - Clear old builds if needed

BEST PRACTICES

  - Test builds locally before uploading to Copr
  - Use Git-based builds for reproducibility
  - Support at least 2 recent Fedora releases
  - Include EPEL builds for RHEL/Rocky/Alma users
  - Keep spec file in sync with upstream releases
  - Monitor build failures and fix promptly
  - Respond to user issues on Copr

MULTI-ARCHITECTURE SUPPORT

  To add ARM64 support:

  $ copr-cli modify lgx-runtime \
      --chroot fedora-38-aarch64 \
      --chroot fedora-39-aarch64 \
      --chroot fedora-40-aarch64

  Note: Ensure code is portable to ARM64 architecture

REFERENCES

  - Copr User Documentation: https://docs.pagure.org/copr.copr/
  - RPM Packaging Guide: https://rpm-packaging-guide.github.io/
  - Fedora Packaging Guidelines: https://docs.fedoraproject.org/en-US/packaging-guidelines/

SUPPORT

  For issues with Copr:
  - Copr Support: https://pagure.io/copr/copr/issues
  - Fedora Packaging: https://discussion.fedoraproject.org/c/package-maintenance/

  For LGX Runtime issues:
  - GitHub: https://github.com/dream1290/LGX/issues
