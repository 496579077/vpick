#include <iostream>
#include <fstream>
#include <vector>
#include <iterator>
#include "xor_encrypt.h"

// 使用 XOR 加密/解密数据的函数
void xor_encrypt(std::vector<char>& data, const std::string& key) {
    size_t key_len = key.length();
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] ^= key[i % key_len];
    }
}

// 使用 XOR 加密 gzip 文件的函数
int encrypt_gzip_file(const std::string& input_file, const std::string& output_file, const std::string& key) {
    // 以二进制模式打开输入文件
    std::ifstream input(input_file, std::ios::binary);
    if (!input) {
        std::cerr << "无法打开输入文件: " + input_file << std::endl;
        return 1;  // 返回错误码
    }

    // 将整个文件内容读入缓冲区
    std::vector<char> buffer((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    input.close();

    // 对数据进行加密
    xor_encrypt(buffer, key);

    // 将加密后的数据写入输出文件
    std::ofstream output(output_file, std::ios::binary);
    if (!output) {
        std::cerr << "无法打开输出文件: " + output_file << std::endl;
        return 1;  // 返回错误码
    }

    output.write(buffer.data(), buffer.size());
    output.close();

    return 0;  // 成功
}


// 使用 XOR 解密 gzip 文件的函数
int decrypt_gzip_file(const std::string& input_file, const std::string& output_file, const std::string& key) {
    // 以二进制模式打开输入文件
    std::ifstream input(input_file, std::ios::binary);
    if (!input) {
        std::cerr << "无法打开输入文件: " + input_file << std::endl;
        return 1;  // 返回错误码
    }

    // 将整个文件内容读入缓冲区
    std::vector<char> buffer((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    input.close();

    // 对数据进行解密（XOR 操作与加密相同）
    xor_encrypt(buffer, key);

    // 将解密后的数据写入输出文件
    std::ofstream output(output_file, std::ios::binary);
    if (!output) {
        std::cerr << "无法打开输出文件: " + output_file << std::endl;
        return 1;  // 返回错误码
    }

    output.write(buffer.data(), buffer.size());
    output.close();
    return 0;  // 成功
}