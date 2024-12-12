#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <openssl/md5.h>
#include <vector>
#include <set>
#include <dirent.h>
#include <regex>
#include <iomanip>
#include <ctime>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <map>
#include <random>

#define WORK_DIR "/data/local/tmp/gif/"
#define VERSION "1.0.0"

using namespace std;
bool dbg = false;
bool keepcache = false;


struct DeviceInfo {
    string manufacturer;
    string model;
    string brand;
    string imei1;
    string imei2;
};

DeviceInfo gDeviceInfo = {
    .manufacturer = "",
    .model = "",
    .brand = "",
    .imei1 = "",
    .imei2 = "",
};

// Helper function to remove spaces from a string
string remove_spaces(const string &str) {
    string result;
    for (char ch : str) {
        if (ch != ' ' && ch != '\n' && ch != '\r' && ch != '\t') {  // 去除空格、换行、回车和制表符
            result += ch;
        }
    }
    return result;
}

// Helper function to execute a shell command and return the output
string execute_command(const string &command) {
    FILE *fp = popen(command.c_str(), "r");
    if (!fp) {
        return "";
    }

    string result;
    char buffer[128];
    while (fgets(buffer, sizeof(buffer), fp) != nullptr) {
        result += buffer;
    }

    fclose(fp);
    return result;
}

void gif_config(const std::string& param, const std::string& value) {
    std::string command = "gif config -a " + param + "=" + value;
    if (dbg) cout << command << endl;
    execute_command(command);
}

// Function to compute the MD5 hash of a string
string md5sum(const string &str) {
    unsigned char result[MD5_DIGEST_LENGTH];
    MD5_CTX ctx;
    MD5_Init(&ctx);
    MD5_Update(&ctx, str.c_str(), str.size());
    MD5_Final(result, &ctx);

    stringstream ss;
    for (int i = 0; i < MD5_DIGEST_LENGTH; ++i) {
        ss << hex << (int)result[i];
    }
    return ss.str();
}

bool create_directory(const string &dir_path) {
    // Construct the mkdir -p command
    string command = "mkdir -p " + dir_path;
    // Execute the command using popen
    FILE *fp = popen(command.c_str(), "r");
    if (!fp) {
        cerr << "Failed to execute mkdir command: " << command << endl;
        return false;
    }

    // Check if the command executed successfully
    int status = pclose(fp);
    if (status != 0) {
        cerr << "Failed to create directory: " << dir_path << endl;
        return false;
    }

    return true;
}

void print_help() {
    cout << "Usage: vpick [options]" << endl;
    cout << "  -v|version     Print version" << endl;
    cout << "  -b|backup      Execute information extraction logic" << endl;
    cout << "  -h|help        Display this help message" << endl;
}

// Function to extract system properties and save them
string extract_system_properties_and_save() {
    // Step 2: Get system properties and remove spaces
    string manufacturer = remove_spaces(execute_command("getprop ro.product.manufacturer"));
    string model = remove_spaces(execute_command("getprop ro.product.model"));
    string fingerprint = remove_spaces(execute_command("getprop ro.build.fingerprint"));
    string version = remove_spaces(execute_command("getprop ro.build.version.release"));
    string build_id = remove_spaces(execute_command("getprop ro.build.id"));

    // Step 3: Create the working directory name with "=" separator
    string work_dir_name = manufacturer + "=" + model + "=" + version + "=" + build_id + "=" + md5sum(fingerprint);

    // Step 4: Create the working directory under /sdcard/gif/
    string full_work_dir_path = string(WORK_DIR) + work_dir_name;
    if (!create_directory(full_work_dir_path)) {
        return "";
    }

    // Step 5: Execute getprop and save the output to the specified file
    string getprop_output = execute_command("getprop");
    string file_path = full_work_dir_path + "/prop.org";
    ofstream file(file_path);
    if (file.is_open()) {
        file << getprop_output;
        file.close();
        if (dbg) cout << "Output saved to: " << file_path << endl;
    } else {
        cerr << "Failed to open file: " << file_path << endl;
        return "";
    }

    return full_work_dir_path;
}

// Function to extract system files
bool extract_system_files(const string &work_dir) {
    const vector<string> files_to_extract = {
        "/proc/meminfo",
        "/proc/cpuinfo",
        "/proc/self/mountinfo",
        "/proc/mounts",
        "/sys/devices/system/cpu/possible",
        "/sys/devices/system/cpu/present",
        "/proc/version",
        "/proc/sys/kernel/random/boot_id",
        "/proc/self/smaps",
        "/proc/self/maps",
        "/proc/self/attr/prev"
    };

    for (const string &file : files_to_extract) {
        // Determine the full path for the output file
        stringstream output_file;
        output_file << work_dir << file;
        string output_file_path = output_file.str();

        // Extract the file content by executing 'cat' command
        string file_content = execute_command("cat " + file);

        // Ensure the directory for the file exists
        string file_dir = output_file_path.substr(0, output_file_path.find_last_of('/'));
        if (!create_directory(file_dir)) {
            if (dbg) cerr << "Failed to create directory: " << file_dir << endl;
            return false;
        }

        // Write the file content to the specified file
        ofstream out(output_file_path);
        if (out.is_open()) {
            out << file_content;
            out.close();
            if (dbg) cout << "Saved file: " << output_file_path << endl;
        } else {
            if (dbg) cerr << "Failed to write file: " << output_file_path << endl;
            return false;
        }
    }

    return true;
}

