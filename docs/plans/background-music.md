# Background music delivery plan

| Platform | Music delivery |
|---|---|
| Desktop | Bundle MP3s alongside the executable. |
| Steam / Steam Deck | Bundle MP3s alongside the executable. |
| Cardputer Zero | Download from Cloudflare when music is enabled; cache for offline playback. |
| Cardputer ADV | No music support, controls, downloads, or music assets. |

## 1. Publish the playlist on Cloudflare

Use Workers Static Assets alongside the existing palette-feed Worker.

Generate a versioned playlist manifest from `assets/audio/music/`, containing each track's ID, title, order, URL, file size, and checksum. Publish the manifest and MP3s under `/music/`.

Use versioned filenames so replacing a track cannot mix old cached files with new metadata. Verify the music endpoints and existing palette endpoints before switching the app to downloads.

## 2. Keep the Zero package small

Change the build rules so Zero packages contain the player and cartridge sound effect, with no bundled MP3s.

Desktop and Steam builds continue copying the complete playlist into their installation folders. Update the Zero build-transfer script to exclude the MP3s as well.

The Zero simulator should use the same download behavior as the physical Zero.

## 3. Download only when music is enabled

Music remains off by default, with no music-related network requests while disabled.

When enabled:

1. Play any verified cached tracks immediately.
2. Fetch the playlist manifest in the background.
3. Download the first missing track, then the remaining tracks sequentially.
4. Start playback when the first track is ready.

Downloading must continue without blocking drawing, menus, or dialogs. Keep the user's music preference enabled while loading, rather than treating an empty cache as a playback failure.

## 4. Cache reliably on the Zero

Store downloaded music under:

`~/.local/share/bitmap16dx/music/`

Download into temporary files, verify their size and checksum, then rename them into place. Incomplete files must never appear in the playable playlist.

Reuse completed downloads after restarts. Network failures leave cached music playable and offer a retry. Check available storage before downloading.

Newly downloaded tracks should join the playlist in manifest order without restarting the current song or reopening the audio output.

## 5. Preserve the agreed controls

- **Fn+Q:** Play/pause, preserving the current playback position.
- **Fn+W:** Previous track.
- **Fn+E:** Next track.
- Desktop and USB keyboards use their corresponding media keys.
- Settings > Audio provides independent sound effects and music on/off toggles, plus separate Sound effects volume and Music volume percentages. Mix levels balance app audio independently of system volume.
- Master volume and previous/next track use key commands.

While downloading, previous/next operates on ready tracks. Turning music off cancels outstanding downloads and retains completed files.

Show concise progress such as **Downloading music 2/4**. Add **Retry music download** and **Remove downloaded music** where applicable. Removing music stops playback and downloading, clears the music cache, and turns music off.

Cartridge sound effects remain bundled and play over background music.

## 6. Verify and release

Automated checks should cover:

- Correct packaging for all four platforms.
- No downloads while music is off.
- First-track playback before the full playlist finishes downloading.
- Interrupted downloads, retries, corrupt files, and insufficient storage.
- Offline playback and cached-file reuse.
- Pause/resume and track controls throughout the app, including dialogs.
- Adding completed tracks without interrupting playback.
- Compatibility with existing settings.

Use small test fixtures and simulated downloads for automated tests.

Finish with a physical Zero test on Wi-Fi and offline, plus a Steam Deck check confirming its bundled playlist works without internet access.
