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

#include "version.h"
#include "xor_encrypt.h"


#define WORK_DIR "/data/local/tmp/plugin/meta/vpk/"

using namespace std;

const string encrypted = "E";
bool dbg = false;
bool keepcache = false;


struct GPUInfo {
    std::string vendor;
    std::string renderer;
    std::string version;
};

struct DeviceInfo {
    string manufacturer;
    string brand;
    string model;
    string imei1;
    string imei2;
    bool withGpu;
    GPUInfo gpu;
};

DeviceInfo gDeviceInfo = {
    .manufacturer = "",
    .model = "",
    .brand = "",
    .imei1 = "",
    .imei2 = "",
    .withGpu = false,
    .gpu = {
        .vendor = "",
        .renderer = "",
        .version = "",
    },
};

struct OpstionsType {
    //for restore
    bool withIndex;
    int index;
    bool withBrand;
    string brand;
    bool withModel;
    string model;
    //for show
    bool propOnly;
    bool featureOnly;
    //for encript & decript
    string mode; // {"encript", "decript"}
    bool withInput;
    string input;
    bool withOutput;
    string output;
    bool withkey;
    string key;
    bool withSetpropCmd;
    string setpropCmd;
};

OpstionsType gOpstions = {
    //for restore
    .withIndex = false,
    .index = -1,
    .withBrand = false,
    .brand = "",
    .withModel = false,
    .model = "",
    //for show
    .propOnly = false,
    .featureOnly = false,
    ////for encrypt & decrypt
    .mode = "",
    .withInput = false,
    .input = "",
    .withOutput = false,
    .output = "",
    .withkey = false,
    .key = "hello",
};

