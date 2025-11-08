#!/usr/bin/env python3
"""Download Rocket League car presets from bakkesplugins.com.

This helper fetches the public car preset catalogue and writes it to the
pipe-delimited format understood by the Expanded Presets plugin. Because the
website is protected by Cloudflare, the script attempts to use the optional
``cloudscraper`` module when it is available. Without it you may receive HTTP
403 errors; install the dependency via ``pip install cloudscraper`` if that
happens.  If the popular ``requests`` package is unavailable the script falls
back to Python's standard ``urllib`` module so Windows users can run it with a
vanilla interpreter.

The generated file contains one preset per line using the following schema:

    Name|LoadoutCode|primaryR,primaryG,primaryB|accentR,accentG,accentB|
    Car|Decal|Wheels|MatteFlag|PearlescentFlag

Use the ``--install`` switch to copy the resulting ``bakkesplugins_cars.cfg``
directly into your BakkesMod data folder. The script attempts to locate common
install paths automatically but you can supply ``--install-path`` to override
it.
"""

import argparse
import json
import logging
import os
import shutil
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable, List, Optional, Sequence, Tuple

try:  # pragma: no cover - dependency is optional
    import cloudscraper  # type: ignore
except ImportError:  # pragma: no cover - dependency is optional
    cloudscraper = None  # type: ignore

try:
    import requests
except ImportError:  # pragma: no cover - requests is optional now
    requests = None  # type: ignore

if requests is None:  # pragma: no cover - executed only when requests missing
    import urllib.error
    import urllib.parse
    import urllib.request

LOGGER = logging.getLogger("bakkesplugins")
BASE_URL = "https://bakkesplugins.com"
CATALOG_ENDPOINT = f"{BASE_URL}/api/presets"
DETAIL_ENDPOINT = f"{BASE_URL}/api/presets/{{slug}}"

DEFAULT_HEADERS = {
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
        "AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/119.0 Safari/537.36"
    ),
    "Accept": "application/json, text/plain, */*",
}


class HTTPStatusError(RuntimeError):
    """Raised when an HTTP request returns a non-success status code."""

    def __init__(self, status_code: int, body: str):
        super().__init__(f"HTTP {status_code}: {body[:200]}")
        self.status_code = status_code
        self.body = body


class SimpleResponse:
    def __init__(self, status_code: int, text: str):
        self.status_code = status_code
        self._text = text
        self._json: Optional[object] = None

    def raise_for_status(self) -> None:
        if not 200 <= self.status_code < 300:
            raise HTTPStatusError(self.status_code, self._text)

    def json(self) -> object:
        if self._json is None:
            self._json = json.loads(self._text or "{}")
        return self._json


class SimpleSession:
    """Fallback HTTP session using urllib when requests is unavailable."""

    def __init__(self) -> None:
        self.headers = DEFAULT_HEADERS.copy()

    def get(self, url: str, *, params: Optional[dict] = None, timeout: int = 30) -> SimpleResponse:
        if params:
            query = urllib.parse.urlencode(params)
            delimiter = "&" if "?" in url else "?"
            url = f"{url}{delimiter}{query}"

        request = urllib.request.Request(url, headers=self.headers)

        try:
            with urllib.request.urlopen(request, timeout=timeout) as response:  # nosec B310 - network request is intended
                text = response.read().decode("utf-8")
                status = response.getcode() or 0
        except urllib.error.HTTPError as exc:  # pragma: no cover - error path
            text = exc.read().decode("utf-8", errors="ignore")
            status = exc.code
            return SimpleResponse(status, text)
        except urllib.error.URLError as exc:  # pragma: no cover - error path
            raise RuntimeError(f"Failed to fetch {url}: {exc}") from exc

        return SimpleResponse(status, text)


def iter_candidate_data_dirs() -> Iterable[Path]:
    env_data_dir = os.environ.get("BAKKESMOD_DATA_DIR") or os.environ.get("BAKKESMOD_DATA")
    if env_data_dir:
        yield Path(env_data_dir)

    env_root = os.environ.get("BAKKESMOD_DIR")
    if env_root:
        yield Path(env_root) / "data"
        yield Path(env_root) / "bakkesmod" / "data"

    appdata = os.environ.get("APPDATA")
    if appdata:
        base = Path(appdata) / "bakkesmod" / "bakkesmod"
        yield base / "data"

    local_appdata = os.environ.get("LOCALAPPDATA")
    if local_appdata:
        base = Path(local_appdata) / "bakkesmod" / "bakkesmod"
        yield base / "data"

    home = Path.home()
    manual_candidates = [
        home / "AppData" / "Roaming" / "bakkesmod" / "bakkesmod" / "data",
        home / "AppData" / "Local" / "bakkesmod" / "bakkesmod" / "data",
        home / "Documents" / "BakkesMod" / "data",
        home / "BakkesMod" / "data",
        home / "bakkesmod" / "data",
    ]
    for candidate in manual_candidates:
        yield candidate


