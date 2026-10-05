#!/usr/bin/env python3
"""Generate bundle metadata, localizations, and original raster icons."""
import json
from pathlib import Path
import plistlib
import struct
import zlib

ROOT = Path(__file__).resolve().parents[1]
R = ROOT / 'Resources'
VERSION = '0.1.0'
base = dict(CFBundleDevelopmentRegion='en', CFBundleVersion=VERSION, CFBundleShortVersionString=VERSION)
app = dict(base, CFBundleIdentifier='com.dcmmc.basebanddisabler', CFBundleExecutable='BasebandDisabler', CFBundleName='Baseband Disabler', CFBundleDisplayName='Baseband Disabler', CFBundlePackageType='APPL', LSRequiresIPhoneOS=True, MinimumOSVersion='16.0', UIDeviceFamily=[1,2], UILaunchScreen={}, UISupportedInterfaceOrientations=['UIInterfaceOrientationPortrait','UIInterfaceOrientationLandscapeLeft','UIInterfaceOrientationLandscapeRight'], **{'UISupportedInterfaceOrientations~ipad':['UIInterfaceOrientationPortrait','UIInterfaceOrientationPortraitUpsideDown','UIInterfaceOrientationLandscapeLeft','UIInterfaceOrientationLandscapeRight']}, CFBundleIcons={'CFBundlePrimaryIcon':{'CFBundleIconFiles':['Icon120','Icon180']}}, **{'CFBundleIcons~ipad':{'CFBundlePrimaryIcon':{'CFBundleIconFiles':['Icon152','Icon167']}}})
prefs = dict(base, CFBundleIdentifier='com.dcmmc.basebanddisabler.preferences', CFBundleExecutable='BasebandDisablerPrefs', CFBundleName='BasebandDisablerPrefs', CFBundlePackageType='BNDL', NSPrincipalClass='BDBPreferencesController')
entry = {'entry':{'cell':'PSLinkCell','label':'Baseband Disabler','id':'BasebandDisabler','detail':'BDBPreferencesController','bundle':'BasebandDisablerPrefs','icon':'Icon60.png','isController':True}}
for name, value in [('AppInfo.plist',app),('PrefsInfo.plist',prefs),('PreferenceLoader.plist',entry),('app.entitlements',{'com.apple.private.security.no-container':True,'com.apple.private.security.no-sandbox':True})]:
    (R/name).write_bytes(plistlib.dumps(value))

