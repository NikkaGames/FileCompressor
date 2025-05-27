#include <openssl/aes.h>
#include <fstream>
#include <vector>
#include <cstring>
#include <iostream>
#include <elf.h>
#include <lzma.h>

bool compress_lzma(const std::vector<char>& input_data, std::vector<char>& output_data) {
    lzma_stream strm = LZMA_STREAM_INIT;
    lzma_ret ret = lzma_easy_encoder(&strm, 5, LZMA_CHECK_CRC64);  // compression level 5, CRC64 checksum
    if (ret != LZMA_OK) {
        std::cerr << "Error initializing encoder: " << ret << std::endl;
        return false;
    }
    strm.next_in = reinterpret_cast<const uint8_t*>(input_data.data());
    strm.avail_in = input_data.size();
    output_data.resize(input_data.size() * 2);  // reserve space for output
    strm.next_out = reinterpret_cast<uint8_t*>(output_data.data());
    strm.avail_out = output_data.size();
    ret = lzma_code(&strm, LZMA_FINISH);
    if (ret != LZMA_STREAM_END) {
        std::cerr << "Error during compression: " << ret << std::endl;
        return false;
    }
    output_data.resize(strm.total_out);
    lzma_end(&strm);
    return true;
}

void xor_cipher(std::vector<char>& data, const std::string& key, bool mode) {
    uint32_t key1 = 0x1EFF2FE1, key2 = 0x1E00A2E3;
    for (char c : key) {
        key1 = (key1 * 33) ^ static_cast<uint8_t>(c);
        key2 = (key2 * 31) + static_cast<uint8_t>(c);
    }
    for (size_t i = 0; i < data.size(); ++i) {
        if (mode) { // Encrypt
            data[i] = (data[i] << 3) | (data[i] >> 5);
            data[i] ^= static_cast<uint8_t>(key1 >> (i % 32));
            data[i] = (data[i] >> 2) | (data[i] << 6);
            data[i] ^= static_cast<uint8_t>(key2 >> ((i + 5) % 32));
        } else { // Decrypt
            data[i] ^= static_cast<uint8_t>(key2 >> ((i + 5) % 32));
            data[i] = (data[i] << 2) | (data[i] >> 6);
            data[i] ^= static_cast<uint8_t>(key1 >> (i % 32));
            data[i] = (data[i] >> 3) | (data[i] << 5);
        }
    }
}

bool encrypt_elf(const std::string& input_file, const std::string& output_file) {
    std::ifstream in(input_file, std::ios::binary);
    if (!in) {
        std::cerr << "Error opening input file.\n";
        return false;
    }
    std::vector<char> old_elf_data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    std::vector<char> elf_data;
    compress_lzma(old_elf_data, elf_data);
    xor_cipher(elf_data, "System.Reflection", true);
    std::ofstream out(output_file, std::ios::binary);
    if (!out) {
        std::cerr << "Error opening output file.\n";
        return false;
    }
    out.write(elf_data.data(), elf_data.size());
    out.close();
    std::cout << "ELF encrypted and saved to " << output_file << std::endl;
    return true;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input_file> <output_file>\n";
        return 1;
    }
    std::string input_file = argv[1];
    std::string output_file = argv[2];
    if (encrypt_elf(input_file, output_file)) {
        std::cout << "Encryption completed successfully!" << std::endl;
    } else {
        std::cerr << "Encryption failed." << std::endl;
    }
    return 0;
}