// Function to format and save properties
void format_and_save_properties(const string &work_dir) {
    // Open the prop.org file
    string prop_org_path = work_dir + "/prop.org";
    ifstream prop_file(prop_org_path);
    if (!prop_file.is_open()) {
        cerr << "Failed to open prop.org file" << endl;
        return;
    }

    // Define the prefixes to ignore
    set<string> ignore_prefixes = {
        "init.", "cache.", "debug.", "dev.", "build.", "aaudio.", "audio.",
        "camera.", "events.", "log.", "media.", "mmp.", "sys.", "telephony.",
        "tunnel.", "vold."
    };

    // Open the prop.pick file to write the formatted properties
    string prop_pick_path = work_dir + "/prop.pick";
    ofstream out_file(prop_pick_path);
    if (!out_file.is_open()) {
        cerr << "Failed to open prop.pick file" << endl;
        return;
    }

    string line;
    while (getline(prop_file, line)) {
        // Remove spaces
        // line = remove_spaces(line);

        // Skip empty lines
        if (line.empty()) {
            continue;
        }

        // Check if the line starts with a known prefix to ignore
        bool ignore = false;
        for (const auto &prefix : ignore_prefixes) {
            if (line.find(prefix) == 0) {
                ignore = true;
                break;
            }
        }
        if (ignore) {
            continue;
        }

        // Remove the square brackets and split by ":"
        size_t pos = line.find(":");
        if (pos != string::npos) {
            string key = line.substr(1, pos - 2); // Remove the brackets from key
            string value = line.substr(pos + 2);  // Get value after ":"

            // Remove square brackets from value if present
            if (!value.empty() && value.front() == '[' && value.back() == ']') {
                value = value.substr(1, value.size() - 2); // Remove the leading '[' and trailing ']'
            }

            // Write to the output file in <key>=<value> format
            out_file << key << "=" << value << endl;
        }
    }

    prop_file.close();
    out_file.close();
    if (dbg) cout << "Formatted properties saved to: " << prop_pick_path << endl;
}


// Function to backup pm list features
bool backup_pm_list_features(const string &work_dir) {
    string features = execute_command("pm list features");
    string file_path = work_dir + "/pm_list_features";
    
    ofstream file(file_path);
    if (file.is_open()) {
        file << features;
        file.close();
        if (dbg) cout << "PM List Features backed up to: " << file_path << endl;
        return true;
    } else {
        cerr << "Failed to write PM List Features to: " << file_path << endl;
        return false;
    }
}

// Function to backup the working directory to a .tar.gz file
bool backup_to_tar(const string &work_dir) {
    string tar_command = "tar -czf " + work_dir + ".tar.gz -C " + WORK_DIR + " " + work_dir.substr(strlen(WORK_DIR));
    int status = system(tar_command.c_str());

    if (status != 0) {
        cerr << "Failed to create tar.gz backup" << endl;
        return false;
    }

    if (dbg) cout << "Backup created: " << work_dir + ".tar.gz" << endl;
    return true;
}

int delete_directory(const std::string& path) {
    // Check if directory exists
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        std::cerr << "Directory does not exist: " << path << std::endl;
        return -1;
    }

    // Use rm -rf to force delete the directory
    std::string command = "rm -rf " + path;
    int ret = system(command.c_str());
    if (ret != 0) {
        std::cerr << "Failed to delete directory: " << path << " with error code " << ret << std::endl;
        return -1;
    }
    if (dbg) std::cout << "deleted directory: " << path << std::endl;
    return 0;
}


void backup_main() {
    // Create the base directory
    if (!create_directory(WORK_DIR)) {
        cerr << "Failed to create base directory" << endl;
        return;
    }

    // Extract system properties and save them, and get the work directory path
    string work_dir = extract_system_properties_and_save();
    if (work_dir.empty()) {
        cerr << "Failed to extract system properties" << endl;
        return;
    }

    // Extract system files and save them
    if (!extract_system_files(work_dir)) {
        cerr << "Failed to extract system files" << endl;
        return;
    }

    // Format and save properties to prop.pick
    format_and_save_properties(work_dir);
    backup_pm_list_features(work_dir);
    backup_to_tar(work_dir);
    if (!keepcache) delete_directory(work_dir);
    cout << "Success" << endl;
}