def determine_install_path(override: Optional[Path], output_name: str) -> Optional[Path]:
    if override is not None:
        override = override.expanduser()
        if override.is_dir() or not override.suffix:
            try:
                override.mkdir(parents=True, exist_ok=True)
            except OSError as exc:  # pragma: no cover - filesystem edge case
                LOGGER.error("Failed to create directory %s: %s", override, exc)
                return None
            return override / output_name
        try:
            override.parent.mkdir(parents=True, exist_ok=True)
        except OSError as exc:  # pragma: no cover - filesystem edge case
            LOGGER.error("Failed to prepare %s: %s", override.parent, exc)
            return None
        return override

    for base in iter_candidate_data_dirs():
        base = base.expanduser()
        if not base.exists() or not base.is_dir():
            continue

        destination_dir = base
        if destination_dir.name.lower() != "expandedpresets":
            destination_dir = destination_dir / "ExpandedPresets"

        try:
            destination_dir.mkdir(parents=True, exist_ok=True)
        except OSError as exc:  # pragma: no cover - filesystem edge case
            LOGGER.debug("Failed to prepare %s: %s", destination_dir, exc)
            continue

        return destination_dir / output_name

    return None


@dataclass
class Preset:
    name: str
    loadout: str
    primary_color: Tuple[float, float, float]
    accent_color: Tuple[float, float, float]
    car: str
    decal: str
    wheels: str
    matte: bool
    pearlescent: bool

    def to_cfg_line(self) -> str:
        def fmt(color: Tuple[float, float, float]) -> str:
            r, g, b = color
            return f"{r:.3f},{g:.3f},{b:.3f}"

        return "|".join(
            [
                self.name.replace("|", "/"),
                self.loadout.strip(),
                fmt(self.primary_color),
                fmt(self.accent_color),
                self.car.replace("|", "/"),
                self.decal.replace("|", "/"),
                self.wheels.replace("|", "/"),
                "1" if self.matte else "0",
                "1" if self.pearlescent else "0",
            ]
        )


def create_session() -> Any:
    if cloudscraper is not None and requests is not None:  # pragma: no cover - optional dependency
        LOGGER.debug("Using cloudscraper session")
        return cloudscraper.create_scraper()

    if requests is not None:
        session = requests.Session()
        session.headers.update(DEFAULT_HEADERS)
        return session

    return SimpleSession()


def clamp_color(value: Optional[float]) -> float:
    if value is None:
        return 0.0
    value = float(value)
    return max(0.0, min(1.0, value / 255.0 if value > 1 else value))


def parse_color(data: object, fallback: Tuple[float, float, float]) -> Tuple[float, float, float]:
    if isinstance(data, Sequence) and len(data) >= 3:
        return tuple(clamp_color(float(component)) for component in data[:3])  # type: ignore[arg-type]

    if isinstance(data, dict):
        r = clamp_color(data.get("r") or data.get("red") or data.get("R"))
        g = clamp_color(data.get("g") or data.get("green") or data.get("G"))
        b = clamp_color(data.get("b") or data.get("blue") or data.get("B"))
        return r, g, b

    return fallback


def extract_item_name(data: object, *keys: str, default: str = "Unknown") -> str:
    if not isinstance(data, dict):
        return default
    node: object = data
    for key in keys:
        if not isinstance(node, dict):
            return default
        node = node.get(key, {})
    if isinstance(node, dict):
        name = node.get("name") or node.get("label")
        if isinstance(name, str):
            return name
    if isinstance(node, str):
        return node
    return default


def normalise_preset(raw: dict) -> Optional[Preset]:
    name = raw.get("name") or raw.get("title") or raw.get("displayName")
    if not isinstance(name, str) or not name.strip():
        return None

    loadout = raw.get("loadout") or raw.get("code") or raw.get("loadout_code")
    if not isinstance(loadout, str) or not loadout.strip():
        return None

    colours = raw.get("colors") or raw.get("colours") or raw.get("paint") or {}
    primary = parse_color(colours.get("primary") if isinstance(colours, dict) else None, (0.18, 0.18, 0.18))
    accent = parse_color(colours.get("accent") if isinstance(colours, dict) else None, (0.9, 0.35, 0.15))

    items = raw.get("items") or raw.get("loadoutItems") or {}
    car = extract_item_name(items, "body", default="Octane")
    decal = extract_item_name(items, "decal", default="None")
    wheels = extract_item_name(items, "wheels", default="OEM")

    finishes = raw.get("finishes") or {}
    matte = bool(finishes.get("matte") or raw.get("matte"))
    pearlescent = bool(finishes.get("pearlescent") or raw.get("pearlescent"))

    return Preset(
        name=name.strip(),
        loadout=loadout.strip(),
        primary_color=primary,
        accent_color=accent,
        car=car,
        decal=decal,
        wheels=wheels,
        matte=matte,
        pearlescent=pearlescent,
    )


