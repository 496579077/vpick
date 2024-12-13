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
    android::status_t init(){ mOnekeyNewDeviceThread.run("onekey", 0); return android::NO_ERROR;}

private:
    android::status_t hello(int out);
    android::status_t printUsage(int out);
    android::status_t onBrandChanged(int out);
    android::status_t onModelChanged(int out);

class OnekeyNewDeviceThread : public android::Thread {
    public:
        OnekeyNewDeviceThread();
        virtual ~OnekeyNewDeviceThread() {}
        void onBrandChanged();
        void onModelChanged();
    private:
        void maybeUpdateDeviceInfo();
        bool threadLoop() final;
        std::mutex mStateMutex;
        std::string mBrand;
        std::string mModel;
        bool mBrandChanged;
        bool mModelChanged;
        bool mEnabled;
        bool mVpickBusy;
};
    OnekeyNewDeviceThread mOnekeyNewDeviceThread;

};
}
#endif
