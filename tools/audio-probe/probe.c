/* audio-probe: what the real CoreAudio says about this machine, through the public HAL API (CoreAudio.framework: AudioObject*).  Run on a Mac; the output is the reference that our coreaudiod has to reproduce
 * (property by property: selector, scope, element, size, value).  No sound is played, nothing is changed.
 *   cc -o audio-probe probe.c -framework CoreAudio -framework CoreFoundation -framework AudioToolbox && ./audio-probe > out.txt */
#include <CoreAudio/CoreAudio.h>
#include <AudioToolbox/AudioServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void four(UInt32 v, char *o) { o[0] = (v >> 24) & 255; o[1] = (v >> 16) & 255; o[2] = (v >> 8) & 255; o[3] = v & 255; o[4] = 0; for (int i = 0; i < 4; i++) if (o[i] < 32 || o[i] > 126) o[i] = '?'; }
static void cfs(CFStringRef s, char *o, size_t n) { if (!s) { snprintf(o, n, "(null)"); return; } if (!CFStringGetCString(s, o, (CFIndex)n, kCFStringEncodingUTF8)) snprintf(o, n, "(?)"); }

/* every property of an object: selector, scope, element, size */
static void dump(AudioObjectID obj, const char *tag)
{
	UInt32 sz = 0; AudioObjectPropertyAddress a = { kAudioObjectPropertyOwnedObjects, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
	char cls[5], sel[5];
	AudioClassID klass = 0; UInt32 s4 = sizeof klass; AudioObjectPropertyAddress ac = { kAudioObjectPropertyClass, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
	if (AudioObjectGetPropertyData(obj, &ac, 0, NULL, &s4, &klass) == 0) four(klass, cls); else strcpy(cls, "----");
	printf("%s object %u class '%s'\n", tag, (unsigned)obj, cls);
	/* the selectors that the headers name (a table of the common ones; the scopes 'glob' 'inpt' 'outp' 'ptru'): size, and for small plain ones the value */
	static const UInt32 sels[] = { 'oclg', 'stdv', 'cdsc', 'oown', 'lnam', 'lmak', 'lcnm', 'luid', 'tran', 'dev#', 'dOut', 'dIn ', 'sOut', 'plgn', 'aggr', 'bxnm', 'ownd', 'clas', 'bcls', 'name', 'manu', 'uid ',
		'mcod', 'nsrt', 'nsr#', 'rate', 'lat ', 'safe', 'stm#', 'stmz', 'strm', 'vmvc', 'volm', 'mute', 'bsiz', 'fsiz', 'bsz#', 'acct', 'jack', 'dflt', 'sflt', 'pdvs', 'pad ', 'ctrl', 'sdsr', 'dsnm', 'dsid', 'dsrc', 'dsn#', 'chan',
		'ldev', 'agrp', 'asct', 'hogm', 'ioct', 'dtyp', 'tdev', 'tlat', 'vcod', 'vpan', 'pcm ', 'lpcm', 'ssuf', 'srnd', 'iscl', 'idnt', 'ifmt', 'dpid', 'pnam', 'rngb' };
	static const UInt32 scopes[] = { kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyScopeInput, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyScopePlayThrough };
	static const char *sn[] = { "glob", "inpt", "outp", "ptru" };
	for (size_t i = 0; i < sizeof sels / sizeof *sels; i++) for (int sc = 0; sc < 4; sc++) for (UInt32 el = 0; el < 3; el++) {
		a.mSelector = sels[i]; a.mScope = scopes[sc]; a.mElement = el;
		if (!AudioObjectHasProperty(obj, &a)) continue;
		sz = 0; OSStatus st = AudioObjectGetPropertyDataSize(obj, &a, 0, NULL, &sz); four(sels[i], sel);
		printf("  '%s' %s el %u: size %u", sel, sn[sc], (unsigned)el, (unsigned)sz); if (st) { printf(" (status %d)", (int)st); }
		if (!st && sz && sz <= 4096) {
			unsigned char buf[4096]; UInt32 got = sz; AudioObjectPropertyAddress q = a; Boolean settable = 0; AudioObjectIsPropertySettable(obj, &q, &settable);
			if (AudioObjectGetPropertyData(obj, &q, 0, NULL, &got, buf) == 0) {
				printf(" settable %d value", (int)settable);
				if (sz == sizeof(CFStringRef) && (sels[i] == 'lnam' || sels[i] == 'lmak' || sels[i] == 'lcnm' || sels[i] == 'luid' || sels[i] == 'name' || sels[i] == 'manu' || sels[i] == 'uid ' || sels[i] == 'bxnm' || sels[i] == 'dsnm' || sels[i] == 'pnam')) { char t[256]; cfs(*(CFStringRef *)buf, t, sizeof t); printf(" \"%s\"", t); }
				else for (UInt32 b = 0; b < got && b < 64; b++) printf(" %02x", buf[b]);
			}
		}
		printf("\n");
	}
}
int main(void)
{
	printf("== AudioObjectShow(system)\n"); fflush(stdout); AudioObjectShow(kAudioObjectSystemObject); fflush(stdout);
	printf("== system object\n"); dump(kAudioObjectSystemObject, "system");
	AudioObjectPropertyAddress a = { kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain }; UInt32 sz = 0;
	if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &a, 0, NULL, &sz) == 0 && sz) {
		AudioDeviceID *ids = malloc(sz); AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, 0, NULL, &sz, ids);
		for (UInt32 i = 0; i < sz / sizeof *ids; i++) {
			printf("== device %u\n", (unsigned)ids[i]); fflush(stdout); AudioObjectShow(ids[i]); fflush(stdout); dump(ids[i], "device");
			AudioObjectPropertyAddress s = { kAudioDevicePropertyStreams, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain }; UInt32 ss = 0;
			if (AudioObjectGetPropertyDataSize(ids[i], &s, 0, NULL, &ss) == 0 && ss) { AudioObjectID *st = malloc(ss); AudioObjectGetPropertyData(ids[i], &s, 0, NULL, &ss, st); for (UInt32 k = 0; k < ss / sizeof *st; k++) { printf("== stream %u\n", (unsigned)st[k]); fflush(stdout); AudioObjectShow(st[k]); fflush(stdout); dump(st[k], "stream"); } free(st); }
			Float32 v = -1; UInt32 vs = sizeof v; AudioObjectPropertyAddress vm = { 'vmvc', kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMain }; OSStatus r = AudioHardwareServiceGetPropertyData(ids[i], &vm, 0, NULL, &vs, &v);
			printf("  AudioHardwareService 'vmvc' outp: status %d value %f\n", (int)r, v);
		}
		free(ids);
	} else printf("no devices (size %u)\n", (unsigned)sz);
	return 0;
}