def fetch_catalog_page(session: Any, page: int, page_size: int) -> dict:
    params = {"type": "cars", "perPage": page_size, "page": page}
    LOGGER.debug("Fetching catalog page %s", page)
    response = session.get(CATALOG_ENDPOINT, params=params, timeout=30)
    if response.status_code == 403:
        raise RuntimeError(
            "Access denied by bakkesplugins.com. Install 'cloudscraper' or "
            "run the script with a VPN/standard browser session."
        )
    response.raise_for_status()
    return response.json()


def iter_catalog(session: Any, limit: Optional[int], page_size: int, sleep: float) -> Iterable[dict]:
    page = 1
    fetched = 0
    last_page: Optional[int] = None

    while True:
        data = fetch_catalog_page(session, page, page_size)
        entries: Sequence[dict] = data.get("data") or data.get("results") or []  # type: ignore[assignment]
        meta = data.get("meta") or {}
        if last_page is None:
            last_page = int(meta.get("last_page") or meta.get("lastPage") or page)
        LOGGER.info("Fetched %d entries from page %d/%s", len(entries), page, last_page)

        for entry in entries:
            yield entry
            fetched += 1
            if limit is not None and fetched >= limit:
                return

        page += 1
        if last_page is not None and page > last_page:
            break
        if sleep > 0:
            time.sleep(sleep)


def enrich_entry(session: Any, entry: dict) -> dict:
    slug = entry.get("slug") or entry.get("uuid")
    if not slug:
        return entry
    try:
        response = session.get(DETAIL_ENDPOINT.format(slug=slug), timeout=30)
        response.raise_for_status()
        details = response.json()
        if isinstance(details, dict):
            entry = {**details, **entry}
    except Exception as exc:
        LOGGER.warning("Failed to fetch detail for %s: %s", slug, exc)
    return entry


def build_presets(session: Any, raw_entries: Iterable[dict], fetch_details: bool) -> List[Preset]:
    presets: List[Preset] = []
    for raw in raw_entries:
        if fetch_details:
            raw = enrich_entry(session, raw)
        preset = normalise_preset(raw)
        if preset is None:
            LOGGER.debug("Skipping entry without loadout code: %s", raw.get("slug") or raw.get("name"))
            continue
        presets.append(preset)
    return presets


def write_presets(presets: Sequence[Preset], output_path: Path) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", encoding="utf-8") as file:
        for preset in presets:
            file.write(preset.to_cfg_line())
            file.write("\n")
    LOGGER.info("Wrote %d presets to %s", len(presets), output_path)


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path("bakkesplugins_cars.cfg"), help="Destination file")
    parser.add_argument("--limit", type=int, default=None, help="Maximum presets to download (for testing)")
    parser.add_argument("--page-size", type=int, default=100, help="Number of presets to request per page")
    parser.add_argument("--sleep", type=float, default=0.75, help="Delay between page requests to avoid rate limits")
    parser.add_argument(
        "--details",
        action="store_true",
        help="Fetch individual preset details to obtain full loadout data",
    )
    parser.add_argument(
        "--install",
        action="store_true",
        help="Copy the generated file into the ExpandedPresets data directory",
    )
    parser.add_argument(
        "--install-path",
        type=Path,
        default=None,
        metavar="PATH",
        help="Override the destination directory or file when using --install",
    )
    parser.add_argument("--log-level", default="INFO", help="Logging level (DEBUG, INFO, WARNING, ...)")
    return parser.parse_args(argv)


def main(argv: Sequence[str]) -> int:
    args = parse_args(argv)
    logging.basicConfig(level=getattr(logging, args.log_level.upper(), logging.INFO), format="%(levelname)s: %(message)s")

    session = create_session()
    raw_entries = list(iter_catalog(session, args.limit, args.page_size, args.sleep))
    presets = build_presets(session, raw_entries, args.details)
    if not presets:
        LOGGER.error("No presets were downloaded. Check your connection or API changes.")
        return 1

    write_presets(presets, args.output)
    if args.install or args.install_path is not None:
        destination = determine_install_path(args.install_path, args.output.name)
        if destination is None:
            LOGGER.error(
                "Unable to determine the BakkesMod data directory automatically. "
                "Pass --install-path to specify it explicitly."
            )
            return 1

        try:
            if destination.resolve() == args.output.resolve():
                LOGGER.info("Catalog already generated at %s", destination)
            else:
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(args.output, destination)
                LOGGER.info("Copied catalog to %s", destination)
        except OSError as exc:
            LOGGER.error("Failed to copy catalog to %s: %s", destination, exc)
            return 1

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