en = dict(checking='Working…', unavailable='Status unavailable', unsupported='Device not supported', off='Baseband is off', on='Faulty baseband detected', healthy='No baseband fault detected', compatibility='Verified support: iPad13,6 on iPadOS 16.3.1 (20D67), with Dopamine.', off_detail='The modem power is off. Cellular service remains unavailable.', on_detail='Turn off the faulty modem to reduce repeated wakeups and standby drain.', healthy_detail='This tool only disables a modem that the driver reports as missing.', failed='Unable to complete', ok='OK', completed='Operation completed', control='BASEBAND', automatic='STARTUP', about='DEVICE', control_footer='Disabling turns off cellular service. Restoring returns the original power settings and turns off automatic disabling.', auto_footer='Runs once after jailbreaking. Turning this switch off leaves the current modem power unchanged. A normal boot without jailbreak does not apply the setting.', about_footer='Baseband Disabler 0.1.0 · Software workaround for a faulty modem. Hardware is not repaired.', restore='Restore original settings', disable='Disable faulty baseband', auto='Disable after jailbreaking', device='Device and system build', copy='Copy diagnostics', copied='Diagnostics copied')
zh = dict(checking='正在处理…', unavailable='无法读取状态', unsupported='暂不支持此设备', off='基带已关闭', on='已检测到故障基带', healthy='未检测到基带故障', compatibility='已验证：iPad13,6、iPadOS 16.3.1（20D67），需要 Dopamine 越狱。', off_detail='基带电源已关闭，蜂窝网络保持停用。', on_detail='关闭故障基带，减少重复唤醒和待机耗电。', healthy_detail='只有系统驱动报告基带缺失时，才允许屏蔽。', failed='操作未完成', ok='确定', completed='操作完成', control='基带控制', automatic='自动启动', about='设备信息', control_footer='屏蔽会停用蜂窝网络。恢复会还原原来的供电设置，并关闭自动屏蔽。', auto_footer='重新越狱后执行一次。关闭此开关不会改变当前基带供电。未越狱的正常启动不会应用屏蔽。', about_footer='Baseband Disabler 0.1.0 · 用于降低故障基带耗电，硬件故障仍需维修。', restore='恢复原来的设置', disable='屏蔽故障基带', auto='越狱后自动屏蔽', device='设备型号与系统版本', copy='复制诊断信息', copied='诊断信息已复制')
errors = {
 'root_required':('The privileged helper is not installed correctly. Reinstall the package.','权限工具安装不完整，请重新安装插件。'),
 'helper_unavailable':('The helper could not be started. Check that Dopamine is active.','无法启动权限工具，请确认 Dopamine 越狱仍在生效。'),
 'dopamine_required':('Dopamine kernel access is unavailable. Jailbreak again and refresh.','Dopamine 内核权限不可用，请重新越狱后刷新。'),
 'unsupported_device':('This device or system build has not been verified. No power settings were changed.','此设备或系统版本尚未验证，供电设置未改变。'),
 'unsupported_layout':('The driver does not match the verified device profile.','基带驱动与已验证的设备配置不匹配。'),
 'unsafe_state':('The modem is not in the verified fault state. The operation was refused.','基带不处于已验证的故障状态，操作已停止。'),
 'snapshot_missing':('The original settings are unavailable for this boot. Reboot and jailbreak to recover the default power state.','缺少本次启动的恢复记录。重新启动并越狱可恢复默认供电状态。'),
 'restore_failed':('Restoration could not be verified. Reboot to recover the default hardware state.','无法验证恢复结果，请重新启动以恢复硬件默认状态。'),
 'busy':('Another operation is running. Please refresh shortly.','另一项操作正在进行，请稍后刷新。'),
 'storage_failed':('The original settings or configuration could not be saved.','无法保存恢复记录或配置。'),
 'operation_failed':('The operation did not succeed. Copy diagnostics to investigate.','操作未成功，可复制诊断信息进行排查。'),
 'verification_failed':('The requested power state could not be verified.','无法验证目标供电状态。'),
 'driver_unavailable':('The baseband driver is unavailable.','基带驱动不可用。'),
 'invalid_command':('Invalid helper command.','权限工具命令无效。')}
for key,(a,b) in errors.items():en['error.'+key]=a;zh['error.'+key]=b
for locale, values in [('en',en),('zh-Hans',zh)]:
    (R/(locale+'.lproj')).mkdir(exist_ok=True)
    (R/(locale+'.lproj')/'Localizable.strings').write_text('\n'.join(json.dumps(k,ensure_ascii=False)+' = '+json.dumps(v,ensure_ascii=False)+';' for k,v in sorted(values.items()))+'\n')

def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
def icon(size):
    rows=[]
    for y in range(size):
        row=bytearray([0])
        for x in range(size):
            u=(x+.5)/size;v=(y+.5)/size
            # Blue field, a white power glyph, and a small green status dot.
            r,g,b=round(20+12*v),round(106-28*v),round(238-24*v)
            radius=((u-.5)**2+(v-.51)**2)**.5
            ring=.225<radius<.29 and not (v<.37 and abs(u-.5)<.09)
            stem=abs(u-.5)<.03 and .22<v<.5
            dot=(u-.78)**2+(v-.24)**2<.046**2
            if ring or stem:r,g,b=255,255,255
            if dot:r,g,b=70,225,155
            row.extend((r,g,b,255))
        rows.append(bytes(row))
    header=struct.pack('>IIBBBBB',size,size,8,6,0,0,0)
    return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',header)+chunk(b'IDAT',zlib.compress(b''.join(rows),9))+chunk(b'IEND',b'')
for size in (60,120,152,167,180):(R/('Icon%d.png'%size)).write_bytes(icon(size))
