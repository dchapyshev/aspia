Updates
=======
An update is a set of static files on a web server: two JSON files per release and a signature next
to each of them. There is no service behind them and no database, nothing is stored about the
machine that asks, and publishing a release means uploading files and committing text.

This document is what to write into those files and what the applications do with them.

Layout
------
A channel is a directory. An application builds the address it reads as
`<server>/<channel>/latest.json`, where the server is `https://aspia.org/updates`.

```
updates/
  stable/
    latest.json
    latest.json.sig
  beta/
    latest.json
    latest.json.sig
    3.0.5.json
    3.0.5.json.sig
  alpha/
    latest.json
    latest.json.sig
```

There are three channels: `stable`, `beta` and `alpha`. A name that is not one of them is read as
the stable one, so a setting written by another version cannot send a check elsewhere.

**Every channel must have a `latest.json`, even when nothing is offered.** An empty directory is
indistinguishable from a server that is down: the application gets a 404 and reports a failed check
instead of "no updates". The smallest file that says "nothing here":

```json
{
  "format": 1,
  "targets": {},
  "updates": []
}
```

The packages themselves are not served from this directory. A release manifest names the directory
they lie in, and that can be anywhere.

latest.json
-----------
Read at every check. It answers one question: what is offered to the version that is asking.

```json
{
  "format": 1,
  "targets": {
    "latest": "3.0.5"
  },
  "updates": [
    { "source": "2.6.0", "target": "3.0.0" },
    { "source": "3.0.4", "target": "@latest" },
    { "source": "3.0.5", "target": "@latest" }
  ]
}
```

- `format` is the version of the schema and is 1. A file with anything else is not read.
- `source` is an exact version, one entry per released version. There are no version masks.
- A version here has three parts. The fourth part of `3.0.5.8006` is the build number and takes no
  part in any comparison.
- `target` is a version or a label written as `@name`; the keys of `targets` are written without
  the `@`. The prefix is what makes a typo visible: a version can never begin with `@`, so a
  misspelled label cannot be mistaken for a version.
- A label is the only place where the current version is written down. A release moves the label
  and leaves the entries alone, and moving the label back withdraws a release.
- A migration barrier, where a version can only be updated through an intermediate one, is written
  as a literal version and is not moved by later releases: `2.6.0 -> 3.0.0`.

What an application makes of it:

| Case                                                 | Result                             |
| :--------------------------------------------------- | :--------------------------------- |
| No entry whose `source` is the running version       | No updates, and no further request |
| `target` names a label that `targets` does not have  | The check fails                    |
| `target` is not a version                            | The check fails                    |
| The target version is not newer than the running one | No updates                         |
| The target version is newer                          | Its manifest is read next          |

A target equal to its own version is not an error, so the entry for a release can be published
together with that release: it does nothing until the label moves on.

Release manifest
----------------
`<version>.json` describes one release: what it is, where its files are and what they must hash to.

```json
{
  "format": 1,
  "version": "3.0.5",
  "description": "Maintenance release, no functional changes since 3.0.4.",
  "path": "https://files.aspia.net/beta/3.0.5",
  "packages": {
    "host": {
      "windows": {
        "x86_64": [
          { "format": "msi", "file": "aspia-host-3.0.5-x86_64.msi", "sha256": "d431..." }
        ],
        "x86": [
          { "format": "msi", "file": "aspia-host-3.0.5-x86.msi", "sha256": "594f..." }
        ]
      },
      "linux": {
        "x86_64": [
          { "format": "deb", "file": "aspia-host-3.0.5-x86_64.deb", "sha256": "6618..." },
          { "format": "rpm", "file": "aspia-host-3.0.5-x86_64.rpm", "sha256": "7e81..." }
        ]
      },
      "macosx": {
        "x86_64": [
          { "format": "pkg", "file": "aspia-host-3.0.5-universal.pkg", "sha256": "d5cb..." }
        ],
        "arm64": [
          { "format": "pkg", "file": "aspia-host-3.0.5-universal.pkg", "sha256": "d5cb..." }
        ]
      },
      "android": {
        "arm64": [
          { "format": "apk", "file": "aspia-host-3.0.5-arm64.apk", "sha256": "9bad..." }
        ]
      }
    },
    "client": { },
    "router": { },
    "relay": { }
  }
}
```