///list
void list_main() {
    string directory = WORK_DIR;
    
    DIR *dir = opendir(directory.c_str());
    if (dir == nullptr) {
        cerr << "Failed to open directory: " << directory << endl;
        return;
    }

    struct dirent *entry;
    vector<string> backup_files;

    // 遍历目录，寻找所有的 .tar.gz 文件
    while ((entry = readdir(dir)) != nullptr) {
        string file_name = entry->d_name;

        // 只处理 .tar.gz 文件
        if (file_name.find(".tar.gz") != string::npos) {
            backup_files.push_back(file_name);
        }
    }

    closedir(dir);

    // 如果没有找到备份文件，给出提示
    if (backup_files.empty()) {
        if (dbg) cout << "No backup files found in " << directory << endl;
        return;
    }

    // 更新的正则表达式，匹配格式：品牌=设备名称=版本号=构建ID.tar.gz
    regex device_regex(R"(([^=]+)=([^=]+)=([^=]+)=([^=]+)=([^=]+)\.tar\.gz)");
    smatch match;

    // 输出标题
    cout << "-----------------------------------------------------" << endl;
    cout << "No.  | Manufacturer    | Device Name     | Version | Build ID" << endl;
    cout << "-----------------------------------------------------" << endl;

    // 打印每个备份文件的信息
    for (size_t i = 0; i < backup_files.size(); ++i) {
        string file_name = backup_files[i];

        if (regex_search(file_name, match, device_regex)) {
            string manufacturer = match[1];  // 厂商
            string device_name = match[2];   // 设备名称
            string version = match[3];       // 版本
            string build_id = match[4];      // 构建 ID

            // 输出信息，左对齐
            printf("%-4zu | %-15s | %-15s | %-7s | %-8s\n", i + 1, manufacturer.c_str(), device_name.c_str(), version.c_str(), build_id.c_str());
        } else {
            cerr << "Failed to parse backup file name: " << file_name << endl;
        }
    }
    cout << "-----------------------------------------------------" << endl;
}


//////restore
bool extract_tar(const string &tar_file, const string &destination) {
    string command = "tar -xzf " + tar_file + " -C " + destination;
    int status = system(command.c_str());
    if (status != 0) {
        cerr << "Failed to extract tar file: " << tar_file << endl;
        return false;
    }
    return true;
}


char random_lowercase() {
    return 'a' + rand() % 26;
}

char random_uppercase() {
    return 'A' + rand() % 26;
}

char random_digit() {
    return '0' + rand() % 10;
}

string generate_new_serialno(const string &value) {
    string new_serialno = value;
    
    for (size_t i = 0; i < new_serialno.length(); ++i) {
        if (islower(new_serialno[i])) {
            new_serialno[i] = random_lowercase();
        } else if (isupper(new_serialno[i])) {
            new_serialno[i] = random_uppercase();
        } else if (isdigit(new_serialno[i])) {
            new_serialno[i] = random_digit();
        }
    }

    return new_serialno;
}

bool restore_property(const string &key, const string &value) {
    // 针对特定属性的特殊处理
    string final_key = key;

    if (key == "ro.build.fingerprint") {
        final_key = "persist.build.v-fingerprint";
    } else if (key == "ro.boot.hwversion") {
        final_key = "ro.boot.hardware.revision";
    }

    if (key == "ro.product.manufacturer") {
        gDeviceInfo.manufacturer = value;
    }
    if (key == "ro.product.brand") {
        gDeviceInfo.brand = value;
    }
    if (key == "ro.product.model") {
        gDeviceInfo.model = value;
        string command = "gif config -a device.model=\"" + value + "\""; 
        if (dbg) cout << command << endl;
        if (system(command.c_str()) != 0) {
            cerr << "Failed to set device.model to " << value << endl;
            return false;
        }
        return true;
    }

    if (key == "ro.serialno") {
        string new_serialno = generate_new_serialno(value);  // 生成新的序列号
        string command = "gif config -a device.serialno=\"" + new_serialno + "\""; 
        if (system(command.c_str()) != 0) {
            cerr << "Failed to set device.serialno to " << new_serialno << endl;
            return false;
        }
        return true;
    }

    if ( key == "ro.ril.oem.imei1" || key == "persist.sim.imei1") {
        gDeviceInfo.imei1 = value;
        return true;
    }
    if (key == "ro.ril.oem.imei2" || key == "persist.sim.imei2") {
        gDeviceInfo.imei2 = value;
        return true;
    }

    // 对其他属性执行 gif setprop 命令
    string command = "gif setprop " + final_key + " \"" + value + "\"";
    if (dbg) cout << command << endl;
    if (system(command.c_str()) != 0) {
        cerr << "Failed to set property: " << final_key << " to " << value << endl;
        return false;
    }

    return true;
}

