#ifndef RCD_SERVICE_H_
#define RCD_SERVICE_H_

#include "binder/BinderService.h"
#include "utils/String16.h"
#include "utils/Vector.h"
#include "com/android/vpkd/BnVPKD.h"

namespace vpk {

using namespace com::android;

class VpkdService : public android::BinderService<vpkd::BnVPKD>, public vpkd::BnVPKD {
public:
    static char const* getServiceName() { return "vpkd"; }
    android::status_t onTransact(uint32_t _aidl_code, const ::android::Parcel &_aidl_data,
                                 ::android::Parcel *_aidl_reply, uint32_t _aidl_flags) override;
    android::status_t dump(int fd, const android::Vector<android::String16>& args) override;
    android::status_t shellCommand(int in, int out, int err, std::vector<std::string>& args);
    android::binder::Status hello(int32_t* _aidl_return) override;
    
private:
    android::status_t hello(int out);
    android::status_t printUsage(int out);
    android::status_t onBrandChanged(int out);
    android::status_t onModelChanged(int out);
};
}
#endif
