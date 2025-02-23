#include "videoplayeradvancedbase.h"

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::VideoPlayerAdvancedBase)
        RTTI_PROPERTY("NumThreads", &nap::VideoPlayerAdvancedBase::mNumThreads, nap::rtti::EPropertyMetaData::Default, "Number of threads to use for decoding. 0 means automatic.")
RTTI_END_CLASS

namespace nap
{
    VideoPlayerAdvancedBase::VideoPlayerAdvancedBase(VideoAdvancedService& service) :
            mService(service)
    { }

}