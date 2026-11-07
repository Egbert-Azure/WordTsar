1. Download `WordTsar.dmg` from the [latest release](https://github.com/Egbert-Azure/WordTsar/releases/latest), open it, and drag `WordTsar` onto `Applications`.
2. Open WordTsar once. macOS blocks it because it isn't notarized by Apple. Click **Done**.
3. Open **System Settings → Privacy & Security**, scroll to **Security**, and click **Open Anyway** next to WordTsar. Confirm with your password or Touch ID.

On macOS 14 Sonoma and earlier, right-click `WordTsar.app`, choose **Open**, and confirm instead.

Comfortable in Terminal? `xattr -dr com.apple.quarantine /Applications/WordTsar.app` does the same. You may need to repeat this after installing a new release.
