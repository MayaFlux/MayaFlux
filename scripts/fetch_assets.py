#!/usr/bin/env python3
"""Applies data/assets.json to a checkout: fetches each asset and places its files under data/.

Usage:
    python3 scripts/fetch_assets.py                  fetch every asset for this platform
    python3 scripts/fetch_assets.py silero_vad       fetch only the named assets
    python3 scripts/fetch_assets.py --check          report what is present, no network
    python3 scripts/fetch_assets.py --replace        overwrite files whose content differs

A file that already exists with the right SHA-256 is left alone. A file that
exists with different content is reported and kept unless --replace is given.
A file with no recorded SHA-256 (a git file) is taken as present when it
exists, because the pinned commit already fixes what a fetch would give.

Sources: files at https URLs, either one URL for the source (a single file or a
zip archive) or one URL per file, so an asset may draw from several hosts; or a
git repository at a full commit. Git needs the git executable. Only the Python
standard library is used. When the jsonschema module is installed the list is
also checked against data/assets.schema.json.

Exit status is 0 when every selected file is in place, 1 otherwise.
"""

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import urllib.error
import urllib.parse
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PLATFORMS = {'linux': 'linux', 'darwin': 'macos', 'win32': 'windows'}
IDENTIFIER = re.compile(r'^[a-z][a-z0-9_]*$')
SHA256 = re.compile(r'^[A-Fa-f0-9]{64}$')
COMMIT = re.compile(r'^[a-f0-9]{40}$')
RELATIVE = re.compile(r'^(?!.*(?:^|/)\.{1,2}(?:/|$))[^/\\:<>"|?*\x00-\x1f\x7f]+(?:/[^/\\:<>"|?*\x00-\x1f\x7f]+)*$')
CHUNK = 1 << 16


class AssetError(Exception):
    pass


def https_url(value, what):
    parsed = urllib.parse.urlsplit(value)
    if parsed.scheme != 'https' or not parsed.hostname or parsed.username or parsed.password:
        raise AssetError(f'{what} must be an https URL without credentials: {value}')
    return value


def check_rules(document):
    if document.get('schema_version') != 1:
        raise AssetError('schema_version must be 1')
    assets = document.get('assets')
    if not isinstance(assets, list):
        raise AssetError('assets must be a list')
    ids = set()
    destinations = set()
    for asset in assets:
        name = asset.get('id', '')
        if not IDENTIFIER.match(name):
            raise AssetError(f'invalid asset id: {name!r}')
        if name in ids:
            raise AssetError(f'duplicate asset id: {name}')
        ids.add(name)
        if asset.get('root', 'data') != 'data':
            raise AssetError(f'{name}: the shipped list may only use root "data"')
        source = asset.get('source', {})
        kind = source.get('kind')
        files = asset.get('files', [])
        if kind == 'url':
            if 'url' in source:
                https_url(source['url'], f'{name}: url')
            if 'archive' in source and 'url' not in source:
                raise AssetError(f'{name}: an archive needs a source url')
            if 'url' in source and 'archive' not in source and len(files) != 1:
                raise AssetError(f'{name}: a single url names one file, use one url per file for more')
            if 'url' not in source and 'archive' not in source:
                for entry in files:
                    https_url(entry.get('url', ''), f'{name}: url of {entry.get("to", "?")}')
        elif kind == 'git':
            https_url(source.get('repo', ''), f'{name}: repo')
            if not COMMIT.match(source.get('ref', '')):
                raise AssetError(f'{name}: ref must be a full commit SHA')
        else:
            raise AssetError(f'{name}: the shipped list allows only url and git sources, not {kind!r}')
        if not files:
            raise AssetError(f'{name}: no files')
        for entry in files:
            target = entry.get('to', '')
            if not RELATIVE.match(target):
                raise AssetError(f'{name}: unsafe destination {target!r}')
            if target in destinations:
                raise AssetError(f'{name}: destination {target} is used twice')
            destinations.add(target)
            if 'sha256' in entry and not SHA256.match(entry['sha256']):
                raise AssetError(f'{name}: malformed sha256 for {target}')
            needs_from = kind == 'git' or 'archive' in source
            if needs_from and not RELATIVE.match(entry.get('from', '')):
                raise AssetError(f'{name}: {target} needs a "from" path')
            if kind == 'url' and (not entry.get('sha256') or not entry.get('size_bytes')):
                raise AssetError(f'{name}: {target} needs sha256 and size_bytes')
            if 'url' in entry and (kind != 'url' or 'archive' in source or 'url' in source):
                raise AssetError(f'{name}: {target} may name its own url only in a url source with no source url')
        notice = asset.get('license', {}).get('notice')
        if notice not in {entry['to'] for entry in files}:
            raise AssetError(f'{name}: license notice {notice!r} is not one of the files')


