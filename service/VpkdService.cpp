#define LOG_TAG "VpkdService"
#include "service/VpkdService.h"
#include <array>

#include <binder/IResultReceiver.h>
#include <binder/IShellCallback.h>
#include <binder/TextOutput.h>
#include "sys/stat.h"


#define DEBUG 1

using namespace android;

namespace rc {

binder::Status VpkdService::hello(int32_t* _aidl_return) {
    hello(0);
    return binder::Status::ok();
}


status_t VpkdService::onTransact(uint32_t _aidl_code, const Parcel &_aidl_data, Parcel *_aidl_reply, uint32_t _aidl_flags) {
    switch (_aidl_code) {
        case IBinder::SHELL_COMMAND_TRANSACTION: {
            int in = _aidl_data.readFileDescriptor();
            int out = _aidl_data.readFileDescriptor();
            int err = _aidl_data.readFileDescriptor();
            int argc = _aidl_data.readInt32();
            std::vector<std::string> args;
            for (int i = 0; i < argc && _aidl_data.dataAvail() > 0; i++) {
                args.emplace_back(String8(_aidl_data.readString16()).string());
            }
            sp<IBinder> unusedCallback;
            sp<IResultReceiver> resultReceiver;
            status_t status;
            if ((status = _aidl_data.readNullableStrongBinder(&unusedCallback)) != OK)
                return status;
            if ((status = _aidl_data.readNullableStrongBinder(&resultReceiver)) != OK)
                return status;
            status = shellCommand(in, out, err, args);
            if (resultReceiver != nullptr) {
                resultReceiver->send(status);
            }
            return OK;
        }
    }
    return BnVPKD::onTransact(_aidl_code, _aidl_data, _aidl_reply, _aidl_flags);
}

status_t VpkdService::dump(int fd, const Vector<String16>& args) {
    dprintf(fd, "%s is happy!!!\n", getServiceName());
    return NO_ERROR;
}

status_t VpkdService::shellCommand(int in, int out, int err, std::vector<std::string>& args) {
    if (args.empty()) {
        dprintf(out, "%s is happy!!!\n", getServiceName());
        return NO_ERROR;
    }
    auto callingUid = IPCThreadState::self()->getCallingUid();
    if (callingUid != 0) {
        return PERMISSION_DENIED;
    }
    if (in == BAD_TYPE || out == BAD_TYPE || err == BAD_TYPE) {
        return BAD_VALUE;
    }
    if (args[0] == "hello") {
        return hello(out);
    }


    if (args[0] == "help") {
        printUsage(out);
        return NO_ERROR;
    }

    printUsage(err);
    return BAD_VALUE;
}

status_t VpkdService::hello(int out) {
    dprintf(out, "hello\n");
    return NO_ERROR;
}

status_t VpkdService::printUsage(int out) {
    return dprintf(out,
                   "%s commands:\n"
                   "  hello: test cmd\n"
                   "  help print this message\n", getServiceName());
}
}