bool restore_system_properties(const string &work_dir) {
    vector<string> properties_to_restore = {
        //boot 
        "ro.boot.hwversion",
        "ro.boot.hardware.revision",
        "ro.bootimage.build.date",
        "ro.bootimage.build.date.utc",
        "ro.bootimage.build.fingerprint",
        //build
        "ro.build.characteristics",
        "ro.build.date",
        "ro.build.date.utc",
        "ro.build.description",
        "ro.build.display.id",
        "ro.build.fingerprint",
        "ro.build.flavor",
        "ro.build.host",
        "ro.build.id",
        "ro.build.product",
        //"ro.build.shutdown_timeout",
        "ro.build.tags",
        "ro.build.user",
        "ro.build.version.all_codenames",
        "ro.build.version.base_os",
        "ro.build.version.codename",
        "ro.build.version.incremental",
        "ro.build.version.min_supported_target_sdk",
        "ro.build.version.preview_sdk",
        "ro.build.version.preview_sdk_fingerprint",
        "ro.build.version.security_patch",
        "ro.odm.build.date",
        "ro.odm.build.date.utc",
        "ro.odm.build.fingerprint",
        "ro.odm.build.id",
        "ro.odm.build.tags",
        "ro.odm.build.type",
        "ro.odm.build.version.incremental",
        "ro.product.build.date",
        "ro.product.build.date.utc",
        "ro.product.build.fingerprint",
        "ro.product.build.id",
        "ro.product.build.tags",
        "ro.product.build.type",
        "ro.product.build.version.incremental",
        "ro.system.build.date",
        "ro.system.build.date.utc",
        "ro.system.build.fingerprint",
        "ro.system.build.id",
        "ro.system.build.tags",
        "ro.system.build.type",
        "ro.system.build.version.incremental",
        "ro.system_ext.build.fingerprint",
        "ro.system_ext.build.id",
        "ro.system_ext.build.version.incremental",
        "ro.vendor.build.date",
        "ro.vendor.build.date.utc",
        "ro.vendor.build.fingerprint",
        "ro.vendor.build.id",
        "ro.vendor.build.security_patch",
        "ro.vendor.build.tags",
        "ro.vendor.build.type",
        "ro.vendor.build.version.incremental",
        "ro.bootimage.build.date.utc",
        "ro.build.date.utc",
        "ro.odm.build.date.utc",
        "ro.product.build.date.utc",
        "ro.system.build.date.utc",
        "ro.vendor.build.date.utc",
        //product
        "ro.product.brand",
        "ro.product.odm.brand",
        "ro.product.product.brand",
        "ro.product.system.brand",
        "ro.product.system_ext.brand",
        "ro.product.vendor.brand",
        "ro.product.device",
        "ro.product.odm.device",
        "ro.product.product.device",
        "ro.product.system.device",
        "ro.product.system_ext.device",
        "ro.product.vendor.device",
        "ro.product.manufacturer",
        "ro.product.odm.manufacturer",
        "ro.product.product.manufacturer",
        "ro.product.system.manufacturer",
        "ro.product.system_ext.manufacturer",
        "ro.product.vendor.manufacturer",
        "ro.product.model",
        "ro.product.odm.model",
        "ro.product.product.model",
        "ro.product.system.model",
        "ro.product.system_ext.model",
        "ro.product.vendor.model",
        "ro.product.name",
        "ro.product.first_api_level",
        //
        "ro.board.platform",
        "ro.product.board",
        "ro.vendor.sdkversion",
        "ro.hardware",
        // "ro.boot.serialno",
        "ro.serialno",
        // "vendor.serialno",
        "ro.boot.bootloader"
        "ro.boot.baseband"
        "ro.bootloader",
        //gsm
        "gsm.current.phone-type",
        "gsm.network.type",
        "gsm.operator.alpha",
        "gsm.operator.iso-country",
        "gsm.operator.isroaming",
        "gsm.operator.numeric",
        "gsm.operator.orig.alpha",
        "gsm.sim.operator.alpha",
        "gsm.sim.operator.iso-country",
        "gsm.sim.operator.numeric",
        "gsm.sim.operator.orig.alpha",
        "gsm.version.baseband",
        "gsm.version.ril-impl",
        //ril
        // "ro.ril.oem.imei",
        "ro.ril.oem.imei1",
        "ro.ril.oem.imei2",
        "persist.sim.imei1",
        "persist.sim.imei2",

    };

    string prop_pick_path = work_dir + "/prop.pick";
    ifstream prop_file(prop_pick_path);
    if (!prop_file.is_open()) {
        cerr << "Failed to open prop.pick file" << endl;
        return false;
    }

    string line;
    while (getline(prop_file, line)) {
        line = line.substr(line.find_first_not_of(" \t\n\r"), line.find_last_not_of(" \t\n\r") + 1);
        if (line.empty()) {
            continue;
        }
        size_t pos = line.find("=");
        if (pos != string::npos) {
            string key = line.substr(0, pos);
            string value = line.substr(pos + 1);

            for (const auto &prop : properties_to_restore) {
                if (key == prop) {
                    if (!restore_property(key, value)) {
                        return false;
                    }
                    break;
                }
            }
        }
    }

    return true;
}


