"""Global hotkey listener, bridged into Qt via signals.

Built on pynput's X11 backend. Under pure Wayland, pynput cannot register a
truly global hotkey (compositors sandbox input by design) unless the
compositor exposes a portal, or the process has evdev/uinput access -
starting the listener there will raise, which we surface as an error signal
instead of crashing the app. XWayland apps are unaffected.
"""

from __future__ import annotations

import logging

from PyQt6.QtCore import QObject, pyqtSignal
from pynput import keyboard

logger = logging.getLogger("luu.hotkey")


class HotkeyListener(QObject):
    """Runs pynput's GlobalHotKeys listener on a background thread.

    triggered: emitted (on the pynput thread's callback, but Qt marshals
               queued-connection signals back to the receiver's thread) each
               time the configured combo fires.
    error: emitted with a human-readable message if the listener could not
           be started (bad hotkey string, no input permission, etc.).
    """

    triggered = pyqtSignal()
    error = pyqtSignal(str)

    def __init__(self, hotkey: str = "<alt>+u", parent=None):
        super().__init__(parent)
        self._hotkey = hotkey
        self._listener: keyboard.GlobalHotKeys | None = None

    @property
    def hotkey(self) -> str:
        return self._hotkey

    def start(self) -> bool:
        """(Re)starts listening for the configured hotkey. Returns False on failure."""
        self.stop()
        try:
            self._listener = keyboard.GlobalHotKeys({self._hotkey: self.triggered.emit})
            self._listener.start()
            logger.info("Listening for hotkey %s", self._hotkey)
            return True
        except Exception as e:  # pynput raises plain Exception/ValueError variants
            logger.error("Failed to register hotkey %r: %s", self._hotkey, e)
            self.error.emit(
                f"Could not register hotkey '{self._hotkey}': {e}\n"
                "On Wayland this usually means the compositor blocks global "
                "input listeners for non-privileged apps."
            )
            self._listener = None
            return False

    def set_hotkey(self, hotkey: str) -> bool:
        """Changes the combo and restarts the listener with it."""
        self._hotkey = hotkey
        return self.start()

    def stop(self) -> None:
        if self._listener is not None:
            self._listener.stop()
            self._listener = None

    def is_running(self) -> bool:
        return self._listener is not None
