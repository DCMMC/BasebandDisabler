#!/usr/bin/env python3
"""Inspect the actual distributable, including privileged file boundaries."""
import io
import json
from pathlib import Path
import plistlib
import subprocess
import tarfile
import tempfile
import re
import unittest

ROOT=Path(__file__).resolve().parents[1]
DEB=ROOT/'dist/com.dcmmc.basebanddisabler_0.1.0_iphoneos-arm64.deb'
class PackageContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tar=tarfile.open(fileobj=io.BytesIO(subprocess.check_output(['dpkg-deb','--fsys-tarfile',str(DEB)])),mode='r:*')
        cls.files={m.name.removeprefix('./'):m for m in cls.tar.getmembers()}
    def read(self,path):return self.tar.extractfile(self.files[path]).read()
    def test_only_rootless_payload(self):
        for name,member in self.files.items():
            if member.isfile():self.assertTrue(name.startswith('var/jb/'),name)
            self.assertEqual(member.uid,0);self.assertEqual(member.gid,0)
    def test_root_helper_and_no_other_setuid_files(self):
        privileged=[name for name,m in self.files.items() if m.mode&0o4000]
        self.assertEqual(privileged,['var/jb/usr/libexec/basebanddisabler/basebandctl'])
        self.assertEqual(self.files[privileged[0]].mode,0o4755)
        self.assertEqual(self.read(privileged[0])[:4],b'\xcf\xfa\xed\xfe')
    def test_no_private_runtime_state_in_distribution(self):
        for name in self.files:
            self.assertNotIn('original-state.json',name);self.assertNotIn('config.json',name)
            self.assertNotIn('kernelcache',name);self.assertNotIn('.log',name)
    def test_once_per_jailbreak_no_periodic_polling(self):
        p=plistlib.loads(self.read('var/jb/Library/LaunchDaemons/com.dcmmc.basebanddisabler.plist'))
        self.assertTrue(p['RunAtLoad']);self.assertFalse(p['KeepAlive'])
        self.assertEqual(p['UserName'],'root');self.assertNotIn('StartInterval',p)
    def test_app_and_settings_entry_match(self):
        app=plistlib.loads(self.read('var/jb/Applications/BasebandDisabler.app/Info.plist'))
        prefs=plistlib.loads(self.read('var/jb/Library/PreferenceBundles/BasebandDisablerPrefs.bundle/Info.plist'))
        entry=plistlib.loads(self.read('var/jb/Library/PreferenceLoader/Preferences/BasebandDisabler.plist'))['entry']
        self.assertEqual(app['CFBundleIdentifier'],'com.dcmmc.basebanddisabler')
        self.assertEqual(entry['detail'],prefs['NSPrincipalClass'])
        self.assertEqual(entry['bundle'],prefs['CFBundleExecutable'])
        for root in ['var/jb/Applications/BasebandDisabler.app','var/jb/Library/PreferenceBundles/BasebandDisablerPrefs.bundle']:
            for locale in ('en','zh-Hans'):self.assertIn(root+'/'+locale+'.lproj/Localizable.strings',self.files)
    def test_trustcache_matches_built_payload(self):
        hashes=self.read('var/jb/usr/libexec/basebanddisabler/trustcache.list').decode().splitlines()
        self.assertEqual(len(hashes),3);self.assertEqual(len(set(hashes)),3)
        self.assertTrue(all(len(h)==40 and all(c in '0123456789abcdef' for c in h) for h in hashes))
        paths=['var/jb/usr/libexec/basebanddisabler/basebandctl','var/jb/Applications/BasebandDisabler.app/BasebandDisabler','var/jb/Library/PreferenceBundles/BasebandDisablerPrefs.bundle/BasebandDisablerPrefs']
        with tempfile.TemporaryDirectory(dir=ROOT/'build') as directory:
            for expected,path in zip(hashes,paths):
                binary=Path(directory)/Path(path).name;binary.write_bytes(self.read(path))
                output=subprocess.check_output(['ldid','-h',str(binary)],text=True)
                self.assertEqual(re.search(r'^CDHash=([0-9a-f]{40})$',output,re.M)[1],expected)
    def test_maintainer_scripts_parse(self):
        shell='/var/jb/bin/sh' if Path('/var/jb/bin/sh').exists() else '/bin/sh'
        for path in (ROOT/'packaging').glob('*'):
            if path.name in ('postinst','prerm','startup.sh'):subprocess.run([shell,'-n',str(path)],check=True)
if __name__=='__main__':unittest.main(verbosity=2)