bool restore_system_files(const string &work_dir) {
    vector<string> files_to_restore = {
        "/proc/meminfo", 
        "/proc/cpuinfo", 
        "/proc/self/mountinfo", 
        "/proc/mounts",
        "/sys/devices/system/cpu/possible", 
        "/sys/devices/system/cpu/present",
        "/proc/version", 
        "/proc/sys/kernel/random/boot_id", 
        "/proc/self/smaps",
        "/proc/self/maps", 
        "/proc/self/attr/prev"
    };

    for (const string &file : files_to_restore) {
        string file_path = work_dir + file;
        ifstream in_file(file_path);
        if (!in_file.is_open()) {
            cerr << "Failed to open file: " << file_path << endl;
            return false;
        }

        stringstream buffer;
        buffer << in_file.rdbuf();
        in_file.close();

        string content = buffer.str();
        ofstream out_file(file);
        if (out_file.is_open()) {
            out_file << content;
            out_file.close();
        } else {
            cerr << "Failed to restore file: " << file << endl;
            return false;
        }
    }

    return true;
}

bool restore_pm_list_features(const string &work_dir) {
    string features_path = work_dir + "/pm_list_features";
    
    ifstream features_file(features_path);
    if (!features_file.is_open()) {
        cerr << "Failed to open pm_list_features file" << endl;
        return false;
    }

    string target_dir = "/data/misc/gif/";
    string target_path = target_dir + "pm_list_features";

    string create_dir_command = "mkdir -p " + target_dir;
    if (system(create_dir_command.c_str()) != 0) {
        cerr << "Failed to create directory: " << target_dir << endl;
        return false;
    }

    string copy_command = "cp " + features_path + " " + target_path;
    if (system(copy_command.c_str()) != 0) {
        cerr << "Failed to copy pm_list_features to: " << target_path << endl;
        return false;
    }

    if (dbg) cout << "pm_list_features copied to: " << target_path << endl;

    return true;
}

bool generate_boot_id() {
    if (dbg) cout << "gen boot_id: /proc/sys/kernel/random/boot_id" << endl;
    execute_command("echo $(uuidgen) > /data/misc/gif/boot_id");
    return true;
}

std::string generate_random_string(int length) {
    std::string result;
    const char charset[] = "0123456789ABCDEF";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(charset) - 2);

    for (int i = 0; i < length; i++) {
        result += charset[dis(gen)];
    }

    return result;
}

std::string generate_oaid_by_manufacturer(const std::string& manufacturer) {
    if (dbg) cout << "generate_imei(" << manufacturer << ")" << endl;
    std::string oaid;
    std::string manufacturer_lowercase = manufacturer;
    std::transform(manufacturer_lowercase.begin(), manufacturer_lowercase.end(), manufacturer_lowercase.begin(), ::tolower);

    if (manufacturer_lowercase == "huawei") {
        oaid = "HUA" + generate_random_string(12);
    } else if (manufacturer_lowercase == "xiaomi") {
        oaid = "XM" + generate_random_string(24);
    } else if (manufacturer_lowercase == "oppo") {
        oaid = "OPP" + generate_random_string(10);
    } else if (manufacturer_lowercase == "vivo") {
        oaid = "VIV" + generate_random_string(10);
    } else {
        oaid = "UNK" + generate_random_string(12);
    }

    return oaid;
}

bool generate_device_info() {
    std::string oaid = generate_oaid_by_manufacturer(gDeviceInfo.manufacturer);

    //oaid    
    string command = "gif config -a device.oaid=\"" + oaid + "\"";
    if (dbg) cout << command << endl;
    execute_command(command);

    return true;
}

int calculate_checkdigit(const std::string& imei) {
    int sum = 0;
    bool is_double = false;

    for (int i = imei.length() - 1; i >= 0; --i) {
        int digit = imei[i] - '0';
        if (is_double) {
            digit *= 2;
            if (digit > 9) {
                digit -= 9;
            }
        }
        sum += digit;
        is_double = !is_double;
    }

    int checkdigit = (10 - (sum % 10)) % 10;
    return checkdigit;
}

std::string get_imei_prefix(const std::string& manufacturer) {
    std::string manufacturer_lower = manufacturer;
    std::transform(manufacturer_lower.begin(), manufacturer_lower.end(), manufacturer_lower.begin(), ::tolower);
    static const std::unordered_map<std::string, std::string> manufacturer_prefixes = {
        {"xiaomi", "86068"},       // 小米
        {"samsung", "35561"},      // 三星
        {"huawei", "86010"},       // 华为
        {"oppo", "86021"},         // OPPO
        {"vivo", "86080"},         // vivo
        {"oneplus", "35999"},      // OnePlus
        {"sony", "35999"},         // 索尼
        {"nokia", "86010"},        // 诺基亚
        {"lenovo", "86039"},       // 联想
        {"asus", "35999"},         // 华硕
        {"realme", "86086"},       // Realme
        {"google", "35812"},       // Google
    };

    auto it = manufacturer_prefixes.find(manufacturer_lower);
    if (it != manufacturer_prefixes.end()) {
        return it->second;
    } else {
        return "86068";
    }
}

