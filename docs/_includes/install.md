1. Download `WordTsar.dmg` from the [latest release](https://github.com/Egbert-Azure/WordTsar/releases/latest), open it, and drag `WordTsar` onto `Applications`.
2. Open WordTsar once. macOS blocks it because it isn't notarized by Apple. Click **Done**.
3. Open **System Settings → Privacy & Security**, scroll to **Security**, and click **Open Anyway** next to WordTsar. Confirm with your password or Touch ID.

On macOS 14 Sonoma and earlier, right-click `WordTsar.app`, choose **Open**, and confirm instead.

Comfortable in Terminal? `xattr -dr com.apple.quarantine /Applications/WordTsar.app` does the same. You may need to repeat this after installing a new release.

`ws`, the terminal version, is in the same download and runs on its own. With `WordTsar.dmg` still open, type these in Terminal:

1. `mkdir -p ~/bin`
2. `rm -f ~/bin/ws && cp /Volumes/WordTsar/ws ~/bin/`
3. `echo 'export PATH="$HOME/bin:$PATH"' >> ~/.zshrc` puts `~/bin` on your PATH. You only need this once.
4. `xattr -d com.apple.quarantine ~/bin/ws` lets macOS run it. If it says there is no such attribute, carry on.
5. Open a new Terminal window and type `ws` and a file name, such as `ws mybook.ws`.

The templates folder in the download holds BOOK.WS and the macro kit; see [templates/README.md](https://github.com/Egbert-Azure/WordTsar/blob/main/templates/README.md).
