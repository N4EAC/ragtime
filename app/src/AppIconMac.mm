#import <AppKit/AppKit.h>

bool startupMotionReduced()
{
    return NSWorkspace.sharedWorkspace.accessibilityDisplayShouldReduceMotion;
}

void setDockIcon()
{
    NSBundle* bundle = NSBundle.mainBundle;
    NSString* file = [bundle objectForInfoDictionaryKey:@"CFBundleIconFile"];
    NSURL* url = [bundle URLForResource:file.stringByDeletingPathExtension withExtension:@"icns"];
    if (!url) return;

    NSImage* icon = [[NSImage alloc] initWithContentsOfURL:url];
    if (icon)
        NSApp.applicationIconImage = icon;
    [icon release];
}