def check_schema(document, list_path):
    try:
        import jsonschema
    except ImportError:
        return False
    schema_path = list_path.with_name('assets.schema.json')
    if not schema_path.is_file():
        return False
    schema = json.loads(schema_path.read_text(encoding='utf-8'))
    errors = sorted(jsonschema.Draft202012Validator(schema).iter_errors(document), key=lambda e: list(e.path))
    if errors:
        first = errors[0]
        raise AssetError(f'schema: {"/".join(map(str, first.path))}: {first.message}')
    return True


class HttpsOnly(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        https_url(newurl, 'redirect')
        return super().redirect_request(req, fp, code, msg, headers, newurl)


def download(url, limit):
    opener = urllib.request.build_opener(HttpsOnly)
    request = urllib.request.Request(url, headers={'User-Agent': 'MayaFlux-fetch-assets'})
    chunks = []
    total = 0
    try:
        with opener.open(request, timeout=60) as response:
            while True:
                block = response.read(CHUNK)
                if not block:
                    break
                total += len(block)
                if total > limit:
                    raise AssetError(f'download exceeds its declared maximum of {limit} bytes: {url}')
                chunks.append(block)
    except urllib.error.URLError as error:
        raise AssetError(f'could not fetch {url}: {error}') from error
    return b''.join(chunks)


def git(arguments, cwd, binary=False):
    environment = dict(os.environ, GIT_TERMINAL_PROMPT='0')
    try:
        done = subprocess.run(['git', *arguments], cwd=cwd, env=environment, capture_output=True, check=False)
    except FileNotFoundError as error:
        raise AssetError('git is required for git sources and was not found') from error
    if done.returncode != 0:
        raise AssetError(f'git {" ".join(arguments[:2])} failed: {done.stderr.decode(errors="replace").strip()}')
    return done.stdout


class GitSource:
    def __init__(self, repo, ref):
        self.repo = repo
        self.ref = ref
        self.directory = tempfile.TemporaryDirectory(prefix='mayaflux-assets-')
        self.opened = False

    def read(self, path):
        if not self.opened:
            work = self.directory.name
            git(['init', '-q'], work)
            git(['remote', 'add', 'origin', self.repo], work)
            try:
                git(['fetch', '-q', '--depth=1', '--filter=blob:none', 'origin', self.ref], work)
            except AssetError:
                git(['fetch', '-q', '--depth=1', 'origin', self.ref], work)
            self.opened = True
        return git(['cat-file', 'blob', f'{self.ref}:{path}'], self.directory.name)

    def close(self):
        self.directory.cleanup()


def no_links(path, stop):
    current = path
    while True:
        if current.is_symlink():
            raise AssetError(f'refusing to write through a symbolic link: {current}')
        if current == stop or current.parent == current:
            return
        current = current.parent


def digest(data):
    return hashlib.sha256(data).hexdigest()


def status(entry, destination):
    if not destination.exists():
        return 'missing'
    if not destination.is_file():
        raise AssetError(f'destination is not a file: {destination}')
    expected = entry.get('sha256')
    if expected is None:
        return 'present'
    return 'present' if digest(destination.read_bytes()) == expected.lower() else 'differs'


def place(data, destination, data_root):
    no_links(destination.parent, data_root.parent)
    destination.parent.mkdir(parents=True, exist_ok=True)
    handle, temporary = tempfile.mkstemp(dir=destination.parent, prefix='.asset-')
    try:
        with os.fdopen(handle, 'wb') as out:
            out.write(data)
        mask = os.umask(0)
        os.umask(mask)
        os.chmod(temporary, 0o666 & ~mask)
        os.replace(temporary, destination)
    except BaseException:
        if os.path.exists(temporary):
            os.unlink(temporary)
        raise


def verify(entry, data, name):
    if 'size_bytes' in entry and len(data) != entry['size_bytes']:
        raise AssetError(f'{name}: {entry["to"]} is {len(data)} bytes, expected {entry["size_bytes"]}')
    if 'sha256' in entry and digest(data) != entry['sha256'].lower():
        raise AssetError(f'{name}: {entry["to"]} does not match its sha256')


def fetch_url(asset, wanted, data_root, replace, report):
    source = asset['source']
    name = asset['id']
    if 'archive' not in source:
        for entry in wanted:
            data = download(entry.get('url', source.get('url')), entry['size_bytes'])
            verify(entry, data, name)
            place(data, data_root / entry['to'], data_root)
            report(f'fetched  {entry["to"]}')
        return
    limit = source['max_extracted_size_bytes']
    archive = download(source['url'], limit)
    temporary = tempfile.NamedTemporaryFile(delete=False)
    try:
        temporary.write(archive)
        temporary.close()
        with zipfile.ZipFile(temporary.name) as bundle:
            members = {info.filename: info for info in bundle.infolist()}
            if len(members) > source['max_files']:
                raise AssetError(f'{name}: archive holds more than {source["max_files"]} entries')
            if sum(info.file_size for info in members.values() if not info.is_dir()) > limit:
                raise AssetError(f'{name}: archive expands past {limit} bytes')
            for entry in wanted:
                info = members.get(entry['from'])
                if info is None or info.is_dir() or (info.external_attr >> 16) & 0o170000 == 0o120000:
                    raise AssetError(f'{name}: {entry["from"]} is not a regular file in the archive')
                data = bundle.read(info)
                verify(entry, data, name)
                place(data, data_root / entry['to'], data_root)
                report(f'fetched  {entry["to"]}')
    finally:
        os.unlink(temporary.name)


def fetch_git(asset, wanted, data_root, replace, report):
    source = asset['source']
    repository = GitSource(source['repo'], source['ref'])
    try:
        for entry in wanted:
            data = repository.read(entry['from'])
            verify(entry, data, asset['id'])
            place(data, data_root / entry['to'], data_root)
            report(f'fetched  {entry["to"]}')
    finally:
        repository.close()


def origins(asset):
    source = asset['source']
    if source['kind'] == 'git':
        urls = [source['repo']]
    elif 'url' in source:
        urls = [source['url']]
    else:
        urls = [entry['url'] for entry in asset['files']]
    return ', '.join(sorted({urllib.parse.urlsplit(url).hostname for url in urls}))


def apply(asset, data_root, check, replace, report):
    name = asset['id']
    platforms = asset.get('platforms')
    if platforms and PLATFORMS.get(sys.platform, sys.platform) not in platforms:
        report(f'{name}: not for this platform')
        return True
    license_info = asset['license']
    source = asset['source']
    report(f'{name}: {asset["description"]} ({license_info["spdx"]}, from {origins(asset)})')

    wanted = []
    complete = True
    for entry in asset['files']:
        destination = data_root / entry['to']
        no_links(destination, data_root.parent)
        state = status(entry, destination)
        if state == 'present':
            report(f'present  {entry["to"]}')
        elif state == 'differs' and not replace:
            report(f'differs  {entry["to"]} (kept; use --replace to overwrite)')
            complete = False
        else:
            wanted.append(entry)
            if check:
                report(f'missing  {entry["to"]}' if state == 'missing' else f'differs  {entry["to"]}')
                complete = False
    if check or not wanted:
        return complete
    try:
        if source['kind'] == 'git':
            fetch_git(asset, wanted, data_root, replace, report)
        else:
            fetch_url(asset, wanted, data_root, replace, report)
    except AssetError as error:
        report(f'failed   {error}')
        return False
    return complete


def main(arguments):
    parser = argparse.ArgumentParser(description='Fetch the assets listed in data/assets.json.')
    parser.add_argument('ids', nargs='*', help='asset ids to fetch, all when none are given')
    parser.add_argument('--list', type=Path, default=ROOT / 'data' / 'assets.json', help='path of the asset list')
    parser.add_argument('--data', type=Path, default=None, help='data directory to fill, data/ beside the list by default')
    parser.add_argument('--check', action='store_true', help='report what is present without using the network')
    parser.add_argument('--replace', action='store_true', help='overwrite files whose content differs')
    options = parser.parse_args(arguments)

    list_path = options.list.resolve()
    data_root = (options.data or list_path.parent).resolve()
    try:
        document = json.loads(list_path.read_text(encoding='utf-8-sig'))
        check_rules(document)
        validated = check_schema(document, list_path)
    except (OSError, json.JSONDecodeError, AssetError) as error:
        print(f'error: {error}', file=sys.stderr)
        return 1
    if not validated:
        print('note: jsonschema is not installed, the schema was not checked', file=sys.stderr)

    known = {asset['id'] for asset in document['assets']}
    unknown = [name for name in options.ids if name not in known]
    if unknown:
        print(f'error: unknown asset id(s): {", ".join(unknown)}', file=sys.stderr)
        return 1

    ok = True
    for asset in document['assets']:
        if options.ids and asset['id'] not in options.ids:
            continue
        try:
            ok = apply(asset, data_root, options.check, options.replace, print) and ok
        except AssetError as error:
            print(f'failed   {asset["id"]}: {error}')
            ok = False
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
