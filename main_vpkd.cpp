#define LOG_TAG "rcd_main"
#include <time.h>
#include <pthread.h>
#include <cutils/properties.h>

#include <memory>
#include <thread>

#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include "service/VpkdService.h"
//#include "curl/curl.h"


using namespace android;
using namespace std::chrono;


static pthread_t proc_monitor_thread_t;

void* system_prop_check_thread(void *para) {
    int cnt = 0;
    char temp[32] = {};

    ALOGI("system_prop_check begin");
    while(1) {
        cnt = property_get_int32("persist.hello.cnt", 0);
        cnt++;
        ALOGE("persist.hello.cnt=%d", cnt);
        sprintf(temp, "%d", cnt);
        property_set("persist.hello.cnt", temp);
        sleep(5);
    }
    ALOGI("system_prop_check end");
    return NULL;
}


int main(int argc, char* argv[]) {
    pthread_create(&proc_monitor_thread_t, NULL, system_prop_check_thread, NULL);

    ProcessState::self()->startThreadPool();
    auto sm = defaultServiceManager();

    sp<vpk::VpkdService> vpkdService(new vpk::VpkdService);

    vpkdService->init();

    auto status = sm->addService(String16(vpk::VpkdService::getServiceName()),
                                 vpkdService,
                                 false,
                                 IServiceManager::DUMP_FLAG_PRIORITY_DEFAULT);

    ALOGI("addService:%d", status);
    IPCThreadState::self()->joinThreadPool();

    return 0;
}