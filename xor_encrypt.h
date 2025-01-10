#ifndef __ENCRYPT_H

void encrypt_set_dbg(bool new_value);
int encrypt_gzip_file(const std::string& input_file, const std::string& output_file, const std::string& key);
int decrypt_gzip_file(const std::string& input_file, const std::string& output_file, const std::string& key);

#endif