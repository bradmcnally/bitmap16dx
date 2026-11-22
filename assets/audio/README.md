# Audio sources and private assets

The app code is MIT licensed. Third-party music and sound effects retain their
own licenses. [CREDITS.md](CREDITS.md) contains the audio attribution notices;
[manifest.json](manifest.json) records each file's source, license, purpose,
size and SHA-256 checksum. Sources were reviewed on November 21, 2026.

## Music in the public repository

The eight MP3s in `assets/audio/music/` come from three CC BY 4.0 sources listed
in the credits. They may be redistributed with the required attribution and
license notices. Keep the credits with copies of the music and app releases.
CMake copies `CREDITS.md` into the build's `music/` folder; Zero packaging and
the Steam Deck installer carry that folder into the installed app.

When adding music, record its creator, original title, source link, exact
license and any conversion or edits in the credits and manifest before adding
the file to Git. New sources need their own license review.

## Private Cyberleaf sound effects

All 13 WAVs currently in `assets/audio/` are from **Modern UI SFX v2** by
**Andrea Baroni / Cyberleaf Studio**:
<https://cyberleaf.itch.io/modern-ui-sfx>

The pack's included `LICENSE.txt`, headed **LICENSE (June 2021)**, permits
commercial and non-commercial app use. It prohibits standalone redistribution,
sublicensing, sharing, and distribution that lets end users easily extract
the original audio files. These files are not covered by the project's MIT
license. Attribution is optional for paid licenses.

The project's ignore rules keep WAVs and other unreviewed audio files out of
ordinary Git adds. The public manifest records their identities without
including the recordings. Ignoring a file does not back it up or remove any
copy that was previously committed.

Keep the purchased ZIP, its included license, and purchase information in your
private, backed-up asset storage. Restore the listed WAVs from your licensed
copy into `assets/audio/` before building an SDL target on another machine.
Keep any version history of these private assets separate from the public
project, with access limited according to the license.

The current SDL build requires the effects named in `CMakeLists.txt` and the
five `keys_minimal` samples. `enable_01b.wav` and `transition_soft_01b.wav` are
local audition files that are not currently embedded.

The extractability restriction needs clarification from Cyberleaf before
publishing builds using the current WAV byte-array embedding. Git ignore rules
address source uploads; embedding alone does not establish compliance with
that separate distribution restriction.

## Updating the file inventory

Update `manifest.json` when an audio file changes. To check a local file's
identity, compare its SHA-256 checksum with the manifest:

```sh
shasum -a 256 assets/audio/*.wav assets/audio/music/*.mp3
```

Changes to the public credits and manifest can be reviewed in Git even when
the corresponding restricted recording remains private.
