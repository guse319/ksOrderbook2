#include "ksSecretKey.h"
#include <cstring>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/err.h>

SecretKey::SecretKey(const std::string& filepath) {
    std::ifstream secret_file(filepath);
    std::string secret((std::istreambuf_iterator<char>(secret_file)),
                   std::istreambuf_iterator<char>());
    secret.erase(secret.find_last_not_of(" \n\r\t") + 1); // trim whitespace
    key = new char[secret.length() + 1];
    strcpy(key, secret.c_str());
    secret_file.close();
}

std::string SecretKey::sign(const char* prehash) {
    // Create a BIO to read the PEM-formatted key
    BIO* bio = BIO_new_mem_buf(key, strlen(key));
    
    if (!bio) {
        throw std::runtime_error("Failed to create BIO");
    }
    
    // Read the RSA private key from the BIO
    EVP_PKEY* pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    
    if (!pkey) {
        unsigned long err = ERR_get_error();
        std::string err_msg = ERR_error_string(err, nullptr);
        throw std::runtime_error("Failed to read private key: " + err_msg);
    }
    
    // Create a signing context
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        EVP_PKEY_free(pkey);
        throw std::runtime_error("Failed to create EVP_MD_CTX");
    }
    
    // First, get the maximum signature size
    size_t signature_len = EVP_PKEY_size(pkey);
    unsigned char* signature = new unsigned char[signature_len];
    
    // Initialize signing with SHA256
    if (EVP_DigestSignInit(mdctx, nullptr, EVP_sha256(), nullptr, pkey) <= 0) {
        unsigned long err = ERR_get_error();
        std::string err_msg = ERR_error_string(err, nullptr);
        delete[] signature;
        EVP_MD_CTX_free(mdctx);
        EVP_PKEY_free(pkey);
        throw std::runtime_error("EVP_DigestSignInit failed: " + err_msg);
    }

    EVP_PKEY_CTX* pctx = EVP_MD_CTX_get_pkey_ctx(mdctx);
    if (!pctx) throw std::runtime_error("No pkey ctx");

    // RSA-PSS padding
    if (EVP_PKEY_CTX_set_rsa_padding(pctx, RSA_PKCS1_PSS_PADDING) <= 0)
        throw std::runtime_error("set_rsa_padding(PSS) failed");

    // saltlen = hashlen is common for PSS
    if (EVP_PKEY_CTX_set_rsa_pss_saltlen(pctx, -1) <= 0)
        throw std::runtime_error("set_rsa_pss_saltlen failed");

    
    // Sign the data
    if (EVP_DigestSign(mdctx, signature, &signature_len, 
                       (unsigned char*)prehash, strlen(prehash)) <= 0) {
        unsigned long err = ERR_get_error();
        std::string err_msg = ERR_error_string(err, nullptr);
        delete[] signature;
        EVP_MD_CTX_free(mdctx);
        EVP_PKEY_free(pkey);
        throw std::runtime_error("EVP_DigestSign failed: " + err_msg);
    }
    
    // Encode signature as base64
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO* bmem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bmem);
    
    BIO_write(b64, signature, signature_len);
    BIO_flush(b64);
    
    BUF_MEM* buffer_ptr = nullptr;
    BIO_get_mem_ptr(b64, &buffer_ptr);
    
    std::string signature_b64(buffer_ptr->data, buffer_ptr->length);
    
    BIO_free_all(b64);
    delete[] signature;
    EVP_MD_CTX_free(mdctx);
    EVP_PKEY_free(pkey);
    
    return signature_b64;
}