The names under `packages` are the ones an application works out for itself, and nothing else is
looked at:

| Level            | Values                                  |
| :--------------- | :-------------------------------------- |
| Application      | `host`, `client`, `router`, `relay`     |
| Operating system | `windows`, `linux`, `macosx`, `android` |
| Architecture     | `x86`, `x86_64`, `arm`, `arm64`         |
| Package format   | `msi`, `deb`, `rpm`, `pkg`, `apk`       |

A key that is absent is legal and means the release has no build for that combination: no update is
offered and nothing is reported as broken. That is how a platform is dropped. Since 3.0.3 the
32-bit Windows build ships the host alone, and the manifest simply has no `client` under
`windows` / `x86`.

The files of one architecture are a list because Linux has two formats and which one to install
depends on the package manager of the machine. The entry whose `format` matches what the machine
prefers is taken; if none matches, the first entry is taken, and the user may be left to install it
by hand.

An entry names a file, not an address. The files of a release lie in one directory, `path` names it
once, and moving the storage is one line to change instead of nineteen. File sizes are not
recorded: the hash covers everything a size would.

Requirements. Anything broken here is a failed check, not "no updates":

| Field                 | Rule                                                                 |
| :-------------------- | :------------------------------------------------------------------- |
| `format`              | Is 1                                                                 |
| `version`             | A version, and the same one the rules pointed at                     |
| `description`         | One line of English, at most 4096 characters, and not translated     |
| `path`                | Not empty, without a trailing slash (an extra one is trimmed anyway) |
| `file`                | Not empty, a file name and not an address                            |
| `path` + `/` + `file` | Between 10 and 256 characters                                        |
| `sha256`              | Exactly 64 characters, lowercase hexadecimal                         |
| `format`              | Not empty, one of the formats listed above                           |

A manifest is written once and is not edited afterwards. A mistake in a release is corrected by the
next release, not by rewriting what was published.

Signatures
----------
Every file has `<name>.sig` next to it with one line in it:

```
1:xhTQ0HNUb8y9k5fZ8oDVTGpVJ0kHTqOaFqLkqz9F8Rk8mQeXhE6c0tIY1FZKQe7L5H1p8vF3sJmZ0rQyWm0nBw==
```

The number before the colon is the version of the signature scheme, not the name of an algorithm:
it covers everything that can change, the algorithm, what exactly is signed and how it is encoded.
Version 1 is Ed25519 over the bytes of the file with the signature in base64. A version that is not
known, or a missing signature, means the file is treated as absent.

An application checks a signature against the public keys built into it. There is room for more
than one, so a key can be replaced without cutting off the versions that know only the old one.

```
aspia_signer genkey <private-key>               creates a key and prints its public half
aspia_signer sign <private-key> <file>...       writes <file>.sig next to each file
aspia_signer signdir <private-key> <dir>...     signs every json file under the directory
aspia_signer verify <public-key> <file>...      checks each <file>.sig
aspia_signer verifydir <public-key> <dir>...    checks every json file under the directory
```

A private key is a file. A public key is the key itself in hex or a file holding the private key it
belongs to, so the same file works for signing and for checking.

A channel, or the whole `updates` directory, is signed and checked in one go with `signdir` and
`verifydir`. They take the manifests lying under the directory, subdirectories included, and not the
signatures next to them. A signature does not depend on when it was made, so a file signed again
keeps the bytes it had.

Signing is the last thing done to a file. A file edited after signing keeps the signature of what
it used to be, and every application will read it as missing. Before publishing, verify with the
public key, which is exactly what the applications will do.

