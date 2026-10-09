#!/usr/bin/env python3
import os
from pathlib import Path
import re
import shutil
import subprocess
ROOT=Path(__file__).resolve().parents[1]
stage=ROOT/'build/package'
if stage.exists():shutil.rmtree(stage)
def copy(source,target,mode=0o644):
    path=stage/target;path.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(ROOT/source,path);path.chmod(mode);return path
def sign(path,entitlements):
    for _ in range(2):subprocess.run(['ldid','-S'+str(ROOT/entitlements),str(path)],check=True)
    out=subprocess.check_output(['ldid','-h',str(path)],text=True)
    return re.search(r'^CDHash=([0-9a-f]{40})$',out,re.M)[1]
helper=copy('build/basebandctl','var/jb/usr/libexec/basebanddisabler/basebandctl',0o4755)
app=copy('build/BasebandDisabler','var/jb/Applications/BasebandDisabler.app/BasebandDisabler',0o755)
hashes=[sign(helper,'Resources/helper.entitlements'),sign(app,'Resources/app.entitlements')]
for directory,info in [('var/jb/Applications/BasebandDisabler.app','AppInfo.plist')]:
    copy('Resources/'+info,directory+'/Info.plist')
    for image in (ROOT/'Resources').glob('Icon*.png'):copy(image.relative_to(ROOT),directory+'/'+image.name)
    for locale in ('en','zh-Hans'):copy('Resources/'+locale+'.lproj/Localizable.strings',directory+'/'+locale+'.lproj/Localizable.strings')
copy('packaging/startup.sh','var/jb/usr/libexec/basebanddisabler/startup.sh',0o755)
copy('packaging/launch.plist','var/jb/Library/LaunchDaemons/com.dcmmc.basebanddisabler.plist')
(stage/'var/jb/usr/libexec/basebanddisabler/trustcache.list').write_text('\n'.join(hashes)+'\n')
for name in ('control','postinst','prerm'):copy('packaging/'+name,'DEBIAN/'+name,0o644 if name=='control' else 0o755)
for directory,dirs,files in os.walk(stage):os.chmod(directory,0o755)
(ROOT/'dist').mkdir(exist_ok=True)
deb=ROOT/'dist/com.dcmmc.basebanddisabler_0.1.2_iphoneos-arm64.deb'
subprocess.run(['dpkg-deb','--root-owner-group','-Zxz','--build',str(stage),str(deb)],check=True)
print(deb)
