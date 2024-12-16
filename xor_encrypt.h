#ifndef __ENCRYPT_H

int encrypt_gzip_file(const std::string& input_file, const std::string& output_file, const std::string& key);
int decrypt_gzip_file(const std::string& input_file, const std::string& output_file, const std::string& key);

#endif