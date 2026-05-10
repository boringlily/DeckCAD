# macOS SDK for cross-compiling

`docker build --target macos-cross` needs a macOS SDK tarball here, named
`MacOSX*.sdk.tar.xz`. Apple's license doesn't allow redistributing it, so
this repo can't fetch or bundle one for you -- you have to package it
yourself from your own Xcode install, using osxcross's own instructions:
https://github.com/tpoechtrager/osxcross#packaging-the-sdk

Files matching `*.sdk.tar.xz` in this directory are git-ignored.
