#include "bongo/platform.h"

#ifdef __APPLE__
#import <Cocoa/Cocoa.h>
#import <Security/Security.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static bool remove_path(NSFileManager *files, NSString *path) {
    return ![files fileExistsAtPath:path] || [files removeItemAtPath:path error:nil];
}

static bool extract(NSString *archive, NSString *directory) {
    NSTask *task = [[NSTask alloc] init];
    [task setLaunchPath:@"/usr/bin/ditto"];
    [task setArguments:@[@"-x", @"-k", archive, directory]];
    @try { [task launch]; [task waitUntilExit]; }
    @catch (NSException *exception) { (void)exception; [task release]; return false; }
    bool ok = [task terminationStatus] == 0; [task release]; return ok;
}

static NSString *find_app(NSFileManager *files, NSString *directory) {
    NSArray *items = [files contentsOfDirectoryAtPath:directory error:nil];
    for (NSString *item in items)
        if ([[item pathExtension] caseInsensitiveCompare:@"app"] == NSOrderedSame)
            return [directory stringByAppendingPathComponent:item];
    return nil;
}

static SecStaticCodeRef static_code(NSString *path) {
    SecStaticCodeRef code = NULL;
    NSURL *url = [NSURL fileURLWithPath:path];
    return SecStaticCodeCreateWithPath((CFURLRef)url,
        kSecCSDefaultFlags, &code) == errSecSuccess ? code : NULL;
}

static bool same_signing_identity(NSString *candidate_path, NSString *current_path) {
    SecStaticCodeRef candidate = static_code(candidate_path);
    SecStaticCodeRef current = static_code(current_path);
    SecRequirementRef requirement = NULL;
    OSStatus current_status = current ? SecStaticCodeCheckValidity(current,
        kSecCSCheckAllArchitectures, NULL) : errSecCSUnsigned;
    OSStatus requirement_status = current_status == errSecSuccess
        ? SecCodeCopyDesignatedRequirement(current, kSecCSDefaultFlags, &requirement)
        : current_status;
    OSStatus candidate_status = candidate && requirement_status == errSecSuccess
        ? SecStaticCodeCheckValidity(candidate, kSecCSCheckAllArchitectures, requirement)
        : requirement_status;
    if (requirement) CFRelease(requirement);
    if (current) CFRelease(current);
    if (candidate) CFRelease(candidate);
    return candidate_status == errSecSuccess;
}

static int apply_update(char **argv) {
    @autoreleasepool {
        NSString *staged = [NSString stringWithUTF8String:argv[2]];
        NSString *bundle = [NSString stringWithUTF8String:argv[3]];
        pid_t parent = (pid_t)strtol(argv[4], NULL, 10);
        for (int i = 0; i < 600 && kill(parent, 0) == 0; ++i) usleep(100000);
        NSFileManager *files = [NSFileManager defaultManager];
        NSString *unpack = [bundle stringByAppendingString:@".new"];
        NSString *backup = [bundle stringByAppendingString:@".old"];
        if (!remove_path(files, unpack) || !remove_path(files, backup) ||
            ![files createDirectoryAtPath:unpack withIntermediateDirectories:YES
                attributes:nil error:nil] || !extract(staged, unpack)) return 1;
        NSString *candidate = find_app(files, unpack);
        if (!candidate || !same_signing_identity(candidate, bundle)) {
            remove_path(files, unpack); return 1;
        }
        if (![files moveItemAtPath:bundle toPath:backup error:nil]) return 1;
        if (![files moveItemAtPath:candidate toPath:bundle error:nil]) {
            [files moveItemAtPath:backup toPath:bundle error:nil]; return 1;
        }
        remove_path(files, unpack); remove_path(files, staged);
        NSString *name = [[[NSBundle bundleWithPath:bundle] executablePath] copy];
        if (!name) { [files moveItemAtPath:bundle toPath:unpack error:nil];
            [files moveItemAtPath:backup toPath:bundle error:nil]; return 1; }
        execl([name fileSystemRepresentation], [name fileSystemRepresentation],
            "--bongo-cleanup-old", [backup fileSystemRepresentation], NULL);
        [name release];
        NSString *failed = [bundle stringByAppendingString:@".failed"];
        remove_path(files, failed);
        if (![files moveItemAtPath:bundle toPath:failed error:nil])
            remove_path(files, bundle);
        [files moveItemAtPath:backup toPath:bundle error:nil];
        remove_path(files, failed); return 1;
    }
}

bool bongo_platform_schedule_update(const char *staged, BongoError *error) {
    @autoreleasepool {
        if (!staged) return false;
        NSString *executable = [[NSBundle mainBundle] executablePath];
        NSString *bundle = [[NSBundle mainBundle] bundlePath];
        if (!executable || !bundle) {
            bongo_error_set(error, BONGO_ERROR_PLATFORM, "Cannot locate macOS app bundle");
            return false;
        }
        char parent[32]; snprintf(parent, sizeof(parent), "%ld", (long)getpid());
        pid_t child = fork();
        if (child == 0) {
            execl([executable fileSystemRepresentation], [executable fileSystemRepresentation],
                "--bongo-apply-update", staged, [bundle fileSystemRepresentation], parent, NULL);
            _exit(127);
        }
        if (child > 0) return true;
        bongo_error_set(error, BONGO_ERROR_PLATFORM, "Cannot launch macOS update helper");
        return false;
    }
}

bool bongo_platform_verify_update(const char *path, const char *version,
    const char *platform, const char *sha256, uint64_t size,
    const char *signature, BongoError *error) {
    (void)path; (void)version; (void)platform; (void)sha256;
    (void)size; (void)signature; (void)error; return true;
}

int bongo_platform_update_helper(int argc, char **argv) {
    if (argc == 5 && strcmp(argv[1], "--bongo-apply-update") == 0)
        return apply_update(argv);
    if (argc == 3 && strcmp(argv[1], "--bongo-cleanup-old") == 0) {
        @autoreleasepool {
            NSString *path = [NSString stringWithUTF8String:argv[2]];
            remove_path([NSFileManager defaultManager], path);
        }
        return -1;
    }
    return -1;
}
#endif
