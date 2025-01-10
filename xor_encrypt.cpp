#include <iostream>
#include <fstream>
#include <vector>
#include <iterator>
#include "xor_encrypt.h"

static bool dbg  = false;

void encrypt_set_dbg(bool new_value) { dbg = new_value;}

// Function to encrypt/decrypt data using XOR
void xor_encrypt(std::vector<char>& data, const std::string& key) {
    if (key.empty()) {
        std::cerr << "Error: Key cannot be empty." << std::endl;
        return;
    }

    size_t key_len = key.length();
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] ^= key[i % key_len];
    }
}

// Function to encrypt a gzip file using XOR
int encrypt_gzip_file(const std::string& input_file, const std::string& output_file, const std::string& key) {
    if (dbg) std::cout << "encrypt_gzip_file(" << input_file << ", " << output_file << ")" << std::endl;

    // Check if the key is valid
    if (key.empty()) {
        std::cerr << "Error: Key cannot be empty." << std::endl;
        return 1;
    }

    // Open input file in binary mode
    std::ifstream input(input_file, std::ios::binary);
    if (!input.is_open()) {
        std::cerr << "Failed to open input file: " << input_file << std::endl;
        return 1;
    }

    // Read entire file content into buffer
    std::vector<char> buffer((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    input.close();

    if (buffer.empty()) {
        std::cerr << "Error: Input file is empty: " << input_file << std::endl;
        return 1;
    }

    // Encrypt the data
    xor_encrypt(buffer, key);

    // Open output file in binary mode
    std::ofstream output(output_file, std::ios::binary);
    if (!output.is_open()) {
        std::cerr << "Failed to open output file: " << output_file << std::endl;
        return 1;
    }

    // Write encrypted data to output file
    output.write(buffer.data(), buffer.size());
    if (output.fail()) {
        std::cerr << "Failed to write to output file: " << output_file << std::endl;
        return 1;
    }

    output.close();
    return 0;
}

// Function to decrypt a gzip file using XOR
int decrypt_gzip_file(const std::string& input_file, const std::string& output_file, const std::string& key) {
    if (dbg) std::cout << "decrypt_gzip_file(" << input_file << "," << output_file << ")" << std::endl;

    // Open input file in binary mode
    std::ifstream input(input_file, std::ios::binary);
    if (!input) {
        std::cerr << "Failed to open input file: " + input_file << std::endl;
        return 1;
    }

    // Read entire file content into buffer
    std::vector<char> buffer((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    input.close();

    // Decrypt the data (XOR operation is symmetric)
    xor_encrypt(buffer, key);

    // Write the decrypted data to the output file
    std::ofstream output(output_file, std::ios::binary);
    if (!output) {
        std::cerr << "Failed to open output file: " + output_file << std::endl;
        return 1;
    }

    output.write(buffer.data(), buffer.size());
    output.close();
    return 0;
}
