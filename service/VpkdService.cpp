#define LOG_TAG "VpkdService"
#include "service/VpkdService.h"
#include <array>
#include <log/log.h>
#include <android-base/logging.h>
#include <binder/IResultReceiver.h>
#include <binder/IShellCallback.h>
#include <binder/TextOutput.h>
#include <cutils/properties.h>
#include "sys/stat.h"


#define DEBUG 1

using namespace android;

namespace vpk {

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

    if (args[0] == "brand-changed") {
        return onBrandChanged(out);
    }

    if (args[0] == "model-changed") {
        return onModelChanged(out);
    }

    if (args[0] == "setprop") {
        if (args.size() < 3) {
            dprintf(err, "setprop requires two arguments: key and value\n");
            return BAD_VALUE;
        }
        return setprop(out, args[1], args[2]);
    }

    if (args[0] == "getprop") {
        if (args.size() < 2) {
            dprintf(err, "getprop requires one argument: key\n");
            return BAD_VALUE;
        }
        return getprop(out, args[1]);
    }

    printUsage(err);
    return BAD_VALUE;
}

status_t VpkdService::hello(int out) {
    dprintf(out, "hi\n");
    return NO_ERROR;
}

status_t VpkdService::printUsage(int out) {
    return dprintf(out,
                   "%s commands:\n"
                   "  hello: test cmd\n"
                   "  help print this message\n", getServiceName());
}

status_t VpkdService::onBrandChanged(int out) {
    mOnekeyNewDeviceThread.onBrandChanged();
    return NO_ERROR;
}

status_t VpkdService::onModelChanged(int out) {
    mOnekeyNewDeviceThread.onModelChanged();
    return NO_ERROR;
}

status_t VpkdService::setprop(int out, std::string &key, std::string &value) {
    std::string gifKey = key.c_str();
	if (gifKey.find("ro.") == 0) {
		gifKey = "gif." + gifKey;
	}
    //ALOGD("setSystemProperties key = %s , value = %s", gifKey.c_str(), value.c_str());
	ssize_t result = property_set(gifKey.c_str(), value.c_str());
	if (result < 0) {
		return -1;
	}
    return NO_ERROR;
}

status_t VpkdService::getprop(int out, std::string &key) {
    char prop_value[PROPERTY_VALUE_MAX];
    property_get(key.c_str(), prop_value, "");
    dprintf(out, "%s\n", prop_value);
    return NO_ERROR;
}

////////OnekeyNewDeviceThread

std::string execute_command(const std::string &command, bool remove_spaces = false) {
    FILE *fp = popen(command.c_str(), "r");
    if (!fp) {
        return "";
    }

    std::string result;
    char buffer[128];
    while (fgets(buffer, sizeof(buffer), fp) != nullptr) {
        result += buffer;
    }
    fclose(fp);

    if (remove_spaces) {
        result.erase(std::remove_if(result.begin(), result.end(), [](char ch) {
            return ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t';
        }), result.end());
    }
    return result;
}

VpkdService::VpkdService::OnekeyNewDeviceThread::OnekeyNewDeviceThread()
            :mBrand("none"),
                mModel("none"),
                mTriggerForce(false),
                mBrandChanged(false),
                mModelChanged(false),
                mEnabled(true),
                mVpickBusy(false){
    mBrand = execute_command("getprop ro.product.brand", true);
    mModel = execute_command("getprop ro.product.model", true);
    ALOGE("mBrand = %s", mBrand.c_str());
    ALOGE("mModel = %s", mModel.c_str());
}

void VpkdService::VpkdService::OnekeyNewDeviceThread::onBrandChanged() {
    ALOGI("%s", __FUNCTION__);
    std::string brand = execute_command("getprop ro.product.brand", true);
    if (brand != mBrand) {
        ALOGE("Brand changed: %s", brand.c_str());
        mBrandChanged = true;
    }
};

void VpkdService::VpkdService::OnekeyNewDeviceThread::onModelChanged() {
    ALOGI("%s", __FUNCTION__);
    std::string model = execute_command("getprop ro.product.model", true);
    if (model != mModel) {
        ALOGE("Model changed: %s", model.c_str());
        mModelChanged = true;
    }
};

void VpkdService::VpkdService::OnekeyNewDeviceThread::maybeUpdateDeviceInfo() {
    ALOGI("%s", __FUNCTION__);

    if (mVpickBusy) {
        ALOGE("Device info update skipped: another update in progress.");
        return;
    }

    mVpickBusy = true;
    // execute_command("setprop sys.vpk.busy 1");

    // Fetch new values only once
    std::string newBrand = execute_command("getprop ro.product.brand", true);
    std::string newModel = execute_command("getprop ro.product.model", true);

    if (mBrandChanged) {
        ALOGI("Updating brand info: %s -> %s", mBrand.c_str(), newBrand.c_str());
        mBrand = newBrand;
        mBrandChanged = false;
    }

    if (mModelChanged) {
        ALOGI("Updating model info: %s -> %s", mModel.c_str(), newModel.c_str());
        mModel = newModel;
        mModelChanged = false;
    }
    
    std::string cammand = "";
    std::string ret = "";
    cammand = "vpick -r --brand " + mBrand  + " --model " + mModel + " --setprop-cmd \"gifsetprop\"";
    ALOGI("%s", cammand.c_str());
    ret= execute_command(cammand, false);
    ALOGI("finished: %s", ret.c_str());

    //
    cammand = "vpick -r --brand " + mBrand  + " --model " + mModel;
    ALOGI("%s", cammand.c_str());
    ret= execute_command(cammand, false);
    ALOGI("finished: %s", ret.c_str());
    // Reset states

    // execute_command("setprop sys.vpk.busy 0");
    mVpickBusy = false;
}

bool VpkdService::VpkdService::OnekeyNewDeviceThread::threadLoop() {
    struct timespec wait_time = {1, 0};
    // int cnt = 0;
    bool delayProcess = false;
    while (!exitPending()) {
        int err = nanosleep(&wait_time, nullptr);
        if (err == -1 && errno == EINTR) {
            continue;
        }
        if (!mEnabled) {
            continue;
        }

        if (mTriggerForce == false && mBrandChanged == false && mModelChanged == false) {
            continue;
        }

        if (mTriggerForce == false && (mBrandChanged == false || mModelChanged == false) && (delayProcess == false)) {
            //品牌和型号中只有一个变化，延时处理
            ALOGE("delay process[mBrandChanged:%d,mModelChanged%d]", mBrandChanged, mModelChanged);
            delayProcess = true;
            continue;
        }
        delayProcess = false;
        mTriggerForce = false;
        maybeUpdateDeviceInfo();
    }
    return false;
}

}