struct FileInfo {
    std::string manufacturer;
    std::string brand;
    std::string model;
    std::string android_version;
    std::string build_id;
    std::string enc_flag;
    std::string checksum;
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


bool gif_setprop(const std::string& key, const std::string& value) {
    std::string setprop_cmd = "gif setprop";
    if (gOpstions.withSetpropCmd == true) {
        setprop_cmd = gOpstions.setpropCmd;
    }

    string command = setprop_cmd + " " + key + " \"" + value + "\"";
    if (dbg) cout << command << endl;
    if (system(command.c_str()) != 0) {
        cerr << "Failed to set property: " << key << " to " << value << endl;
        return false;
    }
    return true;
}

// 全局常量映射表
const std::map<std::string, std::string> config_prop_map = {
    {"device.model",      "ro.product.model"},
    {"device.serialno",   "ro.serialno"},
    {"device.oaid",       "persist.oaid"},
    {"gpu.vendor",        "persist.gpu.vendor"},
    {"gpu.renderer",      "persist.gpu.renderer"},
    {"gpu.version",       "persist.gpu.version"},
    {"sim.shortname",     "persist.sim.shortname"},
    {"sim.deviceid",      "persist.sim.deviceid"},
    {"sim.imei",          "persist.sim.imei"},
    {"sim.meid",          "persist.sim.meid"},
    {"sim.imsi",          "persist.sim.imsi"},
    {"sim.imeisv",        "persist.sim.imeisv"},
    {"sim.iccid",         "persist.sim.iccid"},
    {"sim.operator_name", "persist.sim.operator_name"},
    {"sim.operator_code", "persist.sim.operator_code"},
    {"sim.sim_code",      "persist.sim.sim_code"},
    {"sim.country_iso",   "persist.sim.country_iso"},
    {"sim.sim_iso",       "persist.sim.sim_iso"},
    {"sim.hasicccard",    "persist.sim.hasicccard"},
    {"sim.state",         "persist.sim.state"},
    {"sim.datatype",      "persist.sim.datatype"},
    {"sim.sn",            "persist.sim.sn"},
    {"sim.phonenumber",   "persist.sim.phonenumber"},
    {"sim.sid",           "persist.sim.sid"},
    {"sim.signalStrength","persist.sim.signalStrength"},
    {"sim.lac",           "persist.sim.lac"},
    {"sim.cid",           "persist.sim.cid"},
    {"sim.spn",           "persist.sim.spn"},
    {"sim.msisdn",        "persist.sim.msisdn"},
    {"sim.esn",           "persist.sim.esn"},
    {"sim.phonetype",     "persist.sim.phonetype"},
    {"sim.roaming",       "persist.sim.roaming"},
    {"wifi.ssid",         "persist.wifi.ssid"},
    {"wifi.bssid",        "persist.wifi.bssid"},
    {"wifi.mac_addr",     "persist.wifi.mac_addr"},
    {"wifi.ip_addr",      "persist.wifi.ip_addr"},
    {"wifi.rssi",         "persist.wifi.rssi"},
    {"wifi.linkspeed",    "persist.wifi.linkspeed"},
    {"wifi.enable",       "persist.wifi.enable"},
    {"wifi.dhcp_gateway", "persist.wifi.dhcp_gateway"},
    {"wifi.dhcp_netmask", "persist.wifi.netmask"},
    {"wifi.dhcp_dns1",    "persist.wifi.dns1"},
    {"wifi.dhcp_dns2",    "persist.wifi.dns2"},
    {"wifi.dhcp_server",  "persist.wifi.server"},
    {"wifi.dhcp_lease",   "persist.wifi.lease"},
    {"wifi.frequency",    "persist.wifi.frequency"},
    {"bt.name",           "persist.bt.name"},
    {"bt.mac",            "persist.bt.mac"},
    {"bt.enable",         "persist.bt.enable"},
    {"misc.android_id",   "persist.sys.android_id"},
};

std::string get_propkey_from_config(const std::string& key) {
    auto it = config_prop_map.find(key);
    if (it != config_prop_map.end()) {
        return it->second; // 如果找到键，返回对应值
    }
    return ""; // 如果未找到，返回空字符串
}

bool gif_config(const std::string& key, const std::string& value) {
    std::string prop_key = get_propkey_from_config(key);
    if (prop_key != "") {
        return gif_setprop(prop_key, value);
    }
    std::string command = "gif config -a " + key + "=\"" + value + "\"";
    if (dbg) cout << command << endl;
    execute_command(command);

    // if (key == "device.model") {
    //     command = "settings put global device_name \"" + value + "\"";
    //     if (dbg) cout << command << endl;
    //     execute_command(command);
    // }
    return true;
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

// Function to extract system properties and save them
string extract_system_properties_and_save() {
    // Step 2: Get system properties and remove spaces
    string manufacturer = remove_spaces(execute_command("getprop ro.product.manufacturer"));
    string brand = remove_spaces(execute_command("getprop ro.product.brand"));
    string model = remove_spaces(execute_command("getprop ro.product.model"));
    string fingerprint = remove_spaces(execute_command("getprop ro.build.fingerprint"));
    string version = remove_spaces(execute_command("getprop ro.build.version.release"));
    string build_id = remove_spaces(execute_command("getprop ro.build.id"));

    // Step 3: Create the working directory name with "=" separator
    string work_dir_name = manufacturer + "=" + brand + "=" + model + "=" + version + "=" + build_id + "=" +  encrypted + "=" + md5sum(fingerprint);

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
bool backup_system_files(const string &work_dir) {
    const vector<string> files_to_extract = {
        "/proc/meminfo",
        "/proc/cpuinfo",
        "/proc/self/mountinfo",
        "/proc/mounts",
        "/sys/devices/system/cpu/possible",
        "/sys/devices/system/cpu/present",
        "/proc/version",
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
void backup_system_prop(const string &work_dir) {
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


// 解析 GPU 信息
GPUInfo parse_gpu_info(const std::string &output) {
    GPUInfo info;

    // 定位 "GLES:" 并提取相关内容
    std::size_t pos = output.find("GLES:");
    if (pos == std::string::npos) {
        std::cerr << "Error: No GLES information found in output." << std::endl;
        return info;
    }

    // 提取 "GLES:" 后的部分
    std::string gles_info = output.substr(pos + 5); // 跳过 "GLES: "
    std::istringstream stream(gles_info);

    // 按逗号分割
    std::vector<std::string> parts;
    std::string part;
    while (std::getline(stream, part, ',')) {
        parts.push_back(part);
    }

    // 提取信息
    if (parts.size() >= 3) {
        info.vendor = parts[0];    // 供应商
        info.renderer = parts[1]; // 渲染器

        // 拼接版本信息（从第 3 个部分开始拼接后续所有内容）
        info.version = parts[2];
        for (size_t i = 3; i < parts.size(); ++i) {
            info.version += "," + parts[i];
        }
    }

    // 去除多余空格
    auto trim = [](std::string &s) {
        s.erase(0, s.find_first_not_of(" \t"));
        s.erase(s.find_last_not_of(" \t") + 1);
    };
    trim(info.vendor);
    trim(info.renderer);
    trim(info.version);

    return info;
}

// 备份 GPU 信息到文件
bool backup_gpu_info(const std::string &work_dir) {
    string file_path = work_dir + "/gpu_info";
    // 获取命令输出
    std::string output = execute_command("dumpsys SurfaceFlinger | grep GLES");
    if (output.empty()) {
        std::cerr << "Error: Failed to retrieve GPU information for backup." << std::endl;
        return false;
    }

    // 保存到文件
    std::ofstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "Error: Failed to open file for backup: " << file_path << std::endl;
        return false;
    }

    file << output;
    file.close();

    if (dbg) std::cout << "save GPU info to " << file_path << std::endl;
    return true;
}

// 恢复 GPU 信息并解析
bool restore_gpu_info(const std::string &work_dir) {
    string file_path = work_dir + "/gpu_info";
    // 打开文件读取内容
    std::ifstream file(file_path);
    if (!file.is_open()) {
        if (dbg) std::cerr << "Error: Failed to open file for restore: " << file_path << std::endl;
        return false;
    }

    std::string line, content;
    while (std::getline(file, line)) {
        cout << line << endl;
        content += line;
    }
    file.close();

    if (content.empty()) {
        std::cerr << "Error: No GPU information found in backup file." << std::endl;
        return false;
    }

    // 解析 GPU 信息
    GPUInfo gpu_info = parse_gpu_info(content);
    if (gpu_info.vendor.empty() || gpu_info.renderer.empty() || gpu_info.version.empty()) {
        std::cerr << "Error: Failed to parse GPU information." << std::endl;
        return false;
    }

    gDeviceInfo.withGpu = true;
    gDeviceInfo.gpu.vendor = gpu_info.vendor;
    gDeviceInfo.gpu.renderer = gpu_info.renderer;
    gDeviceInfo.gpu.version = gpu_info.version;

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

void backup_version(const std::string& path) {
    execute_command("echo " + VERSION + " > " + path + "/version");
}

std::vector<std::string> split_string(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

// 解析文件名并提取字段的函数
FileInfo parse_filename(const std::string& filename) {
    FileInfo info;

    // 移除文件扩展名
    size_t pos = filename.find_last_of('.');
    std::string base_name = (pos != std::string::npos) ? filename.substr(0, pos) : filename;

    // 分割字段
    std::vector<std::string> fields = split_string(base_name, '=');
    if (fields.size() == 7) {  // 确保字段数量正确
        info.manufacturer = fields[0];
        info.brand = fields[1];
        info.model = fields[2];
        info.android_version = fields[3];
        info.build_id = fields[4];
        info.enc_flag = fields[5];
        info.checksum = fields[6];
    } else {
        std::cerr << "Invalid file format: " << filename << std::endl;
    }

    return info;
}

bool backup_to_tar(const string &work_dir) {
    //tar work_dir:
    std::string filename = work_dir + ".tar.gz";
    std::string command = "tar -czf " + filename + " -C " + WORK_DIR + " " + work_dir.substr(strlen(WORK_DIR));
    if (dbg) cout << command << endl;
    execute_command(command);
    sync();
    //encrypt
    std::string tmpname = work_dir + ".tar.gz.enc";
    encrypt_set_dbg(dbg);
    encrypt_gzip_file(filename, tmpname, gOpstions.key);
    // command = "/data/local/tmp/vpick encrypt -i "  + filename + " -o " + tmpname;
    // if (dbg) cout << command << endl;
    // execute_command(command);
    //remove tar.gz
    command = "rm -fr "  + filename;
    if (dbg) cout << command << endl;
    execute_command(command);
    //rename tar.gz.enc => tar.gz
    command = "mv "  + tmpname + " " + filename;
    if (dbg) cout << command << endl;
    execute_command(command);
    
    return true;
}

void split_path(const std::string& full_path, std::string& directory, std::string& filename) {
    size_t pos = full_path.find_last_of("/\\");
    if (pos != std::string::npos) {
        directory = full_path.substr(0, pos);
        filename = full_path.substr(pos + 1);
    } else {
        directory.clear();
        filename = full_path;
    }
}

bool extract_tar(const string &tar_file, const string &destination) {
    std::string work_dir;
    std::string filename;
    split_path(tar_file, work_dir, filename);
    FileInfo info = parse_filename(filename);
    std::string command = "";
    std::string file_encrypted = destination + filename + ".enc";
    std::string file_deccrypted = destination + filename;

    if (info.enc_flag == "N") {
        if (dbg) cout << "enc = N" << endl;
        command = "cp "  + tar_file + " " + file_deccrypted;
        if (dbg) cout << command << endl;
        execute_command(command);
    } else if (info.enc_flag == "E") {
        if (dbg) cout << "enc = E" << endl;
        command = "cp "  + tar_file + " " + file_encrypted;
        if (dbg) cout << command << endl;
        execute_command(command);
        //decrypt
        encrypt_set_dbg(dbg);
        decrypt_gzip_file(file_encrypted, file_deccrypted, gOpstions.key);
        //remove tar.gz
        command = "rm -fr "  + file_encrypted;
        if (dbg) cout << command << endl;
        execute_command(command);
    }

    //extract tar.gz
    command = "tar -xzf " + file_deccrypted + " -C " + destination;
    if (dbg) cout << command << endl;
    execute_command(command);

    //rm tar.gz
    command = "rm " + file_deccrypted;
    if (dbg) cout << command << endl;
    execute_command(command);
    
    return true;
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

    backup_version(work_dir);
    backup_system_files(work_dir);
    backup_system_prop(work_dir);
    backup_pm_list_features(work_dir);
    backup_gpu_info(work_dir);
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

    // 匹配格式：厂商=品牌=型号=版本=构建ID=是否加密.tar.gz
    regex device_regex(R"(([^=]+)=([^=]+)=([^=]+)=([^=]+)=([^=]+)=([^=]+)=([^=]+)\.tar\.gz)");
    smatch match;

    // 输出标题
    cout << "-------------------------------------------------------------------------------------------------" << endl;
    cout << "index | manufacturer    | brand           | model           | Version | Build ID           | flag" << endl;
    cout << "-------------------------------------------------------------------------------------------------" << endl;

    // 打印每个备份文件的信息
    for (size_t i = 0; i < backup_files.size(); ++i) {
        string file_name = backup_files[i];

        if (regex_search(file_name, match, device_regex)) {
            string manufacturer = match[1];  // 厂商
            string brand = match[2];         // 品牌
            string model = match[3];         // 型号
            string version = match[4];       // 版本
            string build_id = match[5];      // 构建 ID
            string enc = match[6];           // enc

            if ((gOpstions.withBrand == true) && (gOpstions.brand != brand)) {
                continue;
            }
            if ((gOpstions.withModel == true) && (gOpstions.model != model)) {
                continue;
            }
            // 输出信息，左对齐
            printf("%-5zu | %-15s | %-15s | %-15s | %-7s | %-18s | %-4s\n", i + 1, manufacturer.c_str(), brand.c_str(), model.c_str(), version.c_str(), build_id.c_str(), enc.c_str());
        } else {
            cerr << "Failed to parse backup file name: " << file_name << endl;
        }
    }
    cout << "-------------------------------------------------------------------------------------------------" << endl;
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
        std::string command = "settings put global device_name \"" + value + "\"";
        if (dbg) cout << command << endl;
        execute_command(command);
        //return gif_config("device.model", value);
    }

    if (key == "ro.serialno") {
        string serialno = generate_new_serialno(value);
        return gif_config("device.serialno", serialno);
    }

    if ( key == "ro.ril.oem.imei1" || key == "persist.sim.imei1") {
        gDeviceInfo.imei1 = value;
        return true;
    }
    if (key == "ro.ril.oem.imei2" || key == "persist.sim.imei2") {
        gDeviceInfo.imei2 = value;
        return true;
    }

    return gif_setprop(final_key, value);
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
        "ro.product.odm.name",
        "ro.product.product.name",
        "ro.product.system.name",
        "ro.product.vendor.name",
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
        //
        "persist.sys.gps.lpp",
        "ro.soc.manufacturer",
        "ro.soc.model",
        //version.release
        "ro.build.version.release",
        "ro.odm.build.version.release",
        "ro.product.build.version.release",
        "ro.system.build.version.release",
        "ro.vendor.build.version.release",
    };

    string prop_pick_path = work_dir + "/prop.pick";
    ifstream prop_file(prop_pick_path);
    if (!prop_file.is_open()) {
        cerr << "Failed to open prop.pick file:" << prop_pick_path << endl;
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
                        // return false;
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

string generate_new_oaid(const string &value) {
    string new_oaid = value;
    
    for (size_t i = 0; i < new_oaid.length(); ++i) {
        if (islower(new_oaid[i])) {
            new_oaid[i] = random_lowercase();
        } else if (isupper(new_oaid[i])) {
            new_oaid[i] = random_uppercase();
        } else if (isdigit(new_oaid[i])) {
            new_oaid[i] = random_digit();
        }
    }

    return new_oaid;
}

std::string generate_oaid_by_manufacturer(const std::string& manufacturer) {
    if (dbg) cout << "generate_imei(" << manufacturer << ")" << endl;
    std::string oaid;
    std::string manufacturer_lowercase = manufacturer;
    std::transform(manufacturer_lowercase.begin(), manufacturer_lowercase.end(), manufacturer_lowercase.begin(), ::tolower);

    if (manufacturer_lowercase == "huawei") {
        string oaid_sample = "fbcdfdc7-7bef-9ad6-feff-ba3d777e6b0e";
        oaid = generate_new_oaid(oaid_sample);
    } else if (manufacturer_lowercase == "honor") {
        string oaid_sample = "f8d2a3c4-12e6-4a78-9b2d-abcdef123456";
        oaid = generate_new_oaid(oaid_sample);
    } else if (manufacturer_lowercase == "xiaomi") {
        string oaid_sample = "fc0a41a6a680655f";
        oaid = generate_new_oaid(oaid_sample);
    } else if (manufacturer_lowercase == "blackshark") {
        string oaid_sample = "cf22941f7c814433";
        oaid = generate_new_oaid(oaid_sample);
    } else if (manufacturer_lowercase == "vivo") {
        //string oaid_sample = "5a83151f76e45da3b73d06bfa1893270d274019e2db558d89a7c8f344f88cd06";
        string oaid_sample = "d6d6ad3073f4a3500fe495eb445c2dbc3522f7e17717d19b5fc01a876c6721c2";
        oaid = generate_new_oaid(oaid_sample);
    } else if (manufacturer_lowercase == "oppo") {
        string oaid_sample = "332D37A70BED43338D4864FCD1F08B9550e88790074327f4544802b8031fa895";
        oaid = generate_new_oaid(oaid_sample);
    } else if (manufacturer_lowercase == "realme") {
        string oaid_sample = "2E6D2BE01C6B4BF4A1089901880F6236c03941023114c1957e6856b4e7c65398";
        oaid = generate_new_oaid(oaid_sample);
    } else if (manufacturer_lowercase == "meizu") {
        string oaid_sample = "8f94bfa4c07dd618a46f5e7c57d2457e";
        oaid = generate_new_oaid(oaid_sample);
    } else if (manufacturer_lowercase == "samsung") {
        string oaid_sample = "9f2b8837b90d87f73da0a1c9c7df4f6c3741a24c85a74c7088f48bc914420944";
        oaid = generate_new_oaid(oaid_sample);
    } else if (manufacturer_lowercase == "Google") {
        string oaid_sample = "3f776c49-3479-4297-a1a3-aad4653118f6";
        oaid = generate_new_oaid(oaid_sample);
    } else if (manufacturer_lowercase == "zte") {
        string oaid_sample = "e4aa8cff-225c-4320-8b62-6ad8b8fe0cc3";
        oaid = generate_new_oaid(oaid_sample);
    } else if (manufacturer_lowercase == "motorola") {
        string oaid_sample = "18c90ffb-41f9-491c-b030-9fa75c01b114";
        oaid = generate_new_oaid(oaid_sample);
    } else if (manufacturer_lowercase == "sony") {
        string oaid_sample = "9992c022-1264-46ac-8fb1-38a5dd782e9b";
        oaid = generate_new_oaid(oaid_sample);
    } else if (manufacturer_lowercase == "hmd global") {
        string oaid_sample = "a9fe8f3b-61d3-404a-9b0d-cabee6cfa041";
        oaid = generate_new_oaid(oaid_sample);
    } else if (manufacturer_lowercase == "htc") {
        string oaid_sample = "19035315-9800-4600-8276-5618c15d170";
        oaid = generate_new_oaid(oaid_sample);
    } else if (manufacturer_lowercase == "lge") {
        string oaid_sample = "18b26f65-1380-473d-a463-899aa523479f";
        oaid = generate_new_oaid(oaid_sample);
    } else {
        string oaid_sample = "f8d2a3c4-12e6-4a78-9b2d-abcdef123456";
        oaid = generate_new_oaid(oaid_sample);
    }

    return oaid;
}

// Helper function to transform a string to lowercase
std::string to_lowercase(const std::string &input) {
    std::string result = input;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

// Helper function to transform a string to uppercase
std::string to_uppercase(const std::string &input) {
    std::string result = input;
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

// Function to normalize brand names
std::string normalize_brand(const std::string &brand) {
    if (brand == "google" || brand == "Google") return "google";
    if (brand == "honor" || brand == "HONOR") return "HONOR";
    if (brand == "huawei" || brand == "HUAWEI") return "HUAWEI";
    if (brand == "oppo" || brand == "OPPO") return "OPPO";
    if (brand == "oneplus" || brand == "OnePlus") return "OnePlus";
    if (brand == "poco" || brand == "POCO") return "POCO";
    if (brand == "redmi" || brand == "Redmi") return "Redmi";
    if (brand == "xiaomi" || brand == "Xiaomi") return "Xiaomi";
    if (brand == "samsung" || brand == "Samsung") return "samsung";
    if (brand == "vivo" || brand == "Vivo") return "vivo";
    return brand; // Default: return as-is
}

//mc.meng
GPUInfo generate_gpu_info(const std::string &brand, const std::string &model) {
    if (dbg) cout << "generate_gpu_info(" << brand << "," << model << ")" << endl;
    std::map<std::string, std::map<std::string, GPUInfo>> gpu_database = {
        {"google", {
            {"Pixel8a", {"ARM", "Arm Mali-G715", "OpenGL ES 3.2 V1.2.60.0"}},
            {"Pixel", {"Qualcomm", "Adreno (TM) 530", "OpenGL ES 3.2 V@145.0"}},
            {"Pixel4", {"Qualcomm", "Adreno (TM) 640", "OpenGL ES 3.2 V@175.0"}}
        }},
        {"HONOR", {
            {"ELZ-AN10", {"Qualcomm", "Adreno (TM) 660", "OpenGL ES 3.2 V1.2.54.0"}},
            {"PGT-AN00", {"Qualcomm", "Adreno (TM) 740", "OpenGL ES 3.2 V1.2.56.0"}},
            {"PGT-AN10", {"Qualcomm", "Adreno (TM) 740", "OpenGL ES 3.2 V1.2.56.0"}},
            {"OXF-AN10", {"ARM", "Mali - G76", "OpenGL ES 3.2 V1.2.47.0"}},
            {"YAL-AL50", {"ARM", "G52 MP6", "OpenGL ES 3.2 V1.2.47.0"}}
        }},
        {"HUAWEI", {
            {"NOH-AN00", {"ARM", "Mali-G78 MP24", "OpenGL ES 3.2 V1.2.72.0"}},
            {"TNN-AN00", {"MediaTek", "Mali - G77 MC4", "OpenGL ES 3.2 V1.2.47.0"}},
            {"ALN-AL10", {"ARM", "Mali - G57", "OpenGL ES 3.2 V1.2.47.0"}},
            {"CLS-AL00", {"ARM", "Mali - G57", "OpenGL ES 3.2 V1.2.60.0"}}
        }},
        {"OPPO", {
            {"PHZ110", {"MediaTek", "Immortalis-G720 MC10", "OpenGL ES 3.2 V1.2.63.0"}}
        }},
        {"OnePlus", {
            {"PHB110", {"Qualcomm", "Adreno (TM) 740", "OpenGL ES 3.2 V@0676.32"}},
            {"PJA110", {"Qualcomm", "Adreno (TM) 730", "OpenGL ES 3.2 V@0665.25"}},
            {"PHK110", {"Qualcomm", "Adreno (TM) 740", "OpenGL ES 3.2 V@0676.32"}}
        }},
        {"POCO", {
            {"POCOF2Pro", {"Qualcomm", "Adreno (TM) 650", "OpenGL ES 3.2 V@510.0"}},
        }},
        {"Redmi", {
            {"RedmiNote8Pro", {"ARM", "Mali-G76 MC4", "OpenGL ES 3.2 V1.2.47.0"}},
            {"RedmiK30Pro", {"Qualcomm", "Adreno (TM) 650", "OpenGL ES 3.2 V@450.0"}},
            {"RedmiK30", {"Qualcomm", "Adreno (TM) 618", "OpenGL ES 3.2 V@334.0"}},
            {"M2104K10AC", {"ARM", "Mali-G57 MC3", "OpenGL ES 3.2 V1.2.56.0"}},
            {"22127RK46C", {"ARM", "Adreno (TM) 650", "OpenGL ES 3.2 V1.2.63.0"}}
        }},
        {"Xiaomi", {
            {"Mi10", {"Qualcomm", "Adreno (TM) 650", "OpenGL ES 3.2 V@450.0"}},
            {"M2002J9E", {"Qualcomm", "Adreno (TM) 620", "OpenGL ES 3.2 V@334.0"}},
        }},
        {"samsung", {
            {"SM-A5160", {"ARM", "Mali - G76 MP5", "OpenGL ES 3.2 V1.1.30.0"}}
        }},
        {"vivo", {
            {"V2001A", {"ARM", "Adreno (TM) 620", "OpenGL ES 3.2 V1.2.47.0"}},
            {"V2232A", {"ARM", "Adreno (TM) 730", "OpenGL ES 3.2 V1.2.54.0"}}
        }}
    };

    // GPUInfo default_gpu = {"Qualcomm", "Adreno (TM) 650", "OpenGL ES 3.2 V@450.0"};
    GPUInfo default_gpu = {"Qualcomm", "Adreno (TM) 650", "OpenGL ES 3.2 V@450.0"};

    // Normalize brand name
    std::string normalized_brand = normalize_brand(brand);

    // Remove spaces from model
    std::string trimmed_model = model;
    trimmed_model.erase(std::remove(trimmed_model.begin(), trimmed_model.end(), ' '), trimmed_model.end());
    // Look for the brand and model in the database
    if (gpu_database.find(normalized_brand) != gpu_database.end()) {
        const auto &models = gpu_database[normalized_brand];
        if (models.find(trimmed_model) != models.end()) {
            return models.at(trimmed_model);
        }
    }

    return default_gpu; // Return default GPU info if not found
}

bool generate_device_info() {
    std::string oaid = generate_oaid_by_manufacturer(gDeviceInfo.manufacturer);

    //oaid
    gif_config("device.oaid", oaid);

    //gpu info:
    if (gDeviceInfo.withGpu) {
        gif_config("gpu.vendor", gDeviceInfo.gpu.vendor);
        gif_config("gpu.renderer", gDeviceInfo.gpu.renderer);
        gif_config("gpu.version", gDeviceInfo.gpu.version);
    } else {
        GPUInfo gpu = generate_gpu_info(gDeviceInfo.brand, gDeviceInfo.model);
        gif_config("gpu.vendor", gpu.vendor);
        gif_config("gpu.renderer", gpu.renderer);
        gif_config("gpu.version", gpu.version);
    }
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

std::string generate_imsi(const std::string& operatorCode) {
    // 验证运营商代码的有效性，假设是三位数字，如460，46001等
    if (operatorCode.length() < 3 || operatorCode.length() > 5) {
        return "";  // 返回空字符串表示无效输入
    }

    // 使用高质量随机数生成器
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 9);

    // IMSI 从运营商代码开始
    std::stringstream imsiStream;
    imsiStream << operatorCode;

    // 生成9位用户标识号
    for (int i = 0; i < 9; ++i) {
        imsiStream << dis(gen);  // 生成一个 0-9 之间的随机数
    }

    return imsiStream.str();
}


int generate_gsm_rssi() {
    srand(time(NULL));

    int signal_strength = rand() % 100;  // 随机生成一个信号强度（0-99）

    int rssi = 0;
    if (signal_strength < 20) {
        // 强信号：-50 到 -70 dBm
        rssi = rand() % 21 - 50;  // 随机生成 -50 到 -70 dBm
    } else if (signal_strength < 50) {
        // 中等信号：-70 到 -90 dBm
        rssi = rand() % 21 - 70;  // 随机生成 -70 到 -90 dBm
    } else if (signal_strength < 80) {
        // 弱信号：-90 到 -100 dBm
        rssi = rand() % 11 - 90;  // 随机生成 -90 到 -100 dBm
    } else {
        // 极弱信号：-100 到 -110 dBm
        rssi = rand() % 11 - 100;  // 随机生成 -100 到 -110 dBm
    }

    return rssi;
}

std::string generate_random_esn() {
    srand(time(NULL));  // 使用当前时间作为种子

    std::stringstream esn;
    
    // 生成 8 或 16 个随机十六进制字符
    for (int i = 0; i < 16; ++i) {
        // 随机选择一个字符，可以是 '0'-'9' 或 'A'-'F'
        int random_char = rand() % 16;  // 生成一个 0 到 15 的数字
        esn << std::hex << std::uppercase << random_char;  // 转换为十六进制并以大写字母显示
    }

    return esn.str();
}

std::string generate_random_meid(const std::string& operator_code) {
    srand(time(NULL));
    std::string operator_prefix = operator_code.substr(0, 6);

    std::stringstream meid;
    
    meid << operator_prefix;

    // 生成剩余的 8 位十六进制数（4 个字节），总共 14 位
    for (int i = 0; i < 8; ++i) {
        int random_char = rand() % 16;  // 生成一个 0 到 15 的数字
        meid << std::hex << std::uppercase << random_char;  // 转换为十六进制并以大写字母显示
    }

    return meid.str();
}

// 定义运营商的号码段（前缀）
std::unordered_map<std::string, std::vector<std::string>> operator_prefixes = {
    {"46001", {"130", "131", "132", "133", "134", "135", "136", "137", "138", "139"}}, // 中国联通
    {"46002", {"150", "151", "152", "153", "155", "156", "157", "158", "159"}},        // 中国移动
    {"46003", {"180", "181", "182", "183", "184", "185", "186", "187", "188", "189"}},  // 中国电信
    {"310260", {"310", "311", "312", "313", "314"}},  // T-Mobile US
    {"310150", {"320", "321", "322", "323"}},        // Verizon
    {"40410", {"404", "405", "406", "407"}},         // Airtel India
    // 可以继续扩展其他运营商的前缀
};

// 生成 MSISDN 函数，传入 operator_code
std::string generate_random_msisdn(const std::string& operator_code) {
    srand(time(NULL));  // 使用当前时间作为随机种子

    // 查找对应运营商的号码段前缀
    auto it = operator_prefixes.find(operator_code);
    if (it == operator_prefixes.end()) {
        // 如果没有找到对应的运营商代号，返回空字符串或一个错误信息
        return "";
    }

    // 获取该运营商的前缀
    std::vector<std::string> prefixes = it->second;

    // 随机选择一个前缀
    std::string prefix = prefixes[rand() % prefixes.size()];

    // 生成 MSISDN（手机号）并拼接上 8 位数字
    std::string msisdn = prefix;
    for (int i = 0; i < 8; i++) {
        msisdn += std::to_string(rand() % 10);  // 随机生成一个数字并拼接
    }

    return msisdn;
}

struct BaseStationInfo {
    int lac;  // Location Area Code
    int cid;  // Cell ID
};

BaseStationInfo generate_random_base_station_info() {
    srand(time(NULL)); 

    BaseStationInfo baseStation;

    // 生成随机 LAC（范围 0 到 65535）
    baseStation.lac = rand() % 65536;

    // 生成随机 CID（范围 0 到 65535）
    baseStation.cid = rand() % 65536;

    return baseStation;
}

// 获取指定长度的随机数字字符串
std::string generate_random_digits(int length) {
    std::string result;
    for (int i = 0; i < length; i++) {
        result += std::to_string(rand() % 10);
    }
    return result;
}

// 生成 ICCID
std::string generate_iccid(const std::string& mccmnc) {
    // 验证 MCCMNC 的格式是否正确
    if (mccmnc.size() < 5) {
        std::cerr << "Invalid MCCMNC format!" << std::endl;
        return "";
    }

    // 获取 MCC 和 MNC
    std::string mcc = mccmnc.substr(0, 3); // MCC 为前三位
    std::string mnc = mccmnc.substr(3, 2); // MNC 为接下来的两位

    // 如果 MNC 长度为 3 则根据需要调整
    if (mnc[1] == '0') {
        mnc = mccmnc.substr(3, 3); // 如果 MNC 有三位，取三位
    }

    // 随机生成 10 至 13 位 SIM 卡序列号
    int sim_serial_length = rand() % 4 + 10;  // 随机选择 10 到 13 位
    std::string sim_serial = generate_random_digits(sim_serial_length);

    // 生成完整的 ICCID
    std::string iccid = mcc + mnc + sim_serial;

    return iccid;
}

std::string generate_country_iso() {
    // 国家及其对应的ISO 3166-1 alpha-2国家缩写
    std::vector<std::pair<std::string, std::string>> country_iso = {
        {"中国", "CN"},
        // {"美国", "US"},
        // {"英国", "GB"},
        // {"法国", "FR"},
        // {"德国", "DE"},
        // {"日本", "JP"},
        // {"印度", "IN"},
        // {"澳大利亚", "AU"},
        // {"巴西", "BR"},
        // {"加拿大", "CA"},
        // {"俄罗斯", "RU"},
        // {"意大利", "IT"},
        // {"西班牙", "ES"},
        // {"墨西哥", "MX"},
        // {"南非", "ZA"}
        // 可以继续添加更多国家
    };

    // 随机选择一个国家缩写
    size_t index = rand() % country_iso.size();
    return country_iso[index].second;
}

// 随机生成运营商代号（MCC + MNC）
std::string generate_operator_code() {
    // 存储运营商代号（MCC + MNC）
    std::vector<std::string> operator_codes = {
        "46001", // 中国联通
        "46002", // 中国移动
        "46003", // 中国电信
        // "310260", // 美国 T-Mobile
        // "310260", // 美国 AT&T
        // "40410", // 印度 Airtel
        // "40411", // 印度 Vodafone
        // "310150", // 美国 Verizon
        // "20404", // 英国 Vodafone
        // "26201", // 德国 T-Mobile
        // "25001", // 法国 Orange
        // "310030", // 加拿大 Bell
        // "23430", // 英国 O2
        // "310120", // 美国 Sprint
        // "310240"  // 美国 U.S. Cellular
    };

    // 随机选择一个运营商代号
    size_t index = rand() % operator_codes.size();
    return operator_codes[index];
}

struct OperatorInfo {
    std::string code;      // 运营商代号
    std::string name;      // 运营商名称
    std::string shortname; // 运营商简称
    std::string spn;       // SPN（Service Provider Name）
    std::string country_iso; // 国家ISO代码
};

// 函数：根据运营商代号（如46001）返回运营商信息
OperatorInfo get_operator_info(const std::string& operator_code) {
    // 存储运营商代号与信息的映射
    std::unordered_map<std::string, OperatorInfo> operator_map = {
        {"46001", {"46001", "中国联通", "CUCC", "China Unicom", "CN"}},
        {"46002", {"46002", "中国移动", "CMCC", "China Mobile", "CN"}},
        {"46003", {"46003", "中国电信", "CTCC", "China Telecom", "CN"}},
        {"310260", {"310260", "T-Mobile US", "TMO", "T-Mobile", "US"}},
        {"310150", {"310150", "Verizon", "VER", "Verizon Wireless", "US"}},
        {"40410", {"40410", "Airtel India", "Airtel", "Airtel India", "IN"}},
        {"40411", {"40411", "Vodafone India", "Voda", "Vodafone India", "IN"}},
        {"310120", {"310120", "Sprint", "SPR", "Sprint", "US"}},
        {"20404", {"20404", "Vodafone UK", "VodaUK", "Vodafone UK", "GB"}},
        {"26201", {"26201", "T-Mobile Germany", "TMO-DE", "T-Mobile Germany", "DE"}},
        {"25001", {"25001", "Orange France", "Orange", "Orange France", "FR"}},
        {"310030", {"310030", "Bell Canada", "Bell", "Bell Canada", "CA"}},
        {"23430", {"23430", "O2 UK", "O2UK", "O2 UK", "GB"}},
        {"310240", {"310240", "U.S. Cellular", "USC", "U.S. Cellular", "US"}}
    };

    // 查找运营商代号并返回相应的运营商信息
    auto it = operator_map.find(operator_code);
    if (it != operator_map.end()) {
        return it->second;  // 返回找到的运营商信息
    } else {
        // 如果没有找到匹配的代号，返回中国联通
        return {"46001", "中国联通", "CUCC", "China Unicom", "CN"};
    }
}

int generate_network_type() {
    srand(time(NULL));

    std::vector<int> network_types = {3, 13, 20};  // UMTS, LTE, 5G (NR),

    int network_type = network_types[rand() % network_types.size()];

    return network_type;
}

bool generate_sim_info() {
    srand(time(NULL));

    std::string operator_codes = generate_operator_code();
    OperatorInfo operator_info = get_operator_info(operator_codes);
    std::string imeisv = std::to_string(10 + random() % 90);
    std::string esn = generate_random_esn();
    BaseStationInfo baseStation = generate_random_base_station_info();
    int rssi = generate_gsm_rssi();

    std::string msisdn = generate_random_msisdn(operator_codes);
    std::string imei = generate_imei_with_checkdigit(gDeviceInfo.manufacturer, gDeviceInfo.imei1);
    std::string meid = generate_random_meid(operator_codes);
    std::string iccid = generate_iccid(operator_codes);
    std::string imsi = generate_imsi(operator_codes);
    int network_type = generate_network_type();

    gif_config("sim.shortname", operator_info.shortname);
    gif_config("sim.deviceid", imei);
    gif_config("sim.imei", imei);
    gif_config("sim.meid", meid);
    gif_config("sim.imsi", imsi);
    gif_config("sim.imeisv", imeisv);
    gif_config("sim.iccid", iccid);
    gif_config("sim.operator_name", operator_info.name);
    gif_config("sim.operator_code", operator_info.code);		
    gif_config("sim.sim_code", operator_info.code);
    gif_config("sim.country_iso", operator_info.country_iso);
    gif_config("sim.sim_iso", operator_info.country_iso);
    gif_config("sim.hasicccard", "1");
    gif_config("sim.state", "5");
    gif_config("sim.datatype", std::to_string(network_type));
    gif_config("sim.sn", iccid);
    gif_config("sim.phonenumber", msisdn);
    gif_config("sim.sid", "100");
    gif_config("sim.signalStrength", std::to_string(rssi));
    gif_config("sim.lac", std::to_string(baseStation.lac));
    gif_config("sim.cid", std::to_string(baseStation.cid));
    gif_config("sim.spn", operator_info.spn);
    gif_config("sim.msisdn", msisdn);
    gif_config("sim.esn", esn);
    gif_config("sim.phonetype", std::to_string(network_type));
    gif_config("sim.roaming", "0");
    return true;
}

//////////////////////////////////////////////////////////////////////
//wifi begin:
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


int generate_frequency() {
    // 随机选择 2.4 GHz 或 5 GHz
    return (rand() % 2 == 0) ? 2400 : 5000;  // 2400 MHz (2.4 GHz), 5000 MHz (5 GHz)
}

int generate_dhcp_lease() {
    // 随机生成 DHCP 租赁时间，单位为秒（例如：3600秒 = 1小时）
    return (rand() % 86400) + 3600;  // 1小时到1天之间
}

std::string generate_dns_address() {
    // 定义常见的公共 DNS 和中国运营商 DNS
    std::vector<std::string> dns_pool = {
        "8.8.8.8", "8.8.4.4",         // Google DNS
        "1.1.1.1", "1.0.0.1",         // Cloudflare DNS
        "208.67.222.222", "208.67.220.220", // OpenDNS
        "9.9.9.9",                    // Quad9 DNS
        "114.114.114.114", "114.114.115.115", // 中国电信
        "218.201.96.130",             // 中国移动
        "202.96.128.86",              // 中国联通
        "192.168.1.1",                // 局域网网关
        "192.168.0.1"                 // 局域网网关
    };

    // 从 DNS 池中随机选择一个地址
    return dns_pool[rand() % dns_pool.size()];
}

// 根据传入的 IP 地址推算 DHCP 服务器地址
std::string generate_dhcp_server(const std::string& ip_addr) {
    // 将 IP 地址按 '.' 分割成 4 个部分
    std::vector<int> octets;
    std::stringstream ss(ip_addr);
    std::string segment;
    while (std::getline(ss, segment, '.')) {
        octets.push_back(std::stoi(segment));
    }

    // 如果 IP 地址不合法，返回空字符串
    if (octets.size() != 4) {
        return "";
    }

    // 推算 DHCP 服务器地址
    if (octets[0] == 192 && octets[1] == 168) {
        return "192.168." + std::to_string(octets[2]) + ".1";
    } else if (octets[0] == 10) {
        return "10." + std::to_string(octets[1]) + "." + std::to_string(octets[2]) + ".1";
    } else if (octets[0] == 172 && octets[1] >= 16 && octets[1] <= 31) {
        return "172." + std::to_string(octets[1]) + "." + std::to_string(octets[2]) + ".1";
    } else {
        // 对于非私有地址，默认返回 .1
        return std::to_string(octets[0]) + "." + std::to_string(octets[1]) + ".1.1";
    }
}

void generate_wifi_info() {
    srand(time(NULL));

    std::string ssid = generate_ssid();
    std::string bssid = generate_bssid(ssid);
    std::string mac_addr = generate_mac_addr();
    std::string ip_addr = generate_ip_addr();
    int rssi = generate_rssi();
    int link_speed = generate_link_speed();
    std::string dhcp_gateway = generate_ip_addr();
    std::string dhcp_netmask = "255.255.255.0";
    std::string dhcp_dns1 = generate_dns_address();
    std::string dhcp_dns2 = generate_dns_address();
    std::string dhcp_server = generate_dhcp_server(ip_addr);
    int dhcp_lease = generate_dhcp_lease();
    int frequency = generate_frequency();

    gif_config("wifi.ssid", ssid);
    gif_config("wifi.bssid", bssid);
    gif_config("wifi.mac_addr", mac_addr);
    gif_config("wifi.ip_addr", ip_addr);
    gif_config("wifi.rssi", std::to_string(rssi));
    gif_config("wifi.linkspeed", std::to_string(link_speed));
    gif_config("wifi.enable", "1");

    gif_config("wifi.dhcp_gateway", dhcp_gateway);
    gif_config("wifi.dhcp_netmask", dhcp_netmask);
    gif_config("wifi.dhcp_dns1", dhcp_dns1);
    gif_config("wifi.dhcp_dns2", dhcp_dns2);
    gif_config("wifi.dhcp_server", dhcp_server);
    gif_config("wifi.dhcp_lease", std::to_string(dhcp_lease));
    gif_config("wifi.frequency", std::to_string(frequency));
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

void generate_bluetooth_info() {
    srand(static_cast<unsigned int>(time(0)));

    std::string name = generate_bluetooth_name();
    std::string mac_addr = generate_bt_mac(name);

    if (mac_addr.empty()) {
        std::cerr << "Failed to generate Bluetooth MAC Address." << std::endl;
        return;
    }

    gif_config("bt.name", name);
    gif_config("bt.mac", mac_addr);
    gif_config("bt.enable", "1");
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
    string destination_dir = "/data/local/tmp/.vpk/";

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
    
    restore_system_properties(work_dir);
    restore_pm_list_features(work_dir);
    restore_gpu_info(work_dir);
    if (!keepcache) delete_directory(work_dir);
    return true;
}

bool select_backup_and_restore(const string &brand, const string &model) {
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

    // 匹配格式：厂商=品牌=型号=版本=构建ID=是否加密.tar.gz
    regex device_regex(R"(([^=]+)=([^=]+)=([^=]+)=([^=]+)=([^=]+)=([^=]+)=([^=]+)\.tar\.gz)");
    smatch match;
    vector<string> matching_files;
    for (size_t i = 0; i < backup_files.size(); ++i) {
        string file_name = backup_files[i];

        if (regex_search(file_name, match, device_regex)) {
            string t_manufacturer = match[1];  // 厂商
            string t_brand = match[2];         // 品牌
            string t_model = match[3];         // 型号
            string t_version = match[4];       // 版本
            string t_build_id = match[5];      // 构建 ID
            string t_enc = match[6];           // enc

            if (t_brand == brand && t_model == model)  {
                matching_files.push_back(file_name);
            }
        } else {
            cerr << "Failed to parse backup file name: " << file_name << endl;
        }
    }

    if (matching_files.empty()) {
        cerr << "No matching backup files found for brand: " << brand << ", model: " << model << endl;
        return false;
    }

    // Randomly select one if multiple matches exist
    string selected_backup;
    if (matching_files.size() == 1) {
        selected_backup = matching_files[0];
    } else {
        srand(time(nullptr));
        int random_index = rand() % matching_files.size();
        selected_backup = matching_files[random_index];
        if (dbg) cout << "Multiple backups found. Randomly selected: " << selected_backup << endl;
    }

    if (dbg) cout << "Selected backup: " << selected_backup << endl;

    // Prepare destination directory
    string destination_dir = "/data/local/tmp/.vpk/";

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

    restore_system_properties(work_dir);
    restore_pm_list_features(work_dir);
    restore_gpu_info(work_dir);

    if (!keepcache) delete_directory(work_dir);
    return true;
}

void restore_main() {
    auto start_time = std::chrono::high_resolution_clock::now();
    if (gOpstions.withIndex) {
        if (dbg) cout << "Restoring backup with index: " << gOpstions.index << endl;
        if (gOpstions.index <= 0) {
            cerr << "Invalid backup index" << endl;
            return;
        }

        if (!select_backup_and_restore(gOpstions.index)) {
            cerr << "Failed to restore the selected backup" << endl;
            return;
        }
    } else if (gOpstions.withBrand && gOpstions.withModel) {
        if (dbg) cout << "Restoring backup with brand: " << gOpstions.brand << ", model: " << gOpstions.model << endl;

        if (!select_backup_and_restore(gOpstions.brand, gOpstions.model)) {
            cerr << "Failed to restore the selected backup based on brand and model" << endl;
            return;
        }
    } else {
        cerr << "No valid criteria provided for restore" << endl;
        return;
    }

    generate_boot_id();
    generate_device_info();
    generate_sim_info();
    generate_wifi_info();
    generate_bluetooth_info();
    generate_misc_info();
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    if (dbg) std::cout << "execution time: " << duration.count()/1000.0 << "s" << std::endl;
    cout << "Success" << endl;
}

int show_file(const string &work_dir, const string &filename) {
    ifstream prop_file(work_dir + "/" + filename);
    if (!prop_file.is_open()) {
        cerr << "Failed to open prop.pick file" << endl;
        return -1;
    }

    string line;
    while (getline(prop_file, line)) {
        line = line.substr(line.find_first_not_of(" \t\n\r"), line.find_last_not_of(" \t\n\r") + 1);
        if (line.empty()) {
            continue;
        }
        cout << line << endl;
    }

    return 0;
}

string select_backup_by_index(int index) {
    DIR *dir = opendir(WORK_DIR);
    if (dir == nullptr) {
        cerr << "Failed to open directory: " << WORK_DIR << endl;
        return "";
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

    // Sort the backup files to ensure consistent order
    sort(backup_files.begin(), backup_files.end());

    if (backup_files.empty()) {
        cerr << "No backup files found in " << WORK_DIR << endl;
        return "";
    }

    // Check if the index is valid
    if (index < 1 || index > backup_files.size()) {
        cerr << "Invalid backup index: " << index << endl;
        return "";
    }

    // Adjust to 0-based index
    int adjusted_index = index - 1;
    string selected_backup = backup_files[adjusted_index];
    if (dbg) cout << "Selected backup: " << selected_backup << endl;

    // Prepare destination directory
    string destination_dir = string(WORK_DIR) + ".vpk/";
    struct stat st = {0};
    if (stat(destination_dir.c_str(), &st) == -1) {
        if (errno == ENOENT) {
            if (dbg) cout << "Directory " << destination_dir << " does not exist. Creating it." << endl;
            if (mkdir(destination_dir.c_str(), 0777) != 0) {
                cerr << "Failed to create directory: " << destination_dir 
                     << ", errno: " << strerror(errno) << endl;
                return "";
            }
        } else {
            cerr << "Failed to check directory: " << destination_dir 
                 << ", errno: " << strerror(errno) << endl;
            return "";
        }
    }

    // Extract the selected tar.gz file
    string tar_file = WORK_DIR + selected_backup;
    if (!extract_tar(tar_file, destination_dir)) {
        cerr << "Failed to extract tar file: " << tar_file << endl;
        return "";
    }

    // Generate work directory name
    size_t pos = selected_backup.find(".tar.gz");
    if (pos == string::npos) {
        cerr << "Invalid backup file name: " << selected_backup << endl;
        return "";
    }
    string work_dir = destination_dir + selected_backup.substr(0, pos);

    if (dbg) cout << "Work directory: " << work_dir << endl;
    return work_dir;
}


int show_main() {
    string work_dir = select_backup_by_index(gOpstions.index);
    if (work_dir.empty()) {
        cerr << "Failed to select by index: " << gOpstions.index << endl;
        return -1;
    }
    int ret = 0;
    if (gOpstions.propOnly) {
        show_file(work_dir, "prop.pick");
        if (!keepcache) delete_directory(work_dir);
        return 0;
    }
    if (gOpstions.featureOnly) {
        show_file(work_dir, "pm_list_features");
        if (!keepcache) delete_directory(work_dir);
        return 0;
    }

    show_file(work_dir, "prop.pick");
    show_file(work_dir, "pm_list_features");
    if (!keepcache) delete_directory(work_dir);
    return ret;
}
/////////////////////////////////////////////////////////////////////
int encrypt_main() {
    encrypt_set_dbg(dbg);
    if (gOpstions.mode == "encrypt") {
        encrypt_gzip_file(gOpstions.input, gOpstions.output, gOpstions.key);
        std::cout << "File encrypted successfully: " << gOpstions.output << "\n";
    } else if (gOpstions.mode == "decrypt") {
        decrypt_gzip_file(gOpstions.input, gOpstions.output, gOpstions.key);
        std::cout << "File decrypted successfully: " << gOpstions.input << "\n";
    } else {
        std::cerr << "Invalid mode: " << gOpstions.mode << ". Use 'encrypt' or 'decrypt'.\n";
        return 1;
    }
    return 0;
}

/////////////////////////////////////////////////////////////////////
void print_help() {
    cout << "Usage: vpick <command> [options]\n";
    cout << "\nCommands:" << endl;
    cout << "  -v, --version         Show version information." << endl;
    cout << "  -b, backup            Create a backup." << endl;
    cout << "  -l, list              List available backups." << endl;
    if (dbg) cout << "  -r, restore           Restore from a backup." << endl;
    if (dbg) cout << "  -e, encrypt           Encrypt a file." << endl;
    if (dbg) cout << "  -d, decrypt           Decrypt a file." << endl;
    if (dbg) cout << "  -s, show              Show details of a backup." << endl;
    if (dbg) cout << "  getprop               Retrieve system properties." << endl;
    if (dbg) cout << "  -h, help              Show this help message." << endl;

    if (dbg) cout << "\nOptions:" << endl;
    if (dbg) cout << "  --index <index>       Specify the target backup index (1-based)." << endl;
    if (dbg) cout << "  --brand <brand>       Specify the target device brand." << endl;
    if (dbg) cout << "  --model <model>       Specify the target device model." << endl;
    if (dbg) cout << "  --prop-only           Only restore system properties." << endl;
    if (dbg) cout << "  --feature-only        Only restore system features." << endl;
    if (dbg) cout << "  --key <key>           Specify the encryption/decryption key." << endl;
    if (dbg) cout << "  -i, --input <file>    Specify the input file for encryption/decryption." << endl;
    if (dbg) cout << "  -o, --output <file>   Specify the output file for encryption/decryption." << endl;
    if (dbg) cout << "  --dbg, --debug        Enable debug mode to show detailed logs." << endl;
    if (dbg) cout << "  --kc, --keepcache     Keep cache files after restore." << endl;
    if (dbg) cout << "  --setprop-cmd <setprop-cmd>     indicate how to setprop" << endl;
    

    cout << "\nExamples:" << endl;
    cout << "  vpick -v" << endl;
    cout << "  vpick backup" << endl;
    cout << "  vpick list" << endl;
    if (dbg) cout << "  vpick restore --index 1 --brand Xiaomi --model Redmi" << endl;
    if (dbg) cout << "  vpick -r --dbg" << endl;
    if (dbg) cout << "  vpick encrypt -i file.txt -o file.txt.enc --key mykey" << endl;
    if (dbg) cout << "  vpick decrypt -i file.txt.enc -o file.txt --key mykey" << endl;
}


void process_options(int argc, char* argv[], int& i) {
    while (i < argc) {
        string option = argv[i];

        if (option == "--dbg" || option == "--debug") {
            dbg = true;
        } else if (option == "--kc" || option == "--keepcache") {
            keepcache = true;
        } else if (option == "-i" || option == "--input") {
            gOpstions.withInput = true;
            gOpstions.input = argv[++i];
        } else if (option == "--index") {
            if (i + 1 < argc) {
                gOpstions.withIndex = true;
                gOpstions.index = atoi(argv[++i]);
                if (dbg) cout << "Index set to: " << gOpstions.index << endl;
            } else {
                cerr << "Error: --brand requires a value." << endl;
                print_help();
                exit(1);
            }
        } else if (option == "-o" || option == "--output") {
            gOpstions.withOutput = true;
            gOpstions.output = argv[++i];
        } else if (option == "--key") {
            gOpstions.withkey = true;
            gOpstions.key = argv[++i];
        } else if (option == "-b" || option == "--brand") {
            if (i + 1 < argc) {
                gOpstions.withBrand = true;
                gOpstions.brand = argv[++i];
                if (dbg) cout << "Brand set to: " << gOpstions.brand << endl;
            } else {
                cerr << "Error: --brand requires a value." << endl;
                print_help();
                exit(1);
            }
        } else if (option == "-m" || option == "--model") {
            if (i + 1 < argc) {
                gOpstions.withModel = true;
                gOpstions.model = argv[++i];
                if (dbg) cout << "Model set to: " << gOpstions.model << endl;
            } else {
                cerr << "Error: --model requires a value." << endl;
                print_help();
                exit(1);
            }
        } else if (option == "--prop-only") {
            gOpstions.propOnly = true;
        } else if (option == "--feature-only") {
            gOpstions.featureOnly = true;
        } else if (option == "--setprop-cmd") {
            if (i + 1 < argc) {
                gOpstions.withSetpropCmd = true;
                gOpstions.setpropCmd = argv[++i];
            } else {
                cerr << "Error: --setprop-cmd requires a value." << endl;
                print_help();
                exit(1);
            }
        } else {
            cerr << "Unknown option: " << option << endl;
            print_help();
            exit(1);
        }
        i++;
    }
}

void handle_command(const string& cmd, int argc, char* argv[]) {
    if (cmd == "-v" || cmd == "version") {
        cout << VERSION << endl;
    } else if (cmd == "-b" || cmd == "backup") {
        int i = 2;
        process_options(argc, argv, i);
        backup_main();
    } else if (cmd == "-l" || cmd == "list") {
        int i = 2;
        process_options(argc, argv, i);
        list_main();
    } else if (cmd == "-h" || cmd == "help") {
        int i = 2;
        process_options(argc, argv, i);
        print_help();
    } else if (cmd == "-e" || cmd == "encrypt") {
        gOpstions.mode = "encrypt";
        int i = 2;
        process_options(argc, argv, i);
        encrypt_main();
    } else if (cmd == "-d" || cmd == "decrypt") {
        gOpstions.mode = "decrypt";
        int i = 2;
        process_options(argc, argv, i);
        encrypt_main();
    } else if (cmd == "-s" || cmd == "show") {
        if (argc < 3) {
            print_help();
            return;
        }
        gOpstions.withIndex = true;
        gOpstions.index = atoi(argv[2]);
        int i = 3;
        process_options(argc, argv, i);
        show_main();
    } else if (cmd == "getprop") {
        if (argc < 3) {
            print_help();
            return;
        }
        gOpstions.withIndex = true;
        gOpstions.index = atoi(argv[2]);
        gOpstions.propOnly = true;
        show_main();
    } else if (cmd == "-r" || cmd == "restore") {
        // if (argc < 3) {
        //     print_help();
        //     return;
        // }
        // gOpstions.withIndex = true;
        // gOpstions.index = atoi(argv[2]);
        int i = 2;
        process_options(argc, argv, i);
        restore_main();
    } else {
        cerr << "Unknown command: " << cmd << endl;
        print_help();
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_help();
        return 1;
    }

    string cmd = argv[1];
    handle_command(cmd, argc, argv);

    return 0;
}
