"""Config persistence and the Settings dialog for LUU."""

from __future__ import annotations

import copy
import json
import logging
from pathlib import Path
from typing import Any

from PyQt6.QtWidgets import (
    QCheckBox,
    QComboBox,
    QDialog,
    QDialogButtonBox,
    QFormLayout,
    QKeySequenceEdit,
    QLineEdit,
    QMessageBox,
    QSpinBox,
)
from PyQt6.QtGui import QKeySequence

logger = logging.getLogger("luu.settings")

CONFIG_DIR = Path.home() / ".config" / "luu"
CONFIG_PATH = CONFIG_DIR / "settings.json"

DEFAULTS: dict[str, Any] = {
    "hotkey": "<alt>+u",
    "upscale_mode": "fsr",
    "target_resolution": [1920, 1080],
    "frame_gen_enabled": True,
    "framegen_method": "lsfg",
    "quality": "high",
    "capture_backend": "auto",
    "capture_output": "",
}

UPSCALE_MODES = ["fsr", "lanczos", "bilinear", "nearest"]
FRAMEGEN_METHODS = ["lsfg", "interpolation"]
QUALITY_LEVELS = ["low", "medium", "high", "ultra"]


class ConfigManager:
    """Loads/saves ~/.config/luu/settings.json, merged onto DEFAULTS.

    A corrupt file is backed up (settings.json.bak) instead of being
    silently overwritten, so a bad manual edit never loses the user's data.
    """

    def __init__(self, path: Path = CONFIG_PATH):
        self.path = path
        self.data: dict[str, Any] = copy.deepcopy(DEFAULTS)

    def load(self) -> None:
        if not self.path.exists():
            logger.info("No config at %s, writing defaults", self.path)
            self.save()
            return

        try:
            with self.path.open("r", encoding="utf-8") as f:
                loaded = json.load(f)
        except (OSError, json.JSONDecodeError) as e:
            logger.error("Failed to read %s: %s", self.path, e)
            backup = self.path.with_suffix(self.path.suffix + ".bak")
            try:
                backup.write_bytes(self.path.read_bytes())
                logger.warning("Backed up corrupt config to %s", backup)
            except OSError:
                pass
            self.data = copy.deepcopy(DEFAULTS)
            return

        merged = copy.deepcopy(DEFAULTS)
        merged.update(loaded)
        self.data = merged

    def save(self) -> bool:
        try:
            self.path.parent.mkdir(parents=True, exist_ok=True)
            with self.path.open("w", encoding="utf-8") as f:
                json.dump(self.data, f, indent=4)
            return True
        except OSError as e:
            logger.error("Failed to write %s: %s", self.path, e)
            return False

    def get(self, key: str, default: Any = None) -> Any:
        return self.data.get(key, default)

    def set(self, key: str, value: Any) -> None:
        self.data[key] = value


class SettingsDialog(QDialog):
    """Modal form for editing upscale/frame-gen/hotkey settings."""

    def __init__(self, config: ConfigManager, parent=None):
        super().__init__(parent)
        self.config = config
        self.setWindowTitle("LUU Settings")
        self.setMinimumWidth(360)

        self.hotkey_edit = QKeySequenceEdit(QKeySequence(self._hotkey_to_qt(config.get("hotkey"))))

        self.upscale_combo = QComboBox()
        self.upscale_combo.addItems(UPSCALE_MODES)
        self.upscale_combo.setCurrentText(config.get("upscale_mode", "fsr"))

        self.framegen_check = QCheckBox("Enable Frame Generation")
        self.framegen_check.setChecked(bool(config.get("frame_gen_enabled", True)))

        self.framegen_combo = QComboBox()
        self.framegen_combo.addItems(FRAMEGEN_METHODS)
        self.framegen_combo.setCurrentText(config.get("framegen_method", "lsfg"))

        self.quality_combo = QComboBox()
        self.quality_combo.addItems(QUALITY_LEVELS)
        self.quality_combo.setCurrentText(config.get("quality", "high"))

        width, height = config.get("target_resolution", [1920, 1080])
        self.width_spin = QSpinBox()
        self.width_spin.setRange(320, 7680)
        self.width_spin.setValue(width)
        self.height_spin = QSpinBox()
        self.height_spin.setRange(240, 4320)
        self.height_spin.setValue(height)

        self.capture_output_edit = QLineEdit(config.get("capture_output", ""))
        self.capture_output_edit.setPlaceholderText("e.g. DP-2 (see `hyprctl monitors`) - empty = auto")

        form = QFormLayout()
        form.addRow("Hotkey", self.hotkey_edit)
        form.addRow("Upscale mode", self.upscale_combo)
        form.addRow(self.framegen_check)
        form.addRow("Frame gen method", self.framegen_combo)
        form.addRow("Quality", self.quality_combo)
        form.addRow("Target width", self.width_spin)
        form.addRow("Target height", self.height_spin)
        form.addRow("Capture output", self.capture_output_edit)

        buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Save | QDialogButtonBox.StandardButton.Cancel
        )
        buttons.accepted.connect(self._on_save)
        buttons.rejected.connect(self.reject)
        form.addRow(buttons)

        self.setLayout(form)

    @staticmethod
    def _hotkey_to_qt(hotkey: str) -> str:
        # pynput style "<alt>+u" -> Qt style "Alt+U"
        parts = [p.strip("<>") for p in hotkey.split("+")]
        return "+".join(p.capitalize() if len(p) > 1 else p.upper() for p in parts)

    @staticmethod
    def _qt_to_hotkey(qt_seq: QKeySequence) -> str:
        text = qt_seq.toString()
        if not text:
            return DEFAULTS["hotkey"]
        parts = [p.strip() for p in text.split("+")]
        out = []
        for p in parts[:-1]:
            out.append(f"<{p.lower()}>")
        out.append(parts[-1].lower())
        return "+".join(out)

    def _on_save(self):
        self.config.set("hotkey", self._qt_to_hotkey(self.hotkey_edit.keySequence()))
        self.config.set("upscale_mode", self.upscale_combo.currentText())
        self.config.set("frame_gen_enabled", self.framegen_check.isChecked())
        self.config.set("framegen_method", self.framegen_combo.currentText())
        self.config.set("quality", self.quality_combo.currentText())
        self.config.set(
            "target_resolution", [self.width_spin.value(), self.height_spin.value()]
        )
        self.config.set("capture_output", self.capture_output_edit.text().strip())

        if self.config.save():
            self.accept()
        else:
            QMessageBox.critical(
                self,
                "Save failed",
                f"Could not write settings to {self.config.path}.\n"
                "Check that the directory is writable.",
            )