What an application does
------------------------
First it works out what it is. The operating system and the architecture come from the build; the
package format it prefers is `msi` on Windows, `apk` on Android, `pkg` on macOS, and on Linux `deb`
when `apt-get` is on the machine or `rpm` when `dnf` is.

1. Reads `latest.json` of its channel and its signature.
2. Finds the entry for its own version and resolves the target, a label through `targets`. Nothing
   offered ends the check here, with no further request.
3. Reads `<target>.json` and its signature.
4. Takes the file for its application, system and architecture, and checks that the version inside
   the manifest is the one the rules pointed at.
5. Downloads the file into a directory only its owner can write to, and compares the hash with the
   manifest. A file that does not match is refused and nothing is started.
6. Hands the package to the system. Windows gets `msiexec /i`, quiet when nobody is watching;
   macOS `installer -pkg`; Linux `apt-get install -y` or `dnf install -y`; Android passes the file
   to the installer of the system, which asks the user.

What this asks of the hosting:

- Both JSON files and both signatures answer over `http` or `https` with code 200. Redirects are
  followed, up to fifteen of them.
- A JSON file is at most 1 MiB.
- The certificate of the server is verified.
- A check has 30 seconds to connect and 60 seconds in total, so the files have to be served
  quickly. The packages themselves are downloaded without such a deadline.
- A cache or CDN in front of the files holds them for minutes, so a published release is not
  visible at once.

Because the packages are installed without anyone answering questions, a package has to install
quietly, replace a running installation, and bring the service back up by itself.

When applications check
-----------------------
**Client.** Once at start, when checking is enabled in its settings, and whenever a person asks in
the settings. What is found is offered in a dialog.

**Host, asked for.** A person opens the settings of the host and asks.

**Host, by itself.** On Windows, when automatic updates are on, no more often than the number of
days set there. Nobody is watching: it downloads and installs quietly. A router can also tell a
host to check now, and that goes the same way.

**Router and relay.** Only when asked from the command line: `--check-update` and
`--install-update`, with `--update-channel` naming the channel.

Example
-------
A Windows host of version 3.0.4.8006 on the beta channel, against the files above:

1. `https://aspia.org/updates/beta/latest.json` and its signature are read, and the signature is
   good.
2. The running version is 3.0.4, the entry `{ "source": "3.0.4", "target": "@latest" }` matches,
   the label `latest` resolves to 3.0.5, which is newer.
3. `https://aspia.org/updates/beta/3.0.5.json` and its signature are read.
4. Under `packages.host.windows.x86_64` there is one entry, an `msi`, which is what Windows wants.
5. The file is `https://files.aspia.net/beta/3.0.5/aspia-host-3.0.5-x86_64.msi` and must hash to
   what the entry says.
6. The package is installed quietly. It stops the service, replaces the files and starts it again.

The same host on Linux would take the `rpm` entry where `dnf` is the manager and the `deb` entry
where `apt-get` is. A client on 32-bit Windows would find nothing under
`packages.client.windows.x86` and would be told there is no update.

Publishing a release
--------------------
1. Upload the files of the release to the directory `path` will name. The hashes go into the
   manifest from exactly those files: a release rebuilt with another build number hashes
   differently, and the installation then stops on a package that does not match the manifest.
2. Add `<version>.json` to the channel.
3. In `latest.json`, move `targets.latest` to the new version and add an entry for the version that
   came before it.
4. Sign every file that changed and verify all of them with the public key.
5. Commit the channel in one go. There is no intermediate state to be caught in.

Nothing else in the files is touched. The installers of versions that entries still lead to,
intermediate ones included, stay where they are.

Withdrawing a version
---------------------
Removing the entry whose `source` is that version ends support for it: machines running it are told
there is nothing, and they stay where they are until someone installs a new version by hand.
A version withdrawn for good loses its manifest and its signature as well, and then nothing leads
into it or out of it.

A release that should no longer be installed is taken back by moving the label to the previous
version. The manifest and the packages can stay: without a rule leading to it, nothing reads them.
