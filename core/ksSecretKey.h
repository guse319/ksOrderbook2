#include <fstream>
#include <string>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include "ksTypes.h"

class SecretKey {
public:
    SecretKey(const std::string& filepath);

    std::string sign(const char* prehash);

private:
    Key key;
};