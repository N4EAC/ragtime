#include "MicrophonePermission.h"

#import <AVFoundation/AVFoundation.h>

namespace ragtime { namespace audio { namespace macos {

MicrophonePermission microphonePermission()
{
    switch ([AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio])
    {
    case AVAuthorizationStatusAuthorized: return MicrophonePermission::Authorized;
    case AVAuthorizationStatusNotDetermined: return MicrophonePermission::NotDetermined;
    default: return MicrophonePermission::Denied;
    }
}

bool requestMicrophonePermission(std::function<void()> completed)
{
    NSString* usage = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"NSMicrophoneUsageDescription"];
    if (![usage isKindOfClass:[NSString class]] || ![usage length]) return false;
    [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio completionHandler:^(BOOL) {
        completed();
    }];
    return true;
}

} } }