std::string generate_imei_with_checkdigit(const std::string& manufacturer, const std::string& imei = "") {
    if (dbg) cout << "generate_imei(" << manufacturer << "," << imei << ")" << endl;
    std::string imei_prefix = get_imei_prefix(manufacturer);

    std::string imei_number;

    if (imei.empty()) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dis(0, 9);

        imei_number = imei_prefix;
        for (int i = 0; i < 13; i++) {
            imei_number += std::to_string(dis(gen));
        }
    } else {
        imei_number = imei.substr(0, 6);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dis(0, 9);
        
        for (int i = 6; i < 14; i++) {
            imei_number += std::to_string(dis(gen));
        }
    }

    int checkdigit = calculate_checkdigit(imei_number);

    imei_number += std::to_string(checkdigit);

    return imei_number;
}

bool generate_sim_info() {
    std::string imei1 = generate_imei_with_checkdigit(gDeviceInfo.manufacturer, gDeviceInfo.imei1);
    std::string imei2 = generate_imei_with_checkdigit(gDeviceInfo.manufacturer, gDeviceInfo.imei2);

    
    string command = "gif config -a sim.imei1=\"" + imei1 + "\"";
    if (dbg) cout << command << endl;
    execute_command(command);

    //imei2
    command = "gif config -a sim.imei2=\"" + imei2 + "\"";
    if (dbg) cout << command << endl;
    execute_command(command);
    return true;
}

std::string generate_ssid() {
    const std::vector<std::string> vendors = {
        "TP-Link", "Netgear", "Xiaomi", "Linksys", "D-Link", "ASUS", "Belkin", 
        "Zyxel", "Huawei", "Tenda", "CTNet", "CTWIFI", "CMCC", "UNINET", 
        "GlocalMe", "Falcon", "Cellular", "Yilian", "Skyroam", "MobileWiFi"
    };

    std::string vendor = vendors[rand() % vendors.size()];
    int number_suffix = rand() % 10000;
    std::string ssid = vendor + "-" + std::to_string(number_suffix);

    if (ssid.length() > 20) {
        ssid = ssid.substr(0, 20);
    } else if (ssid.length() < 10) {
        const std::string charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        while (ssid.length() < 10) {
            ssid += charset[rand() % charset.size()];
        }
    }

    return ssid;
}

const std::unordered_map<std::string, std::string> vendor_ouis = {
    {"TP-Link", "50:3E:AA"},     // TP-Link
    {"Netgear", "E0:91:F5"},     // Netgear
    {"Xiaomi", "28:6C:07"},      // Xiaomi
    {"Linksys", "C0:25:E9"},     // Linksys
    {"D-Link", "78:54:2E"},      // D-Link
    {"ASUS", "D8:50:E6"},        // ASUS
    {"Huawei", "00:1A:3F"},      // Huawei
    {"Tenda", "00:26:5A"},       // Tenda
    {"Skyroam", "90:E2:BA"},     // Skyroam
    {"GlocalMe", "AC:DE:48"},    // GlocalMe
    {"MobileWiFi", "00:1D:A5"},  // 华为移动WiFi
    {"Belkin", "94:10:3E"},      // Belkin
    {"Zyxel", "D8:FE:E3"},       // Zyxel
    {"CTNet", "88:1F:A1"},       // 中国电信网络
    {"CTWIFI", "88:1F:A1"},      // 中国电信WiFi
    {"CMCC", "00:25:86"},        // 中国移动
    {"UNINET", "EC:8A:4C"},      // 中国联通
    {"Falcon", "74:C6:3B"}       // Falcon
};

std::string generate_random_mac_suffix() {
    char suffix[9];
    snprintf(suffix, sizeof(suffix), "%02X:%02X:%02X",
             rand() % 256, rand() % 256, rand() % 256);
    return std::string(suffix);
}

std::string extract_vendor_from_ssid(const std::string &ssid) {
    size_t pos = ssid.find_first_of("-_");
    if (pos != std::string::npos) {
        return ssid.substr(0, pos);
    }
    return ssid;
}

std::string generate_bssid(const std::string &ssid) {
    std::string vendor = extract_vendor_from_ssid(ssid);
    auto it = vendor_ouis.find(vendor);
    std::string prefix = (it != vendor_ouis.end()) ? it->second : "02:00:00";
    std::string suffix = generate_random_mac_suffix();
    return prefix + ":" + suffix;
}

std::string generate_mac_addr() {
    srand(time(NULL));
    unsigned char mac[6];
    // 第一个字节的最低位设置为 0，符合本地管理地址规则
    mac[0] = (rand() % 256) & 0xFE;  // 使其为偶数，符合常见的本地管理地址规范
    for (int i = 1; i < 6; ++i) {
        mac[i] = rand() % 256;
    }

    std::ostringstream oss;
    for (int i = 0; i < 6; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)mac[i];
        if (i != 5) oss << ":";
    }

    return oss.str();
}

