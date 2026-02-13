LGX Runtime Core - Ubuntu PPA Setup Guide
==========================================

This guide explains how to publish LGX Runtime Core to a Ubuntu PPA
(Personal Package Archive) for easy installation on Ubuntu and Debian systems.

PREREQUISITES

  - Launchpad account (https://launchpad.net)
  - GPG key registered with Launchpad
  - dput configured for PPA uploads
  - Ubuntu/Debian build environment

INITIAL SETUP

  1. Create Launchpad account:
     Visit https://launchpad.net and create an account

  2. Generate GPG key (if you don't have one):
     $ gpg --full-generate-key
     Select: (1) RSA and RSA
     Key size: 4096
     Expiration: 0 (does not expire) or set expiration
     Enter your name and email

  3. Upload GPG key to Launchpad:
     $ gpg --list-keys
     $ gpg --send-keys YOUR_KEY_ID
     $ gpg --keyserver keyserver.ubuntu.com --send-keys YOUR_KEY_ID

     Then add the key fingerprint to your Launchpad profile:
     https://launchpad.net/~/+editpgpkeys

  4. Create PPA on Launchpad:
     Visit https://launchpad.net/~/+activate-ppa
     PPA name: lgx-runtime
     Display name: LGX Runtime Core
     Description: High-performance Linux gaming runtime with specialized
                  memory allocators and hardware adaptation

  5. Configure dput:
     Create or edit ~/.dput.cf:

     [ppa:YOUR_LAUNCHPAD_ID/lgx-runtime]
     fqdn = ppa.launchpad.net
     method = ftp
     incoming = ~YOUR_LAUNCHPAD_ID/ubuntu/lgx-runtime/
     login = anonymous
     allow_unsigned_uploads = 0

BUILDING SOURCE PACKAGE

  1. Ensure debian/ directory is up to date:
     $ cd packaging/debian
     $ ls -la
     Should contain: control, rules, changelog, copyright, compat, *.install

  2. Update changelog for new version:
     $ cd ../..  # Back to project root
     $ dch -v 1.0.1-1ubuntu1 "New upstream release"
     $ dch -r ""

     Or manually edit packaging/debian/changelog:

     lgx-runtime (1.0.1-1ubuntu1) jammy; urgency=medium

       * New upstream release 1.0.1
       * Added comprehensive benchmark suite
       * Fixed compilation warnings in Release builds
       * Enhanced security audit compliance

      -- Your Name <your.email@example.com>  Thu, 13 Feb 2026 10:00:00 +0000

  3. Build source package:
     $ cd packaging
     $ debuild -S -sa -k YOUR_GPG_KEY_ID

     This creates:
     - lgx-runtime_1.0.1-1ubuntu1.dsc
     - lgx-runtime_1.0.1-1ubuntu1.tar.xz
     - lgx-runtime_1.0.1-1ubuntu1_source.changes
     - lgx-runtime_1.0.1-1ubuntu1_source.build

  4. Verify source package:
     $ lintian lgx-runtime_1.0.1-1ubuntu1_source.changes

UPLOADING TO PPA

  1. Upload to PPA:
     $ dput ppa:YOUR_LAUNCHPAD_ID/lgx-runtime lgx-runtime_1.0.1-1ubuntu1_source.changes

  2. Check upload status:
     Visit https://launchpad.net/~YOUR_LAUNCHPAD_ID/+archive/ubuntu/lgx-runtime
     
     Build status will show:
     - Pending: Waiting to build
     - Building: Currently building
     - Successfully built: Ready for installation
     - Failed to build: Check build log for errors

  3. Monitor build logs:
     Click on the build status to view detailed logs
     Builds are performed for each Ubuntu release (jammy, noble, etc.)

MULTI-RELEASE SUPPORT

  To support multiple Ubuntu releases, upload for each:

  1. Update changelog for each release:
     $ dch -v 1.0.1-1ubuntu1~jammy1 "Backport to Ubuntu 22.04 Jammy"
     $ debuild -S -sa -k YOUR_GPG_KEY_ID
     $ dput ppa:YOUR_LAUNCHPAD_ID/lgx-runtime lgx-runtime_1.0.1-1ubuntu1~jammy1_source.changes

     $ dch -v 1.0.1-1ubuntu1~noble1 "Backport to Ubuntu 24.04 Noble"
     $ debuild -S -sa -k YOUR_GPG_KEY_ID
     $ dput ppa:YOUR_LAUNCHPAD_ID/lgx-runtime lgx-runtime_1.0.1-1ubuntu1~noble1_source.changes

INSTALLATION FROM PPA

  Once published, users can install with:

  $ sudo add-apt-repository ppa:YOUR_LAUNCHPAD_ID/lgx-runtime
  $ sudo apt update
  $ sudo apt install lgx-runtime lgx-runtime-dev

AUTOMATION SCRIPT

  Create a script to automate PPA uploads:

  #!/bin/bash
  # upload-to-ppa.sh

  VERSION="1.0.1"
  RELEASES=("jammy" "noble" "focal")
  PPA="ppa:YOUR_LAUNCHPAD_ID/lgx-runtime"
  GPG_KEY="YOUR_GPG_KEY_ID"

  for RELEASE in "${RELEASES[@]}"; do
      echo "Building for $RELEASE..."
      
      # Update changelog
      dch -v "${VERSION}-1ubuntu1~${RELEASE}1" "Backport to Ubuntu $RELEASE"
      dch -r ""
      
      # Build source package
      debuild -S -sa -k "$GPG_KEY"
      
      # Upload to PPA
      dput "$PPA" "../lgx-runtime_${VERSION}-1ubuntu1~${RELEASE}1_source.changes"
      
      # Clean up
      git checkout packaging/debian/changelog
  done

  echo "All uploads complete. Check Launchpad for build status."

TROUBLESHOOTING

  Upload rejected:
    - Verify GPG key is registered with Launchpad
    - Check that version number is higher than previous uploads
    - Ensure changelog entry matches version in upload

  Build failures:
    - Check build dependencies in debian/control
    - Review build log on Launchpad
    - Test build locally: debuild -b

  Missing dependencies:
    - Add to Build-Depends in debian/control
    - Ensure dependencies are available in Ubuntu repositories

  Version conflicts:
    - Use ~ for backports: 1.0.1-1ubuntu1~jammy1
    - Increment ubuntu version: 1.0.1-1ubuntu2

BEST PRACTICES

  - Always test builds locally before uploading
  - Use semantic versioning for upstream releases
  - Include detailed changelog entries
  - Support at least 2 Ubuntu LTS releases
  - Monitor build failures and fix promptly
  - Respond to user issues on Launchpad

REFERENCES

  - Launchpad PPA Guide: https://help.launchpad.net/Packaging/PPA
  - Debian Packaging: https://www.debian.org/doc/manuals/maint-guide/
  - Ubuntu Packaging: https://packaging.ubuntu.com/html/

SUPPORT

  For issues with PPA setup:
  - Launchpad Help: https://answers.launchpad.net/launchpad
  - Ubuntu Packaging: https://discourse.ubuntu.com/c/packaging/

For LGX Runtime issues:
  - GitHub: https://github.com/dream1290/LGX/issues