std::string generate_ip_addr() {
    srand(time(NULL));
    int network_choice = rand() % 100;
    int ip_part1 = 0, ip_part2 = 0, ip_part3 = 0, ip_part4 = 0;

    if (network_choice < 80) {
        ip_part1 = 192;
        ip_part2 = 168;
        ip_part3 = rand() % 256;
        ip_part4 = rand() % 256;
    } else {
        ip_part1 = 172;
        ip_part2 = 16 + (rand() % 16);
        ip_part3 = rand() % 256;
        ip_part4 = rand() % 256;
    }

    std::ostringstream oss;
    oss << ip_part1 << "." << ip_part2 << "." << ip_part3 << "." << ip_part4;

    return oss.str();
}

int generate_rssi() {
    srand(time(NULL));
    int signal_strength = rand() % 100;
    int rssi = 0;
    if (signal_strength < 20) {
        // 强信号：-30 到 -50 dBm
        rssi = rand() % 21 - 30;
    } else {
        // 中等信号：-50 到 -70 dBm
        rssi = rand() % 21 - 50;
    }

    return rssi;
}

int generate_link_speed() {
    srand(time(NULL));

    int standard_choice = rand() % 3;  // 0 = Wi-Fi 4, 1 = Wi-Fi 5, 2 = Wi-Fi 6
    int link_speed = 0;

    switch (standard_choice) {
        case 0:  // Wi-Fi 4 (802.11n)
            link_speed = rand() % 581 + 20;  // 20 到 600 Mbps
            break;
        case 1:  // Wi-Fi 5 (802.11ac)
            link_speed = rand() % 1201 + 100;  // 100 到 1300 Mbps
            break;
        case 2:  // Wi-Fi 6 (802.11ax)
            link_speed = rand() % 9501 + 100;  // 100 到 9600 Mbps
            break;
    }

    return link_speed;
}

void set_wifi_config(const std::string& param, const std::string& value) {
    std::string command = "gif config -a " + param + "=" + value;
    if (dbg) cout << command << endl;
    execute_command(command);
}

void generate_wifi_info() {
    srand(time(NULL));

    std::string ssid = generate_ssid();
    std::string bssid = generate_bssid(ssid);
    std::string mac_addr = generate_mac_addr();
    std::string ip_addr = generate_ip_addr();
    int rssi = generate_rssi();
    int link_speed = generate_link_speed();

    set_wifi_config("wifi.ssid", ssid);
    set_wifi_config("wifi.bssid", bssid);
    set_wifi_config("wifi.mac_addr", mac_addr);
    set_wifi_config("wifi.ip_addr", ip_addr);
    set_wifi_config("wifi.rssi", std::to_string(rssi));
    set_wifi_config("wifi.linkspeed", std::to_string(link_speed));
    set_wifi_config("wifi.enable", "true");
}

std::map<std::string, std::vector<std::string>> headphone_models = {
    {"Bose", {"SoundSport", "QuietComfort", "QuietControl 30", "Bose Sport Open Earbuds"}},
    {"Sony", {"WH-1000XM5", "WF-1000XM4", "WH-CH710N", "WF-SP800N"}},
    {"Beats", {"Studio Buds", "Powerbeats Pro", "Beats Flex", "Beats Solo3"}},
    {"JBL", {"JBL Free", "JBL Tune 500BT", "JBL Live Pro+", "JBL Reflect Flow"}},
    {"Sennheiser", {"Momentum True Wireless 2", "CX 400BT", "PXC 550-II", "HD 450BT"}}
};

std::string generate_bluetooth_name() {
    srand(time(NULL));

    std::vector<std::string> brands = {"Bose", "Sony", "Beats", "JBL", "Sennheiser"};
    std::string brand = brands[rand() % brands.size()];

    std::vector<std::string> models = headphone_models[brand];
    std::string model = models[rand() % models.size()];

    return brand + " " + model;
}

std::string get_oui_from_bluetooth_name(const std::string& name) {
    std::string brand = name.substr(0, name.find(' '));

    std::map<std::string, std::string> vendor_ouis = {
        {"Sony", "00:1A:7D"},
        {"Bose", "00:16:94"},
        {"Beats", "00:1E:AC"},
        {"JBL", "00:1F:3C"},
        {"Sennheiser", "00:14:9A"}
    };

    if (vendor_ouis.find(brand) != vendor_ouis.end()) {
        return vendor_ouis[brand];
    }

    return "Android-BT";
}

std::string generate_bt_mac(const std::string& bluetooth_name) {
    srand(time(NULL));

    std::string oui = get_oui_from_bluetooth_name(bluetooth_name);
    if (oui.empty()) {
        std::cerr << "Error: Could not find OUI for brand in bluetooth name: " << bluetooth_name << std::endl;
        return "";
    }

    unsigned char mac[6];
    sscanf(oui.c_str(), "%2x:%2x:%2x", &mac[0], &mac[1], &mac[2]);

    for (int i = 3; i < 6; ++i) {
        mac[i] = rand() % 256;
    }

    std::ostringstream oss;
    for (int i = 0; i < 6; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)mac[i];
        if (i != 5) oss << ":";
    }

    return oss.str();
}

void set_bluetooth_config(const std::string& param, const std::string& value) {
    std::string command = "gif config -a " + param + "=" + value;
    if (dbg) cout << command << endl;
    execute_command(command);
}

void generate_bluetooth_info() {
    srand(static_cast<unsigned int>(time(0)));

    std::string name = generate_bluetooth_name();
    std::string mac_addr = generate_bt_mac(name);

    if (mac_addr.empty()) {
        std::cerr << "Failed to generate Bluetooth MAC Address." << std::endl;
        return;
    }

    set_bluetooth_config("bt.name", name);
    set_bluetooth_config("bt.mac", mac_addr);
    set_bluetooth_config("bt.enable", "true");
}

std::string generate_android_id() {
    std::srand(std::time(nullptr));

    unsigned long long timestamp = static_cast<unsigned long long>(std::time(nullptr));

    unsigned long long random_part = std::rand();

    unsigned long long android_id_value = timestamp ^ random_part;

    std::stringstream ss;
    ss << std::hex << std::setw(16) << std::setfill('0') << android_id_value;

    return ss.str();
}

bool generate_misc_info() {
    std::string android_id = generate_android_id();
    gif_config("misc.android_id", android_id);
    return true;
}

bool select_backup_and_restore(int index) {
    DIR *dir = opendir(WORK_DIR);
    if (dir == nullptr) {
        cerr << "Failed to open directory: " << WORK_DIR << endl;
        return false;
    }

    vector<string> backup_files;
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        string file_name = entry->d_name;
        if (file_name.find(".tar.gz") != string::npos) {
            backup_files.push_back(file_name);
        }
    }
    closedir(dir);

    if (backup_files.empty()) {
        cerr << "No backup files found in " << WORK_DIR << endl;
        return false;
    }

    // Check if the index is valid
    if (index < 1 || index > backup_files.size()) {
        cerr << "Invalid backup index" << endl;
        return false;
    }

    // Adjust to 0-based index
    int adjusted_index = index - 1;
    string selected_backup = backup_files[adjusted_index];
    if (dbg) cout << "Selected backup: " << selected_backup << endl;

    // Prepare destination directory
    string destination_dir = "/data/local/tmp/.gif/";

    // Check if the destination directory exists, and create it if not
    struct stat st = {0};
    if (stat(destination_dir.c_str(), &st) == -1) {
        if (dbg) cout << "Directory " << destination_dir << " does not exist. Creating it." << endl;
        if (mkdir(destination_dir.c_str(), 0777) != 0) {
            cerr << "Failed to create directory: " << destination_dir << endl;
            return false;
        }
    }

    // Extract the selected tar.gz file
    string tar_file = WORK_DIR + selected_backup;
    if (!extract_tar(tar_file, destination_dir)) {
        cerr << "Failed to extract tar file: " << tar_file << endl;
        return false;
    }

    string work_dir = destination_dir + selected_backup.substr(0, selected_backup.find(".tar.gz"));
    
    if (!restore_system_properties(work_dir)) {
        cerr << "Failed to restore system properties" << endl;
        return false;
    }

    if (!restore_pm_list_features(work_dir)) {
        cerr << "Failed to restore pm list features" << endl;
        return false;
    }

    //TODO: Call other restore functions as needed (e.g., restore_system_files)

    if (!keepcache) delete_directory(work_dir);
    return true;
}

void restore_main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "Please provide a backup index to restore" << endl;
        return;
    }

    int index = atoi(argv[2]);

    if (dbg) cout << "restore index provided: " << index << endl;

    if (index <= 0) {
        cerr << "Invalid backup index" << endl;
        return;
    }

    if (!select_backup_and_restore(index)) {
        cerr << "Failed to restore the selected backup" << endl;
    }
    generate_boot_id();
    generate_device_info();
    generate_sim_info();
    generate_wifi_info();
    generate_bluetooth_info();
    generate_misc_info();
    cout << "Success" << endl;
}


int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_help();
        return 0;
    }

    for (int i = 1; i < argc; ++i) {
        string option = argv[i];
        if ( option == "--dbg" || option == "--debug") {
            dbg = true;
            cout << "Debug mode enabled." << endl;
            continue;
        } else if ( option == "--kc" || option == "--keepcache") {
            keepcache = true;
            cout << "keepcache enabled." << endl;
            continue;
        }
    }

    string option = argv[1];
    
    if (option == "-v" || option == "--version") {
        cout << "Version: " << VERSION << endl;
    } else if (option == "-b" || option == "backup") {
        backup_main();
    } else if (option == "-r" || option == "restore") {
        restore_main(argc, argv);
    } else if (option == "-l" || option == "list") {
        list_main();
    } else if (option == "-h" || option == "help") {
        print_help();
    } else {
        cerr << "Unknown option: " << option << endl;
        print_help();
    }

    return 0;
}